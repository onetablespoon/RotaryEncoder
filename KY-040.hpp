#include <string>

// #defines
// KY-040 Switch 
#define CLK         0
#define DT          1
#define SW          2

// Select
// Note that the sense of these is reversed
// ACTIVE is not selected Tune Switch not pushed in and 
// INACTIVE is selected Tune Switvh pushed in 
#define ACTIVE      8    
#define INACTIVE    9

#define EXIT        12

// Tuner State
#define CW          20  // Clockwise
#define CCW         21  // Counter ClockWise
#define IDLE        22
#define SELECTED    24

#define SOMETHINGHAPPENED   2000
#define NOTHINGISHAPPENING  2001

#define FAILURE  -1

// GPIO Lines
// Input GPIO lines numbers and counts
#define NINTUNE     3  // There are 3 Tuner Rotary Encoder (RE) inputs
#define NTUNERDT    17 // GPIO 17 Tuner RE DT (Data) Pin (White)
#define NTUNERCLK   27 // GPIO 27 Tuner RE CLK (Clock) Pin (Orange)
#define NTUNERSW    4  // GPIO  4 Tuner RE SW (Switch) Pin (Brown)



// End GPIO Lines

// DialMap Null string Fields
#define NOTUSED ""

// Command line args defines
#define PRIMARY     1 
#define SECONDARY   2
#define TERTIARY    3
#define NONE        4
// End command line args defines

// Return Codes
#define OKKY040                         100
#define OKGPIOLINEFREE                  104
#define OKINITGPIOACTION                105

#define ERRINREQGETVALUES               913
#define ERRREQUESTCONFIG                914
#define ERRFAILEDINPUTREQ               915
#define ERRREADINFO                     916
#define ERRLINENOTFREE                  917
#define ERRKY040OWNED                   918
#define ERRGPIOINUSE                    920
#define ERRLINESETTINGS                 921
#define ERRLINECONFIG                   922  

// libgpiod defines
#define CONSUMER        "KY-040"     // This is name of the GPIO (libgpiod) user 
#define GPIOCHIP        "/dev/gpiochip0"   // This is for RPi5
// End libgpiod defines





// Structure Definitions
// Structure to keep the GPIO line number, name, and GPIO line state values together
struct stGPIOline
{
    struct gpiod_line_values *val;
    char name[32];
    unsigned int number;
};

// Structure that contains all the libgpiod data
// structures necessary to make a request to set or get
// one or more GPIO lines. This structure allows for groups of
// GPIO lines that are defined the same i.e., have the same function
// This structure is used to group the switch states and LED outputs together
struct stGPIOActivity
{
    struct gpiod_line_settings *lset; 
    struct gpiod_line_config *lcfg;
    struct gpiod_request_config *rcfg;
    struct gpiod_line_request *lreq;
};
