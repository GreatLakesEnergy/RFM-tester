
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
#define NODEID       MASTER  //change to "SENDER" if this is the sender node (the one with the button)

// **** Config Variables
#define WAIT_FOR_ACK        500
#define WAIT_FOR_ACK_TX     500
#define SEND_MSG_ATTEMPTS   3
#define MINIMUM_MSG_LENGTH  10
#define PACKET_SIZE         16
#define WAIT_FOR_RESPONSE   1000
#define DELAY_FAILED_PACKET 100
#define DEBUG_OUTPUT        0

//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY     RF69_433MHZ        //RF69_915MHZ
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

char begin_char, end_char;

uint8_t RECIPIENT;
uint8_t MSG_DATA[RF69_MAX_DATA_LEN];
uint8_t MSG_LEN, MSG_SENDER_ID, MSG_TARGET_ID;
int MSG_RSSI, rcv_count, counter;
uint16_t test_length;

//String Letter;
//char Packet[RF69_MAX_DATA_LEN];
char ABC, STM;
int NUM;
char buff[100];
uint8_t buff_s;
char sequence[PACKET_SIZE];                       
int iki = 0;
uint8_t endloop = 0;
uint8_t packet_successful = 0;
long time_transmit, time_to_ack, time_to_response;
int Tx_timeouts, Rx_timeouts;

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
  Rx_timeouts = 0;
  Tx_timeouts = 0;
  test_length = 0;
  begin_char = 'a';
  end_char = 'i';
  
  for(int a=0; a<PACKET_SIZE; a++){
    sequence[a] = begin_char;
  }
  //sequence[16] = '\0';
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
  
  if(DEBUG_OUTPUT)  Serial.print("l");

  // ****** *********************** * * * * * * *    Slave
  // * * * * * * * * *
  // *
  if(NODEID==SLAVE)
  {
    //Serial.print("s");
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
      else
        Serial.println("\t-- NO ACK requested");
      
      if(STM=='t')
      {
          if(LEDSTATE==LOW)
            LEDSTATE=HIGH;
          else LEDSTATE=LOW;
          digitalWrite(LED, LEDSTATE);
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
          endloop = 0;

          while(!endloop)
          {
            // *** Try send response Loop
            // *
            if(radio.sendWithRetry(MSG_SENDER_ID, buff, buff_s, 0, WAIT_FOR_ACK ) )
            {
              // ** If Send Acknowledged
              endloop = 1;
              Serial.println(buff);
              sprintf(buff,"\t-- Response ACKnowledged on %d. attempt", counter+1);
              Serial.println(buff);
            }
            else
            {
              // Count failures
              counter++;
              if(counter >= SEND_MSG_ATTEMPTS)
              {
                // If failed too many times
                Serial.println("\t[Tx]  --Response FAILED!");
                endloop = 1;
                radio.receiveDone();  // Put radio in Tx mode immediately
              }
              else
              {
                // *** Repeat
                Serial.println("\t-- NOT Acknl.  Repeating Response:");
                buff_s = sprintf(buff,"[%d,%d,%d,%d,%c%c%c%c:%c%c%c%c:%c%c%c%c:%c%c%c%c]",
                   rcv_count, counter, MSG_RSSI, MSG_LEN,
                   MSG_DATA[0],MSG_DATA[1],MSG_DATA[2],MSG_DATA[3],
                   MSG_DATA[4],MSG_DATA[5],MSG_DATA[6],MSG_DATA[7],
                   MSG_DATA[8],MSG_DATA[9],MSG_DATA[10],MSG_DATA[11],
                   MSG_DATA[12],MSG_DATA[13],MSG_DATA[14],MSG_DATA[15]
                    );
                } //Eo else
              
            } // Eo if send failed
            
            // Exit --> with Endloop
            
            // ***  Eo while  
          }

          // Succeeded or failed.


          // Eo STM - testing
      }
      else
      {
        // ** If not in testing mode
        // *  look at start signal
          if( MSG_DATA[0]==127 && MSG_DATA[1]==127 && MSG_DATA[4]==127 && MSG_DATA[5]==127 )
          {
            // ****   initialise test run by transmitting msg [ # # X Y # # # P Q ... ]
            rcv_count = 0;
            if( char(MSG_DATA[2]) == 't')
              STM = 't';
    
            // Read parameters from Frame?
          }
          else
          {
            // ****   idle
            // save power when just waiting
            sprintf(buff, "Slave in Idle mode, whats this msg?");
            Serial.println(buff);
            
            //STM = 'i';
            Serial.flush();
            radio.receiveDone();
            LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON); 
          }
      }
      
      //Serial.flush();
      
      
    }  // ********************    Eo if Receive
    else{
        // ***** If no data received
        // *  check serial
        //
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
    
    Serial.flush();
    radio.receiveDone(); //put radio in RX mode
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_ON); 
    
    // *********************** Eo SLAVE If
  }
  
  if(NODEID==MASTER)
  {
    //Serial.print("M");
    /* ************************** * **  * * * * * * *  * * *  MASTER Code
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
     *  ***************************************  * * * * * * 
     */
    NUM = Serial.read();
    
    if( NUM != -1){
      Serial.print("Reading serial comm:\t");
      //Serial.print(NUM);
      //Serial.print("\t");
      //Serial.print( char(NUM) );
      ABC = char(NUM);
      //ABC = 't';
      
      if(ABC=='t'){
        sprintf(buff,"CMD : %c\t--Begin test sequence", ABC);
        Serial.println(buff);
        
        STM = 't';
        test_length = 0;
        Rx_timeouts = 0;
        Tx_timeouts = 0;
      }
      else 
      {
        sprintf(buff,"CMD : %c\t--Idle mode.", ABC);
        Serial.println(buff);
        
        STM = 'i';
        
        
      }
      
      
        // Eo if Serial read cmd
      }
    
    //Serial.print(STM);
      
    if(STM=='t')
    {
      //Serial.print("t");
      sprintf(buff,"[TX] New packet %d.",test_length);
      Serial.print(buff);
      
      counter = 0;
      endloop = 0;
      
      // ***** Send package with retries and timeout  
      while(!endloop)
      {
        time_transmit = millis();
        if(radio.sendWithRetry(SLAVE, sequence, PACKET_SIZE, 0, WAIT_FOR_ACK_TX ) )  
        {
          time_to_ack = millis() -time_transmit;
          radio.receiveDone();    // Put Master into Rx mode
          endloop = 1;
          packet_successful = 1;
          sprintf(buff, "\t--- ACK Y! on %d. attmpt", counter+1);
          Serial.println(buff);
          sprintf(buff, "\t- d T :  %ld ms after transm.", time_to_ack );
          Serial.println(buff);
          // *** Eo successful transmit
        } 
        else
        {
          
          if(counter >= SEND_MSG_ATTEMPTS){
            time_to_ack = 0; //millis() -time_transmit;
            endloop = 1;
            packet_successful = 0;
            Tx_timeouts++;
            sprintf(buff,"\n[Tx] --Failed %d attmpts, \tTotal %d.", counter, Tx_timeouts);
            Serial.println(buff);
          } // Eo -- Send msg attempts
          counter++;
        } //Eo else

        // exit loop --> endloop 
        // * Eo while Tx loop
      }   
      
      /*buff_s = sprintf(buff,"Pkt: [%c%c%c%c:%c%c%c%c:%c%c%c%c:%c%c%c%c]",
               MSG_DATA[0],MSG_DATA[1],MSG_DATA[2],MSG_DATA[3],
               MSG_DATA[4],MSG_DATA[5],MSG_DATA[6],MSG_DATA[7],
               MSG_DATA[8],MSG_DATA[9],MSG_DATA[10],MSG_DATA[11],
               MSG_DATA[12],MSG_DATA[13],MSG_DATA[14],MSG_DATA[15]
                );        */
      
      //sequence[16] = '\0';
      Serial.print("  [Packet: ");
      for(int a=0;a<PACKET_SIZE;a++){
        Serial.print(sequence[a]);
      }
      Serial.println(" ]");
      
      //sprintf(buff, "[Packet: %s ]\n", sequence);
      //Serial.print(buff);
      //Serial.println(sequence);
      
      
      // ************* Packet Acknowledged
      if(!packet_successful){
        Serial.println("[TX] ### Transmit failed");
        
        // Send report of failed pack

        
        //delay(DELAY_FAILED_PACKET);
        // *** Eo unsuccessful packet routine
        Serial.flush();
        radio.receiveDone();
        LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_ON); 
      }
      else
      {
          endloop = 0;
          
          // ** Wait for Echo loop
          // *
          while( !endloop ){
            
            if(radio.receiveDone() )
            {
              // *** If received something
              // *
              Serial.println("[RX] Packet Response ");
              endloop = 1;
              MSG_SENDER_ID = radio.SENDERID;
              MSG_RSSI = radio.RSSI;
              MSG_LEN = radio.DATALEN;

              Serial.print("  [MSG]\t");
              for(int i=0;i<MSG_LEN;i++)
              {
                MSG_DATA[i] = radio.DATA[i];
                Serial.print( char(MSG_DATA[i]) );       
              }

              sprintf(buff,"\n  [id:%d]",MSG_SENDER_ID);
              Serial.print(buff);
              sprintf(buff,"\t[Rxrssi:%d]",MSG_RSSI);
              Serial.print(buff);
              sprintf(buff,"\t[sequ:%d]",test_length);
              Serial.print(buff);
              sprintf(buff,"\t[len:%d]",MSG_LEN);
              Serial.println(buff);

              if (radio.ACKRequested()){
                radio.sendACK();      
                Serial.println("\t-- ACK Sent");
              }
              // got packet in radio.DATA
            }
            else
            {
              // How much time has passed?
              time_to_response = millis() -time_transmit -time_to_ack;
              
              if(time_to_response >= WAIT_FOR_RESPONSE) {
                Rx_timeouts++;
                endloop = 1;
                sprintf(buff,"\n[Rx] --Failed #Response Timeout. No of timeouts:  %d", Rx_timeouts);
                Serial.println(buff);
                
                //delay(DELAY_FAILED_PACKET);
                Serial.flush();
                radio.receiveDone();
                LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_ON); 
              }
                
            } // Eo - else
            
          }   // ***** * *  Eo Receive Response Loop

          
          // ***Eo Successful packet routine
      }

      // Modify package for next frame
      sequence[iki] += 0x01;
      iki++;
      if(iki>=PACKET_SIZE)
        iki=0;
        
      test_length++;
      
      if( sequence[12]==end_char && sequence[13]==end_char && sequence[14]==end_char && sequence[15]==end_char )
      {
        STM = 'i';    // Change state

        // Reset sequence
        for(int a=0; a<PACKET_SIZE; a++){
          sequence[a] = begin_char;
        }

        sprintf(buff,"Reached end of sequence.\nThank you,\tand good night.");
        Serial.println(buff);
        // Eo if End sequence
      }
      // **** Eo     Test mode
    }
    else{
      // **** If not in test sequence mode
      // be idle, read serial every 2 s
      Serial.flush();
      radio.receiveDone();
      LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_ON); 
    }
    //
     
    // *************** Eo Master If
  }
  
  //Eo *************************************      * * * * * * * * * * * * *    LOOP
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

