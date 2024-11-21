
###A note about KY-040 type rotary encoder switches 
This is all necessary because of the asychronous nature of
human knob twiddling, how noisy the KY-040 seems to be and 
watching for just levels of the CLK and DT line are unpredictable
So we have to resort to watching the edge transitions from 
the rotary encoder. After a series of experiments the edges 
seem to work relatively predictably. There are indeed other 
ways to read the encoder but this project uses libgpoid 2.1.2
Also please note that multiple edge events are ignore as 
best as possible and that the CLK and DT lines have been 
debounced
 
There are 4 unique level sequences 
that are observed twisting a rotary encoder in one direction. 
With this in mind and confirmed by a little noodling
and debugging we find the corresponding edge sequences

          Twisting CW                 Twisting CCW
      CLKR, DTR , CLKF, DTF       CLKR, DTF , CLKF, DTR
      DTR , CLKF, DTF , CLKR      DTF , CLKF, DTR , CLKR
      CLKF, DTF , CLKR, DTR       CLKF, DTR , CLKR, DTF
      DTF , CLKR, DTR , CLKF      DTR , CLKR, DTF , CLKF
 
      Where
         CLKR = Clock Rising
         DTR  = Data Rising
         CLKF = Clock Falling
         DTF  = Data Falling
 
Unfortunately when looking a series of edges events there is no way 
to know what the level of the opposing signals are simultaniouslly
as far as I understand libgpiod. So we are left with coding the 
states of the edges which is different than codeing the concurrent 
levels of CLK and DT per sample. 
There are single edge states that can be observed for any event edge 
sample requesting CLK and DT activity. 
The following code is used to determine the direction of rotation
knowing only the squence of four edge events.
We will code the first digit of a two bit binary number by using 
source of the edge event number as the first digit (MSB) and the 
direction of the edge event as the second digit (LSB). 
We will choose a CLK edge event source as 1 and a DT edge event source as as 0. 
For direction well define an rising edge event as 1 and 
the falling edge event as 0. So if CLK is read as a rising edge event then 
we choose 11 to represent that edge. The leading 1 (MSB) is for CLK and the 
trailing (LSB) is 1 for a rising edge event. if we see a DT rising event then 
we code that event as 01. There are only 4 possible codes seen below
 
      CLKR is 11 (3)
      CLKR is 10 (2)
      DTR  is 01 (1)
      DTF  is 00 (0)
 
For each direction of rotation there are only 4 possible valid sequences
of edge events, please see a diagram of a typical quadrature rotary 
encoder (1). Since the encoder can be turned clockwise (CW) and counter 
clockwise (CCW) and there are 4 valid sequences for any sequential sampled 
set of edges for one direction of rotation there are 8 total valid edge 
sequences for both CW and CCW rotation. Clearly we are assuming that the 
edge sequences represent 4 presumablty contiguous events. This is why 
looking at the edges is easiser to use than levels becasue the rate at 
which humans twist knobs is unpredictacle and inconsistant. Since each 
edge event sample represents a sequance we have to also code for time
The easist way to do is to weight each time in the sequence. For this 
effort weights (or multipliers) of increasing powers of two were chosen 
for simplicty. Also the simplest thing to do is increase the weights with 
time. For the four time intervals 
there are four weights:
   
      t=0 multipiled by 2^0 
      t=1 multipiled by 2^1
      t=2 multipiled by 2^2
      t=3 multipiled by 2^3
 
So putting the edge event description coding, the source of the edge and the 
direction of the edge, and the weights we can find a number associated with 
any of the eight valid edge event sequences. In this case we find that the 
resultant number that comes from this process is unique amongst the  
4^4 (256) possible 4 edge event sequences. 

Below are then the coded sequences that we can use to detect KY-040 rotation 
either in the CW or CCW directions 
 
  CW
  CLKR, DTR , CLKF, DTF   -> 3 + 2*1 + 4*2 + 8*0 = 13
  DTR , CLKF, DTF , CLKR  -> 1 + 2*0 + 4*0 + 8*3 = 29
  CLKF, DTF , CLKR, DTR   -> 2 + 2*3 + 4*3 + 8*1 = 22
  DTF , CLKR, DTR , CLKF  -> 0 + 2*1 + 4*1 + 8*2 = 26
 
  CCW
  CLKR, DTF , CLKF, DTR   -> 3 + 2*0 + 4*2 + 8*1 = 19
  DTF , CLKF, DTR , CLKR  -> 0 + 2*2 + 4*1 + 8*3 = 32
  CLKF, DTR , CLKR, DTF   -> 2 + 2*1 + 4*3 + 8*0 = 16 
  DTR , CLKR, DTF , CLKF  -> 1 + 2*3 + 4*0 + 8*2 = 23
 
 Note:
     Please refer to the excellent web page on the rotary encoder
     which greatly helped inform this method
     (1) https://www.best-microcontroller-projects.com/rotary-encoder.html
     Vellemann WP1435 was used to for this code 
 
