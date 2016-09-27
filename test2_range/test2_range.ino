
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

// ************************************ **************** * * * * * define Settings
// *

#define TEST                TEST_BEACON

#define NODEID              MASTER                 // MASTER / SLAVE
#define BITRATE             BITRATE_55k5          // see above
#define DEBUG               1
#define CSV_OUTPUT          1


#define TEST_INTERVAL       1000
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
uint16_t test_length, test_duration;
unsigned long t_this_packet, t_last_packet, t_test_begin, t_duration, t_calc;
uint8_t this_pack, last_pack;
uint8_t packet_count, packet_dropped;
uint8_t packet_bug;

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
  // put your main code here, to run repeatedly:

  // ** * * * * * * * * *       Save code
  int t=0;
  
  char *variables[5];
  
  if(NODEID==MASTER){
    // ***    Read Serial
    // *
    
    // if in test state
    if( radio.receiveDone() ) {
      
      // 1. a message has been received if radio.receiveDone()
      t_this_packet = millis();
      
      t_duration = t_this_packet -t_test_begin;
      
      // 2. receive_message() to unpack it into local struct
      receive_message();
      
      // unpack string content into CS-Variables
      // returns the number of variables
      //t = unpack_message( variables);

      
      if(DEBUG){
          //Serial.print(variables[0]);
          //sprintf(buff,"var1: %s, var2: %s, var3: %s", variables[0], variables[1], variables[2] );
          //Serial.println(buff);     
        }
      
      t_calc = t_this_packet -t_last_packet;

      //sprintf(buff,"diff: %ld  - /700 is %ld,  /800 is %ld\n", t_calc, t_calc/700, t_calc/800);
      //Serial.print(buff);

      if( t_calc / 740 > 1)
        packet_dropped += (t_calc/740) -1;
        
      if(CSV_OUTPUT){
        // CSV-format
        // "packet ID, variable name, variable data, ms since last packet"
        
        /*sprintf(buff,"[CSV] %d,%s,%d,%d,%ld \n",
                      this_pack, variables[1],
                      string_to_int(variables[2]),
                       message.rssi, millis() );
        Serial.print(buff);            */

        sprintf(buff,"[CSV] #%d  rssi: %d\t%ldms last, test: %lds\t dropped %d\n",
                      packet_count, message.rssi,                       t_calc, t_duration/1000, packet_dropped );
        Serial.print(buff);
        }

          /*this_pack = string_to_int(variables[0]);
          if(DEBUG){
            sprintf(buff,"unpacked packet ID as %d\n", this_pack);
              Serial.print(buff);
          }
          if(packet_bug){
              packet_bug = 0;
              last_pack = this_pack;
          }
          else{
              // use ID to see if packet is missing
              t = packet_missing_t( this_pack );
              if( t > 0){
                packet_dropped += t;
              }
          }*/
          
      t_last_packet = t_this_packet;    // time
      
    }   // *** Eo receiveDone() 
    
    Serial.flush();
    radio.receiveDone();  // into Rx mode
    //LowPower.powerDown(BEACON_SLEEP, ADC_OFF, BOD_ON); 
    //LowPower.idle(SLEEP_2S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_OFF);
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


void receive_message(  ){
  packet_count++;
  
  // Unpack
  message.sender = radio.SENDERID;
  message.rssi = radio.RSSI;
  message.datalen = radio.DATALEN;

  // Print received packet
  if(DEBUG) Serial.print("\n[Rx] ");
  
  for(int m=0; m<message.datalen; m++)
  {     
    message.data[m] = char(radio.DATA[m]);
    if(DEBUG)     Serial.print(message.data[m]);
  }
  if(DEBUG)     Serial.println();
  
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

int unpack_message_t( char *vars[] ) {
  vars[0][0]='T';
  vars[0][1] = '1';
  vars[0][2] = '\0';
  vars[1][0] = 'a';
  vars[1][1] = 'b';
  vars[1][2] = 'c';
  vars[1][3] = '\0';
  
  return 2;
}

uint8_t packet_missing( uint8_t pack_ID ){
    uint8_t missing = 0;
    
    if(pack_ID < last_pack ){
      if(pack_ID +TEST_INTERVAL > last_pack+1){
        missing = pack_ID +TEST_INTERVAL - last_pack -1;
        
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
  test_duration = 50;
  
  beacon_tag = "tmp1";
  STATE = 't';
  packet_count = 0;
  packet_dropped = 0;

  packet_bug = 1;
  //for(int i=0;i<BEACON_PACKET_SIZE;i++)
  //  beacon_data[i]=char(i+36);

  t_test_begin = millis();
}

void beacon_code( ){
  // a. collect data for packet
  test_length++;
  
  if(test_length==TEST_INTERVAL)
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
      digit += char_to_digit(number[i]) *power(10,l-i-1);
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
      digit += char_to_digit(number[i]) *power(10,l-i-1);
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


