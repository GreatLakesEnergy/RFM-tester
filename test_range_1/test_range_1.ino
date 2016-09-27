/*    
 *   *    This program receives messages from the Beacon. and displays a few parameters in Serial output:
 *   
 *   use #DEFINES
 *   - DEBUGGING - to display the received packet
 *      ""[Rx] 66,tmp1,369
 *      
 *   - CSV_OUTPUT   to display
 *      packet_count , msg RSSI, and current count of dropped messages
 *      ""[CSV] #3  rssi: -51   dropped 0
 *      
 *   - TEST_WINDOW
 *      once packet_count reaches this integer the sequence is evaluated.
 *      how many packets received, how many were dropped, and stats on RSSI
 *      ""[Test] received: 30, dropped: 0 
          RSSI- min:-55 < avrg: -51 < max:-49
 *       
 *       */
 
#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include "RF_lib.h"
#include <SPI.h>
#include <RFM69registers.h>
#include <LowPower.h>         //get library from: https://github.com/lowpowerlab/lowpower

// *************************************** * * * * Defines
// *
#define NETWORKID         100  //the same on all nodes that talk to each other
#define MASTER            1    //unique ID of the gateway/receiver
#define SLAVE             2
#define BEACON            3
#define BITRATE_55k5      555
#define BITRATE_19k2      192
#define BITRATE_1k9       19
//#ifdef __AVR_ATmega1284P__
#define LED           15 // Moteino MEGAs have LEDs on D15
#define BUTTON_INT    1 //user button on interrupt 1 (D3)
#define BUTTON_PIN    11 //user button on interrupt 1 (D3)
/*#else
  #define LED           9 // Moteinos have LEDs on D9
  #define BUTTON_INT    1 //user button on interrupt 1 (D3)
  #define BUTTON_PIN    3 //user button on interrupt 1 (D3)
#endif  */

#define FREQUENCY     RF69_433MHZ        //RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Remove/comment if you have RFM69W!
#define SERIAL_BAUD   115200

#define TEST_BEACON         1

// ***************************** **************** * * * * * define Settings
// *

#define TEST                TEST_BEACON

#define NODEID              MASTER                 // MASTER / SLAVE
#define BITRATE             BITRATE_55k5          // see above
#define DEBUG               0
#define CSV_OUTPUT          1
#define TEST_OUTPUT         1

#define TEST_WINDOW            30
#define SLAVE_PACKET_ID_MAX    1000
// SLEEP TIMES :    15, 30, 60, 120, 250, 500MS, 1S, 2, 4
#define BEACON_SLEEP        SLEEP_500MS
#define BEACON_PACKET_SIZE  8
#define MAX_PACKET_LENGTH   30


// **************************************** * * * * *  * Program variables
char buff[100];
uint8_t buff_s;

char *beacon_tag;
char beacon_data[BEACON_PACKET_SIZE];
int beacon_int;
    
char ABC, STATE;
const char *line;
uint16_t test_length;
int this_pack, last_pack;
int packet_count, packet_dropped;
uint8_t packet_bug;

int t=0;
char texter[20];
char *variables[5];

unsigned long t_this_packet, t_last_packet, t_test_begin, t_duration, t_calc;
long s_rssi;
int avrg_count, s_rssi_min, s_rssi_max;

RFM69 radio;

struct RF_msg {
  uint8_t sender;
  int rssi;
  char data[MAX_PACKET_LENGTH];
  uint8_t datalen;
  uint8_t ack_requested;
}message;

// ************************************ * * * * Setup
// *
void setup() {
  
  Serial.begin(SERIAL_BAUD);
  if(BITRATE==BITRATE_55k5)
    radio.initialize(FREQUENCY,NODEID,NETWORKID);
  else if(BITRATE==BITRATE_19k2)
    radio.initialize_19k2(FREQUENCY,NODEID,NETWORKID);
  else if(BITRATE==BITRATE_1k9)
    radio.initialize_1k9(FREQUENCY,NODEID,NETWORKID);
  else {
    Serial.println("--Bitrate not selected, using default 55kHz");
    radio.initialize(FREQUENCY,NODEID,NETWORKID);
  }
  
  #ifdef IS_RFM69HW
    radio.setHighPower(); //only for RFM69HW!
  #endif
    radio.encrypt(ENCRYPTKEY);

  if(TEST==TEST_BEACON){
      
    initialise_beacon( );
    
  }
  
  pinMode(LED, OUTPUT);
  //pinMode(BUTTON_PIN, INPUT_PULLUP);
  //attachInterrupt(BUTTON_INT, handleButton, FALLING);


  // *******        Serial outputs
  // *
  sprintf(buff, "\nNode ID -   %d\nListening at %d Mhz...", NODEID,FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
    Serial.println(buff);
  sprintf(buff,"Bite-rate set to %d", BITRATE);
    Serial.println(buff);
  // CSV format 
  Serial.println("packet ID, variable name, variable data, ms since last packet");
  //Serial.flush();
  
  // Eo setup
}




// ***************************************** * * * * Loop
void loop() {
  
  // ** * * * * * * * * *       Slave code
  
  if(NODEID==MASTER){
    // ***    Read Serial
    // *
    
    // if in test state
    if( radio.receiveDone() ) {
      
      // 1. a message has been received if radio.receiveDone()
      
      
      // 2. receive_message() to unpack it into local struct
      receive_message(DEBUG);

      /*
      // unpack string content into CS-Variables
      // returns the number of variables
      //t = unpack_message( variables);
      
      if(DEBUG){
          //Serial.print(variables[0]);
          //sprintf(buff,"var1: %s, var2: %s, var3: %s", variables[0], variables[1], variables[2] );
          //Serial.println(buff);     
        } */
      
      unpack_variable(message.data, texter, 0);
      //Serial.println(texter);
      this_pack = string_to_int(texter);
      //Serial.println(this_pack);

      if(packet_bug){         // for the first packet, just adopt the packet ID
        s_rssi_max = message.rssi;
        s_rssi_min = message.rssi;
        packet_bug = 0;
      }
      else if( this_pack < last_pack){     // roll over after packet ID 1000.
       
        packet_dropped += this_pack + SLAVE_PACKET_ID_MAX -last_pack -1;
        if(DEBUG){
          sprintf(buff," # dropped %d: %d > ... > %d\n", this_pack -last_pack -1, last_pack, this_pack);
          Serial.print(buff);     }
      }
      else if(this_pack -last_pack > 2) {     // only after start counting dropped packets
        
        packet_dropped += this_pack -last_pack -1;
        if(DEBUG){
          sprintf(buff," # dropped %d: %d > ... > %d\n", this_pack -last_pack -1, last_pack, this_pack);
          Serial.print(buff);     }
      }


      if(CSV_OUTPUT){
        // CSV-format
        // "packet ID, variable name, variable data, ms since last packet"
        
        /*sprintf(buff,"[CSV] %d,%s,%d,%d,%ld \n",
                      this_pack, variables[1],
                      string_to_int(variables[2]),
                       message.rssi, millis() );
        Serial.print(buff);            */

        sprintf(buff,"[CSV] #%d  rssi: %d\t dropped %d\n",
                      packet_count, message.rssi, packet_dropped );
        Serial.print(buff);
        }

      s_rssi += message.rssi;            avrg_count++;
      if(message.rssi<s_rssi_min)        s_rssi_min = message.rssi;
      if(message.rssi>s_rssi_max)        s_rssi_max = message.rssi;

      
      // **  if test time ran out, print eval & restart
      if( TEST_OUTPUT )
      if(packet_count >= TEST_WINDOW) {
        s_rssi = int( s_rssi / avrg_count);
        sprintf(buff,"[Test] received: %d, dropped: %d \n", packet_count, packet_dropped );
        Serial.print(buff);
        sprintf(buff,"\t RSSI- min:%d < avrg: %ld < max:%d\n", s_rssi_min, s_rssi, s_rssi_max);
        Serial.print(buff);
        initialise_beacon();
      }
      last_pack = this_pack;
      
    }   // *** Eo receiveDone() 
    
    Serial.flush();
    radio.receiveDone();  // into Rx mode
    //LowPower.powerDown(BEACON_SLEEP, ADC_OFF, BOD_ON); 
    LowPower.idle(BEACON_SLEEP, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_OFF);
    // *** Eo if-Master
  }
  else if(NODEID==SLAVE){
    
    if(TEST==TEST_BEACON){

      beacon_code();
      Serial.flush();
      LowPower.powerDown(BEACON_SLEEP, ADC_OFF, BOD_ON); 
      //LowPower.idle(SLEEP_2S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_OFF);
    }
    
    
    // *** Eo if Slave
  }


  // *** Eo loop
}



// ################################ # # # #       Functions
// #

//                        ###### MASTER


void receive_message( uint8_t PRINTOUT  ){
  packet_count++;
  
  // Unpack
  message.sender = radio.SENDERID;
  message.rssi = radio.RSSI;
  message.datalen = radio.DATALEN;

  // Print received packet
  if(DEBUG) Serial.print("[Rx] ");
  
  for(int m=0; m<message.datalen; m++)
  {     
    message.data[m] = char(radio.DATA[m]);
    if(PRINTOUT)     Serial.print(message.data[m]);
  }
  if(PRINTOUT)     Serial.println();
  
  if( radio.ACKRequested() )
  {     radio.sendACK();      }
  
  // ** Eo receive_message
}


int unpack_message( char *vars[] ) {
  uint8_t v_index = 0;    // marks variable num in array
  uint8_t v_length = 0;   // marks length of that var string
  uint8_t m_index = 0;  // marks char in message
  uint8_t endloop = 0;
  
  while( !endloop ){
    //sprintf(buff,"%d-",v_index);
    //Serial.print(buff);
    
    // fill var until ','
    while( message.data[m_index]!=',' 
            && message.data[m_index]!='\0' 
              && m_index<message.datalen )       
    {
      vars[v_index][v_length] = message.data[m_index];
      
      m_index++;
      v_length++;
    }
    
    if(message.data[m_index]== '\0'){
      // reached end of msg
      endloop = 1;
      vars[v_index][v_length] = '\0';
      v_index++;
    }
    else{
      vars[v_index][v_length] = '\0';
      v_length = 0;
      v_index++;
      m_index++;  // step off ','
    }
    
    // Eo while
  }
  
  return v_index;
}

uint8_t unpack_variable( char *line, char *into, int pos ){
    uint8_t v_length = 0;   // marks length of that var string
    uint8_t m_index = pos;  // marks char in message
    
    // fill var until ','
    while( line[m_index]!=',' )       
    {
      into[v_length] = line[m_index];
      
      m_index++;
      v_length++;
    }
    m_index++;    // step off of ','
    into[v_length]='\0';    

    return m_index;
}

uint8_t packet_missing( uint8_t pack_ID ){
    uint8_t missing = 0;
    
    if(pack_ID < last_pack ){
      if(pack_ID +SLAVE_PACKET_ID_MAX > last_pack+1){
        missing = pack_ID +SLAVE_PACKET_ID_MAX - last_pack -1;
        
        if(DEBUG){
          sprintf(buff," #\t%d packs missing: %d > ... > %d \n", missing, last_pack, pack_ID);        
          Serial.print(buff);         }
          
      }
    }
    else if(pack_ID > last_pack+1){
      missing = pack_ID - last_pack -1;
      
      if(DEBUG) {
        sprintf(buff," #\t%d packs missing: %d > ... > %d \n", missing, last_pack, pack_ID);        
        Serial.print(buff);       }
        
    }
    
    
    last_pack = pack_ID;            // packet ID
    return missing;
}

uint8_t packet_missing_t( uint8_t pack_ID ){
    uint8_t missing = 0;
    
    if(pack_ID > last_pack+1){
      missing = pack_ID - last_pack -1;
      
      if(DEBUG) {
        sprintf(buff," #\t%d packs missing: %d > ... > %d \n", missing, last_pack, pack_ID);        
        Serial.print(buff);       }
        
    }
    
    
    last_pack = pack_ID;            // packet ID
    return missing;
}

//                            ########     BEACON
void initialise_beacon(  ){
  test_length = 0;
  //test_duration = 50;
  
  beacon_tag = "tmp1";
  STATE = 't';
  packet_count = 0;
  packet_dropped = 0;

  packet_bug = 1;
  //for(int i=0;i<BEACON_PACKET_SIZE;i++)
  //  beacon_data[i]=char(i+36);

  s_rssi=0;
  avrg_count=0;
  s_rssi_min=0;
  s_rssi_max=0;

  t_test_begin = millis();
}

void beacon_code( ){
  // a. collect data for packet
  test_length++;
  
  if(test_length==SLAVE_PACKET_ID_MAX)
    test_length = 0;
    
  beacon_int = 303 +test_length;
 
  // b. send packet 
  buff_s = sprintf(buff,"%d,%s,%d",test_length, beacon_tag, beacon_int);
  
  // ---- Code ref RFM69.h
  // send(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK=false);
  radio.send(MASTER, buff, buff_s, 0 );
  
  if(DEBUG)   Serial.println(buff);
  
  // Blink
  Blink(LED, 100, 1);

  
}

//              ****************  Functional operators
//

/*
char *read_serial_string( ){
  char *out;
  int i=0;
  
  line = Serial.readStringUntil(' ').c_str();
  while( line[i]!= '\0'){
    out[i] = line[i];
  }
  
  return out;
}*/

int power( int base, int expo){
  int singular = base;
  base = 1;
  
  for(int i=0; i<expo; i++){
    base *= singular;
  }
  return base;
}

int string_length( char *scribbles){
  int l=0;
  while(scribbles[l]!= '\0')
  {     
    l++;   
  }
  return l;
}

int char_to_digit( char T){
  int dig = 0;
  return int(T)-48;
}

int string_to_int(char *number){
  // get length
  int digit = 0;
  
  int i=0;
  int l = string_length(number);  

  if(number[0]=='-'){
    // negatives
    for(int i=1;i<l;i++)
    {
      digit = digit + power(10,l-i-1) *char_to_digit(number[i]);
      //sprintf(buff,"Digit: %c, = %d,\t now number is %d",number[i], char_to_digit(number[i]), digit);
      //Serial.println(buff);
    }
    digit = -digit;
  }
  else
  {
    // positives
    for(int i=0;i<l;i++)
    {
      digit += power(10,l-i-1) *char_to_digit(number[i]);
    }
  }
  
  return digit;
  
}

void Blink(byte PIN, byte DELAY_MS, byte loops)
{
  for (byte i=0; i<loops; i++)
  {
    digitalWrite(PIN,HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN,LOW);
    delay(DELAY_MS);
  }
  
} // ** Eo Blink


