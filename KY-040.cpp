#include <string>
#include <iostream>
#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstring>

#include <gpiod.h>

#include "KY-040.hpp" // Common structs and defines. Look here for error and return codes

using std::cerr;
using std::endl;
using std::strerror;

int GPIOLineFree(struct gpiod_chip *chip, struct stGPIOline *line, int userfeedback)
{

    /*
        GPIOLineFree() - This checks the info about the line to determine
                         if the line is free.
                         If userfeedback is YESPLEASE then print the info about the line
                         
    */

    int GPIOnumber;
    const char *name, *consumer, *dir;
    struct gpiod_line_info *info;
    static bool bheader = true;
    unsigned int offset;

    // Read the info about the line to determine if you can use it
    info = gpiod_chip_get_line_info(chip, line->number);

    if (!info)
    {
        fprintf(stderr, "failed to read info: %s\n", strerror(errno));
        return ERRREADINFO;
    };

    name = gpiod_line_info_get_name(info);
    if (!name)
        name = "unnamed";

    consumer = gpiod_line_info_get_consumer(info);
    if (!consumer) // if the GPIO consumer is NULL then it is free
        consumer = "unused";
    else if (strcmp(consumer, CONSUMER) == 0)
        // This GPIO line has been previously claimed by an instance of this program
        return ERRKY040OWNED;
    else
        // This resource is claimed by another program
        return ERRGPIOINUSE;

    dir = (gpiod_line_info_get_direction(info) ==
           GPIOD_LINE_DIRECTION_INPUT)
              ? "input"
              : "output";

    // Copy the name found from gpiod_line_info_get_name(info)
    // to the line structure
    strcpy(line->name, name);

    if(bheader) {
        fprintf(stderr, "Header Pin num:  name        claimed     direction   state     offset    debounced\n");
        bheader = false;
    };
    fprintf(stderr, "     %3d: %12s %12s   %8s   %10s      %d      %9s\n",
            gpiod_line_info_get_offset(info), name, consumer, dir,
            gpiod_line_info_is_active_low(info) ? "active-low" : "active-high",
            gpiod_line_info_get_offset(info),
            gpiod_line_info_is_debounced(info) ? "debounced" : "otherwise");


    // Now check that the line is "unused" and "unnamed"
    // One assumes that the desired direction will be requested elsewhere
    if (strcmp(consumer, "unused") == 0) {
        return OKGPIOLINEFREE;
    };

    return ERRLINENOTFREE;

}; // End GPIOLineInfo()

int InitGPIOActivity(struct stGPIOActivity *act)
{

    act->lset = gpiod_line_settings_new();
    if (!act->lset)
        return ERRLINESETTINGS;

    act->lcfg = gpiod_line_config_new();
    if (!act->lcfg)
        return ERRLINECONFIG;

    act->rcfg = gpiod_request_config_new();
    if (!act->rcfg)
        return ERRREQUESTCONFIG;

    return OKINITGPIOACTION;

}; // End InitGPIOActivity()

void FreeGPIOActivity(struct stGPIOActivity *act)
{

    if (act->lcfg)
        gpiod_line_config_free(act->lcfg);

    if (act->lset)
        gpiod_line_settings_free(act->lset);

    if (act->lreq)
        gpiod_line_request_release(act->lreq);

    if (act->rcfg)
        gpiod_request_config_free(act->rcfg);

}; // End FreeGPIOActivity()

/*******************************************************************
 * 
 * A note about KY-040 type rotary encoder switches 
 *  
 * This is all necessary because of the asychronous nature of
 * human knob twiddling, how noisy the KY-040 seems to be and 
 * watching for just levels of the CLK and DT line are unpredictable
 * So we have to resort to watching the edge transitions from 
 * the rotary encoder. After a series of experiments the edges 
 * seem to work relatively predictably. There are indeed other 
 * ways to read the encoder but this project uses libgpoid 2.1.2
 * Also please note that multiple edge events are ignore as 
 * best as possible and that the CLK and DT lines have been 
 * debounced
 * 
 * There are 4 unique level sequences 
 * that are observed twisting a rotary encoder in one direction. 
 * With this in mind and confirmed by a little noodling
 * and debugging we find the corresponding edge sequences
 * 
 *          Twisting CW                 Twisting CCW
 *      CLKR, DTR , CLKF, DTF       CLKR, DTF , CLKF, DTR
 *      DTR , CLKF, DTF , CLKR      DTF , CLKF, DTR , CLKR
 *      CLKF, DTF , CLKR, DTR       CLKF, DTR , CLKR, DTF
 *      DTF , CLKR, DTR , CLKF      DTR , CLKR, DTF , CLKF
 * 
 *      Where
 *         CLKR = Clock Rising
 *         DTR  = Data Rising
 *         CLKF = Clock Falling
 *         DTF  = Data Falling
 * 
 *  Unfortunately when looking a series of edges events there is no way 
 *  to know what the level of the opposing signals are simultaniouslly
 *  as far as I understand libgpiod. So we are left with coding the 
 *  states of the edges which is different than codeing the concurrent 
 *  levels of CLK and DT per sample. 
 *  There are single edge states that can be observed for any event edge 
 *  sample requesting CLK and DT activity. 
 *  The following code is used to determine the direction of rotation
 *  knowing only the squence of four edge events.
 *  We will code the first digit of a two bit binary number by using 
 *  source of the edge event number as the first digit (MSB) and the 
 *  direction of the edge event as the second digit (LSB). 
 *  We will choose a CLK edge event source as 1 and a DT edge event source as as 0. 
 *  For direction well define an rising edge event as 1 and 
 *  the falling edge event as 0. So if CLK is read as a rising edge event then 
 *  we choose 11 to represent that edge. The leading 1 (MSB) is for CLK and the 
 *  trailing (LSB) is 1 for a rising edge event. if we see a DT rising event then 
 *  we code that event as 01. There are only 4 possible codes seen below
 * 
 *      CLKR is 11 (3)
 *      CLKR is 10 (2)
 *      DTR  is 01 (1)
 *      DTF  is 00 (0)
 * 
 *  For each direction of rotation there are only 4 possible valid sequences
 *  of edge events, please see a diagram of a typical quadrature rotary 
 *  encoder (1). Since the encoder can be turned clockwise (CW) and counter 
 *  clockwise (CCW) and there are 4 valid sequences for any sequential sampled 
 *  set of edges for one direction of rotation there are 8 total valid edge 
 *  sequences for both CW and CCW rotation. Clearly we are assuming that the 
 *  edge sequences represent 4 presumablty contiguous events. This is why 
 *  looking at the edges is easiser to use than levels becasue the rate at 
 *  which humans twist knobs is unpredictacle and inconsistant. Since each 
 *  edege event sample represents a sequance we have to also code for time
 *  The easist way to do is to weight each time in the sequence. For this 
 *  effort weights (or multipliers) of increasing powers of two were chosen 
 *  for simplicty. Also the simplest thing to do is increase the weights with 
 *  time. For the four time intervals 
 *  there are four weights:
 *   
 *      t=0 multipiled by 2^0 
 *      t=1 multipiled by 2^1
 *      t=2 multipiled by 2^2
 *      t=3 multipiled by 2^3
 * 
 *  So putting the edge event description coding, the source of the edge and the 
 *  direction of the edge, and the weights we can find a number associated with 
 *  any of the eight valid edge event sequences. In this case we find that the 
 *  resultant number that comes from this process is unique amongst the  
 *  4^4 (256) possible 4 edge event sequences. 
 * 
 *  Below are then the coded sequences that we can use to detect KY-040 rotation 
 *  either in the CW or CCW directions 
 * 
 *  CW
 *  CLKR, DTR , CLKF, DTF   -> 3 + 2*1 + 4*2 + 8*0 = 13
 *  DTR , CLKF, DTF , CLKR  -> 1 + 2*0 + 4*0 + 8*3 = 29
 *  CLKF, DTF , CLKR, DTR   -> 2 + 2*3 + 4*3 + 8*1 = 22
 *  DTF , CLKR, DTR , CLKF  -> 0 + 2*1 + 4*1 + 8*2 = 26
 * 
 *  CCW
 *  CLKR, DTF , CLKF, DTR   -> 3 + 2*0 + 4*2 + 8*1 = 19
 *  DTF , CLKF, DTR , CLKR  -> 0 + 2*2 + 4*1 + 8*3 = 32
 *  CLKF, DTR , CLKR, DTF   -> 2 + 2*1 + 4*3 + 8*0 = 16 
 *  DTR , CLKR, DTF , CLKF  -> 1 + 2*3 + 4*0 + 8*2 = 23
 * 
 *  Note:
 *     Please refer to the excellent web page on the rotary encoder
 *     which greatly helped inform this method
 *     (1) https://www.best-microcontroller-projects.com/rotary-encoder.html
 *     Vellemann WP1435 was used to for this code 
 * 
 ********************************************************************/
int main(int argc, char **argv) {    
 
    int i, j, k; // indecies
    int rtn; // Return value
    int userfeedback = PRIMARY;
    int state = NOTHINGISHAPPENING;
    int direction;
    int select = NONE;
    int tune;

    struct gpiod_line_info *info;
    struct gpiod_line_request *request;
    enum gpiod_line_value *value;
    gpiod_chip *chip;
    
    stGPIOline TunerDT, TunerCLK, TunerSW;
    stGPIOline *GPIOLines[NINTUNE] = {&TunerDT, &TunerCLK, &TunerSW};
    unsigned int GPIOInTuneLines[NINTUNE] = {NTUNERDT, NTUNERCLK, NTUNERSW};
    enum gpiod_line_value GPIOInTuneValues[NINTUNE];
    enum gpiod_line_value lastGPIOInTuneValues[NINTUNE];
    stGPIOActivity intune;

    size_t edgeBufSize;
    struct gpiod_edge_event_buffer *eventBuf;
    struct gpiod_edge_event *edgeEvent;
    size_t maxEdgeEvents;
    unsigned int edgeOS;
    enum gpiod_edge_event_type edgeType;
    size_t readEvents;
    unsigned long idx;
    
    // Set the input Tuner GPIO line numbers
    TunerDT.number = NTUNERDT;
    TunerCLK.number = NTUNERCLK;
    TunerSW.number = NTUNERSW;

    // Open the GPIO chip
    chip = gpiod_chip_open(GPIOCHIP);
    if (!chip) {
        fprintf(stderr, "KY-040: Chip %s Open Failure\n", GPIOCHIP);
        return rtn;
    };

    for (i = 0; i < NINTUNE; i++) {
        // Is the GPIO line free?
        rtn = GPIOLineFree(chip, GPIOLines[i], userfeedback);

        if (rtn != OKGPIOLINEFREE) {
            fprintf(stderr, "KY-040: Line %d Not Free (return %d)\n", GPIOLines[i]->number, rtn);
            // Close chip and exit
            gpiod_chip_close(chip);
            return rtn;
        }
    };

    if (userfeedback == PRIMARY)
        fprintf(stderr, "KY-040: All GPIO lines are Free\n");

    rtn = InitGPIOActivity(&intune);
    if (rtn != OKINITGPIOACTION) {
        fprintf(stderr, "KY-040: Error Initializing GPIO Tuner Input Activity\n");
        // Clean up
        FreeGPIOActivity(&intune);
        gpiod_chip_close(chip);
        return rtn;
    };

    // Line settings for the rotary encoder
    gpiod_line_settings_set_direction(intune.lset, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(intune.lset, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_debounce_period_us(intune.lset, 3000);
    
    // Pull up resistors are on the vellemann WP1435 rotary encoder board
    gpiod_line_settings_set_bias(intune.lset, GPIOD_LINE_BIAS_DISABLED);
    // Add all the GPIO input lines to the input config
    gpiod_line_config_add_line_settings(intune.lcfg, GPIOInTuneLines, NINTUNE, intune.lset);
    // Identify the user (CONSUMER) in the request configuration
    gpiod_request_config_set_consumer(intune.rcfg, CONSUMER);

    // Make the line request for the input lines
    intune.lreq = gpiod_chip_request_lines(chip, intune.rcfg, intune.lcfg);
    if (!intune.lreq) {
        fprintf(stderr, "KY-040: Tuner requesting input lines failed: %s\n", strerror(errno));
        // Clean up
        FreeGPIOActivity(&intune);
        gpiod_chip_close(chip);
        return ERRFAILEDINPUTREQ;
    };

    // Now do the initial read into GPIOInTuneValues to check everything is set up and working
    rtn = gpiod_line_request_get_values(intune.lreq, GPIOInTuneValues);
    if (rtn == FAILURE) {
        fprintf(stderr, "KY-040: Failed to get Tuner input values: %s\n", strerror(errno));
        FreeGPIOActivity(&intune);
        gpiod_chip_close(chip);
        return ERRINREQGETVALUES;
    };

    cerr << "libgpoid version: " << gpiod_api_version() << endl;

    // Initialize edge buffer and edge event handling
    edgeBufSize = gpiod_request_config_get_event_buffer_size(intune.rcfg);
    eventBuf = gpiod_edge_event_buffer_new(edgeBufSize);
    maxEdgeEvents = gpiod_edge_event_buffer_get_capacity(eventBuf);
   
    while (true) { // Sure this is dumb but this is just to play in the debugger

        state = NOTHINGISHAPPENING;
        direction = 0;
        select = NONE;

        rtn = gpiod_line_request_wait_edge_events(intune.lreq, -1); // Blocks until an edge is detected in intune line group

        int weight[4] = {1,2,4,8};
        direction = 0;
        
        for(j = 0; j < 4; j++) {
            // When you have an edge event above you have to clear it by reading it otherwise 
            readEvents = gpiod_line_request_read_edge_events(intune.lreq, eventBuf, maxEdgeEvents);      
            for(idx = 0; idx < readEvents; idx++) {
                // This gets to the last edge event in the buffer
                edgeEvent = gpiod_edge_event_copy(gpiod_edge_event_buffer_get_event (eventBuf, idx));
                edgeType =	gpiod_edge_event_get_event_type(edgeEvent);
                edgeOS = gpiod_edge_event_get_line_offset(edgeEvent);               
            };

            if(edgeType == GPIOD_EDGE_EVENT_RISING_EDGE) {
                direction += weight[j];
            };
            
            if(edgeOS == NTUNERCLK) {
                direction += weight[j]*2; 
            };

            /* Uncomment this block to get more info what is going on   
            cerr << "Number of Edge Events: " << readEvents << endl;
            std::string eType;
            eType = "GPIOD_EDGE_EVENT_FALLING_EDGE";
            if (edgeType == GPIOD_EDGE_EVENT_RISING_EDGE) eType = "GPIOD_EDGE_EVENT_RISING_EDGE";
            cerr << "Edge: " << edgeOS << " type: " << eType << " " << direction << endl;
            */

        };

        // Determine the direction of twistyness
        std::string strTune;
        if ((direction == 13)||(direction == 29)||(direction == 22)||(direction == 26)) {
            tune = CW;
            strTune = " CW ";
        }
        else if ((direction == 19)||(direction == 32)||(direction == 16)||(direction == 23)) {
            tune = CCW;
            strTune = " CCW ";
        }
        else {
            tune = IDLE;
            strTune = " IDLE ";
        };

        cerr << "Tune Direction: direction " << strTune << " " << direction << endl << endl;
        
    }; // while(true)

    // Clean up memory
    FreeGPIOActivity(&intune);
    
    return OKKY040; // Adios 

}; // End KY-040()
