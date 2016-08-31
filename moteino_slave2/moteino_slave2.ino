// ***************************************************************************************
// Sample RFM69 sketch for Moteino to illustrate sending and receiving, button interrupts
// ***************************************************************************************
#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include "RF_lib.h"
#include <SPI.h>
#include <LowPower.h> //get library from: https://github.com/lowpowerlab/lowpower
//*********************************************************************************************

#define NETWORKID    100  //the same on all nodes that talk to each other
#define MASTER       1    //unique ID of the gateway/receiver
#define SLAVE        2
#define NODEID       SLAVE  //change to "SENDER" if this is the sender node (the one with the button)

// **** Config Variables
#define WAIT_FOR_ACK        500
#define SEND_MSG_ATTEMPTS   3
#define MINIMUM_MSG_LENGTH  10
#define PACKET_SIZE         16
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Remove/comment if you have RFM69W!

//*********************************************************************************************
#define SERIAL_BAUD   115200
#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define BUTTON_INT    1 //user button on interrupt 1 (D3)
  #define BUTTON_PIN    11 //user button on interrupt 1 (D3)
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define BUTTON_INT    1 //user button on interrupt 1 (D3)
  #define BUTTON_PIN    3 //user button on interrupt 1 (D3)
#endif

RFM69 radio;

uint8_t RECIPIENT;
uint8_t MSG_DATA[RF69_MAX_DATA_LEN];
uint8_t MSG_LEN, MSG_SENDER_ID, MSG_TARGET_ID;
int MSG_RSSI, rcv_count, counter;

//String Letter;
//char Packet[RF69_MAX_DATA_LEN];
char ABC, STM;
int NUM;
char buff[50];
uint8_t buff_s;
uint8_t sequence[PACKET_SIZE];                       
int iki = 0;
boolean endloop = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  sprintf(buff, "\nNode ID -   %d\nListening at %d Mhz...", NODEID,FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  Serial.flush();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  attachInterrupt(BUTTON_INT, handleButton, FALLING);

  
  if( NODEID==MASTER)
    RECIPIENT = SLAVE;
  if(NODEID==SLAVE)
    RECIPIENT = MASTER;

  rcv_count = 0;
  STM = 't';

  for(int a=0; a<PACKET_SIZE; a++){
    sequence[a] = 'a';
  }
}

//******** THIS IS INTERRUPT BASED DEBOUNCING FOR BUTTON ATTACHED TO D3 (INTERRUPT 1)
#define FLAG_INTERRUPT 0x01
volatile int mainEventFlags = 0;
boolean buttonPressed = false;
void handleButton()
{
  mainEventFlags |= FLAG_INTERRUPT;
}

byte LEDSTATE=LOW; //LOW=0

void loop() {
  
  Serial.println("l");

  // ******     Slave
  // *
  // *
  if(NODEID==SLAVE)
  {
    if(radio.receiveDone())
    {
      
      Serial.println("[RX] Receiving");
      rcv_count++;
      
      MSG_SENDER_ID = radio.SENDERID;
      //MSG_TARGET_ID = NODEID;
      MSG_RSSI = radio.RSSI;
      MSG_LEN = radio.DATALEN;
      
      Serial.print("[MSG]\t");
      for(int i=0; i<MSG_LEN; i++){
        MSG_DATA[i] = radio.DATA[i];
        //sprintf(buff,"%c",char(MSG_DATA[i]) );
        Serial.print( char(MSG_DATA[i]) );       
      }

      sprintf(buff,"\n[id:%d]",MSG_SENDER_ID);
      Serial.print(buff);
      sprintf(buff,"\t[rssi:%d]",MSG_RSSI);
      Serial.print(buff);
      sprintf(buff,"\t[cnt:%d]",rcv_count);
      Serial.print(buff);
      sprintf(buff,"\t[len:%d]",MSG_LEN);
      Serial.println(buff);
      
      if (radio.ACKRequested()){
        radio.sendACK();      
        Serial.println("\t-- ACK Sent");
      }
      
      if(STM=='t')
      {
          // ****     Running in test transmit loop
          
          
          // [  count of received message, response attempt counter, TxRssi, MSG_LEN,  16 * Char ]
          counter = 0;
          buff_s = sprintf(buff,"[%d,%d,%d,%d,%c%c%c%c:%c%c%c%c:%c%c%c%c:%c%c%c%c]",
               rcv_count, counter, MSG_RSSI, MSG_LEN,
               MSG_DATA[0],MSG_DATA[1],MSG_DATA[2],MSG_DATA[3],
               MSG_DATA[4],MSG_DATA[5],MSG_DATA[6],MSG_DATA[7],
               MSG_DATA[8],MSG_DATA[9],MSG_DATA[10],MSG_DATA[11],
               MSG_DATA[12],MSG_DATA[13],MSG_DATA[14],MSG_DATA[15]
                );
    
          Serial.print("\n[TX] Response frame:\t  buffer size: "); Serial.println(buff_s);
          //Serial.println(buff);
          while (!radio.sendWithRetry(MSG_SENDER_ID, buff, buff_s, SEND_MSG_ATTEMPTS, WAIT_FOR_ACK ) ) //target node Id, message as string or byte array, message length
          {
            counter++;
            Serial.println("\t-- Response NOT Acknowledged, Repeating Response:");
            buff_s = sprintf(buff,"[%d,%d,%d,%d,%c%c%c%c:%c%c%c%c:%c%c%c%c:%c%c%c%c]",
               rcv_count, counter, MSG_RSSI, MSG_LEN,
               MSG_DATA[0],MSG_DATA[1],MSG_DATA[2],MSG_DATA[3],
               MSG_DATA[4],MSG_DATA[5],MSG_DATA[6],MSG_DATA[7],
               MSG_DATA[8],MSG_DATA[9],MSG_DATA[10],MSG_DATA[11],
               MSG_DATA[12],MSG_DATA[13],MSG_DATA[14],MSG_DATA[15]
                );
            // Eo while  
          }
          Serial.println(buff);
          sprintf(buff,"\t-- Response ACKnowledged on %d. attempt", counter+1);
          Serial.println(buff);


          // Eo STM - testing
      } 
      else 
      if( MSG_DATA[0]==127 && MSG_DATA[1]==127 && MSG_DATA[4]==127 && MSG_DATA[5]==127 )
      {
        // ****   initialise test run by transmitting msg [ # # X Y # # # P Q ... ]
        rcv_count = 0;
        if( char(MSG_DATA[4]) == 't')
          STM = 't';

        // Read parameters from Frame?
      }
      else
      {
        // ****   idle
        // save power when just waiting
        Serial.println("Slave in Idle mode, whats this msg?");
        
        STM = 'i';

        
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON); 
      }
      
      
      //Serial.flush();
      
      
    }  // ********************    Eo if Receive
    else{
        // ***** Serial stuff
        // *
        NUM = Serial.read();
        
        //Serial.println(char(NUM) );
        
        if( NUM != -1){
          Serial.print("Reading serial comm:\t");
          Serial.print(NUM);
          Serial.print("\t");
          Serial.print( char(NUM) );

          // #################
          Serial.println("Begin");
    
          // Loop started by Serial
        }
      
      // Eo Serial command receive
    }
    
    radio.receiveDone(); //put radio in RX mode
    Serial.flush();
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_ON); 

    
    // *********************** Eo SLAVE If
  }
  
  if(NODEID==MASTER)
  {
    /* ************************** MASTER Code
     * 
     * Serial, chose ACK timeout, retry times
     * 
     *  if in loop test sending mode
     *   send frame -> time sent
     *     get ACK  -> time ACK
     *   if not ACK, repeat ->  X times
     *  
     *  wait for response
     *    get values into results array or write via serial.
     *      - frame identical?
     *      - Rssi Tx & Rx
     *      
     */
    NUM = Serial.read();
       
    if( NUM != -1){
      Serial.print("Reading serial comm:\t");
      //Serial.print(NUM);
      //Serial.print("\t");
      //Serial.print( char(NUM) );
      ABC = char(NUM);

      if(ABC=='t'){
        Serial.println("Begin");
        STM = 't';
      }

      
      // Eo if Serial read cmd
    }
    if(STM=='t')
    {
      Serial.print("[TX] New packet");
      
      counter = 0;
      sequence[iki] += 1;
      iki = (iki + 1) % PACKET_SIZE;
      
      Serial.print(sequence);
      
      while(!EndLoop)
      {
        if(radio.sendWithRetry(SLAVE, sequence, PACKET_SIZE, 0, WAIT_FOR_ACK ) ){
          EndLoop = 1;
          
        }
        else{
          counter++;
          Serial.print("\t##Reattmpt");
        }
      
        
      }
      
    }
    
     
    // *************** Eo Master If
  }

  //Eo *************************************      * * * * * * * * * * * * *    LOOP
} 

String int_to_digits( int pijin, int digits){
  String smoosh;
  
  smoosh = String(pijin);
  while(smoosh.length() <digits){
    smoosh = "0" +smoosh;
  }
  
  return smoosh;
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
}

char if_serial_msg( ){
  int ABC = Serial.read(); 
  if(ABC != -1)
  {
    return char(ABC);
    Serial.print(char(ABC));
  }
  else
    return '!';
}


String read_serial_to(char comma ){
  String str;
  str = Serial.readStringUntil(comma );
  
  return str; 
}   //Eo read serial int


String read_serial_line( ){
  String str;
  str = Serial.readString( );
  
  return str;
}   //Eo read serial int


int read_serial_int( ){
  String str;
  str = Serial.readString( );
  
  return str.toInt(); 
}   //Eo read serial int

// = ------------------------------

