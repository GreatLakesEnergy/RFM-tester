/*  ****  * * * * *  R F TEST
 *  *
 *  
 *  --- Test Setup
 *    To test RF communication between two nodes, Master and Slave.
 *    
 *    Packets are sent in a pretty linear sequence. A char array of 16 chars 
 *    Each frame one char is increased by 0x01. This allows for human readable verification
 *    The test therefore establishes wether Packets are sent & Acknowledged.
 *      How many attempts this requires and how long it takes
 *    For further verification of possible bit errors the sent Packet is mirrored back 
 *      and compared to the original.
 *    
 *    ** Slave
 *     For ease of coding, setup and execution the slave is set-up to echo messages 
 *    it receives from the Master. In the content of the response it puts
 *     - RSSI, The received signal strength indicator of the message it receives from Master
 *     - It keeps count of the number of messages received from the Master, 
 *              helps track lost messages
 *     - It adds the number of attempts it makes to send (and receive ACK)
 *              its resposne to the master
 *     
 *    ** Master
 *   
 *   */

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include "RF_lib.h"
#include <SPI.h>
#include <RFM69registers.h>
#include <LowPower.h>         //get library from: https://github.com/lowpowerlab/lowpower

#define NETWORKID         100  //the same on all nodes that talk to each other
#define MASTER            1    //unique ID of the gateway/receiver
#define SLAVE             2
#define BITRATE_55k5      555
#define BITRATE_19k2      192
#define BITRATE_1k9       19
uint8_t DEBUG_OUTPUT =       0;
uint8_t CSV_OUTPUT =         1;
uint8_t CSV_DEBUG =          0;

// **** * * * * * * * * * * * * *     *      *     *     *    *   Config Variables

#define NODEID              MASTER 
#define BITRATE             BITRATE_19k2

#define WAIT_FOR_ACK_SL           200
#define WAIT_FOR_ACK_TX           200
#define SEND_RETRIES              0
#define SEND_MSG_ATTEMPTS         0     // these are actually number of retries
#define SEND_MSG_ATTEMPTS_SL      3
//#define MINIMUM_MSG_LENGTH        10  -- no longer used
#define PACKET_SIZE               16
#define WAIT_FOR_RESPONSE         SEND_MSG_ATTEMPTS *WAIT_FOR_ACK_SL +50
//#define DELAY_FAILED_PACKET       100   -- no longer used


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
char *MSG_TX_RSSI;   // [20];

char Letter[10];
//char Packet[RF69_MAX_DATA_LEN];
char ABC, STATE, STM;
int NUM;
char buff[100];
char temp[20];
uint8_t buff_s;
char sequence[PACKET_SIZE];                       
int iki = 0;
uint8_t endloop = 0;
uint8_t packet_successful = 0;
long time_begin_transmit, time_to_ack, time_to_response, time_begin;
int Tx_timeouts, Rx_timeouts;
uint16_t stat_attmpts[SEND_MSG_ATTEMPTS+1];

int p_index, p_length;
//#define MAX_PACKET
char RSP_RCV_COUNT[PACKET_SIZE];
char RSP_ATT_COUNT[PACKET_SIZE];
char RSP_RSSI[PACKET_SIZE];
char RSP_MSG_LEN[PACKET_SIZE];
char RSP_MSG_DATA[PACKET_SIZE];
//char RSP_VAR1[20];

// **** Stat vars
long s_tx_rssi, s_rx_rssi;
int avrg_count;
int s_tx_rssi_min, s_tx_rssi_max, s_rx_rssi_min, s_rx_rssi_max;
int var1;
long time_end;
float finger;

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
  sprintf(buff, "\nNode ID -   %d\nListening at %d Mhz...", NODEID,FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
    Serial.println(buff);
  sprintf(buff,"Bite-rate set to %d", BITRATE);
    Serial.println(buff);
  Serial.flush();
  pinMode(LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_INT, handleButton, FALLING);
  
  
  STATE = 't';
  initialise_sequence('A','J');
  
  if(NODEID==MASTER){
    buff_s = sprintf(buff,"##tt##1234567890");
    if(radio.sendWithRetry(SLAVE, buff, buff_s, 3, WAIT_FOR_ACK_TX ) )
    {
      Serial.println("Test initialised, SLAVE reset.");
    }
    else
      Serial.println("RF test request not ACKnowledged");
  }

  /*    sprintf(buff,"%d,%s,%c,%d,%ld,%s,%s,%c,%s,%d,%ld,%ld",
                   test_length,sequence,packet_successful?'A':'F',counter,time_to_ack, 
                    RSP_RSSI,RSP_RCV_COUNT,packets_equal?'M':'E',RSP_ATT_COUNT,MSG_RSSI,
                     time_to_response, millis()   );    */
                     
  //sprintf(buff,"CSV DATA FORMAT\nID, chars, matching, n_ACK, t_tack, Tx Rssi, rcv_count, n_SL_ACK, Rx RSSI, t_resp ");
  sprintf(buff,"CSV DATA FORMAT\nID, sequence, Ackwledged, Ack Retries, T_Ack,Tx RSSI, ...");
    Serial.println(buff);
  sprintf(buff,"\t ... Rcv count, Matching?, Rsp Ack attmpts, Rsp RSSI, t Resp, t millis");
    Serial.println(buff);


  // Debug
  /*
  Letter[0] = '1';
  Letter[1] = '4';
  Letter[2] = '2';
  Letter[3] = '9';
  Letter[4] = '\0';
  
  sprintf(buff,"String:  %s", Letter);
  Serial.println(buff);
  sprintf(buff,"int result:  %d", string_to_int(Letter) );
  Serial.println(buff);
  sprintf(buff,"mathable like so:  %d", 2*string_to_int(Letter) );
  Serial.println(buff);
  STATE = 'i';
  sprintf(buff,"char %c to digit %d.\nagain %c to %d.", '9', char_to_digit('9'),'0', char_to_digit('0') );
  Serial.println(buff);
  */
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
int p;
uint8_t packets_equal;

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
      if (radio.ACKRequested()){
        radio.sendACK();      
        if(DEBUG_OUTPUT)  Serial.println("\t-- ACK Sent");
      }
      else
        if(DEBUG_OUTPUT)  Serial.println("\t-- NO ACK requested");

      
      if(DEBUG_OUTPUT)  Serial.println("[RX] Receiving");
      rcv_count++;
      
      MSG_SENDER_ID = radio.SENDERID;
      //MSG_TARGET_ID = NODEID;
      MSG_RSSI = radio.RSSI;
      MSG_LEN = radio.DATALEN;

      if(DEBUG_OUTPUT)  Serial.print("[MSG]\t");
      for(int i=0; i<MSG_LEN; i++){
        MSG_DATA[i] = radio.DATA[i];
        if(DEBUG_OUTPUT)  Serial.print( char(MSG_DATA[i]) );       
      }
      
      if(DEBUG_OUTPUT)  {
        sprintf(buff,"\n[id:%d]",MSG_SENDER_ID);
        Serial.print(buff);
        sprintf(buff,"\t[rssi:%d]",MSG_RSSI);
        Serial.print(buff);
        sprintf(buff,"\t[cnt:%d]",rcv_count);
        Serial.print(buff);
        sprintf(buff,"\t[len:%d]",MSG_LEN);
        Serial.println(buff);
      }
      
      

      if( MSG_DATA[0]==int('#') && MSG_DATA[1]==int('#') && MSG_DATA[4]==int('#') && MSG_DATA[5]==int('#') )
      {
        // ****   initialise test run by transmitting msg [ # # X Y # # # P Q ... ]
        rcv_count = 0;
        if( char(MSG_DATA[2]) == 't' || char(MSG_DATA[3]) == 't' )
          STATE = 't';
        if(DEBUG_OUTPUT)    Serial.println("[Rx] Master reset test sequence byte received");
        // Read parameters from Frame?
      }
      else if(STATE=='t')
      {
          if(LEDSTATE==LOW)
            LEDSTATE=HIGH;
          else LEDSTATE=LOW;
          digitalWrite(LED, LEDSTATE);
          // ****     Running in test transmit loop
          
          
          // [  count of received message, response attempt counter, TxRssi, MSG_LEN,  16 * Char ]
          counter = 0;
          buff_s = sprintf(buff,"[%d,%d,%d,%d,%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c]",
               rcv_count, counter, MSG_RSSI, MSG_LEN,
               MSG_DATA[0],MSG_DATA[1],MSG_DATA[2],MSG_DATA[3],
               MSG_DATA[4],MSG_DATA[5],MSG_DATA[6],MSG_DATA[7],
               MSG_DATA[8],MSG_DATA[9],MSG_DATA[10],MSG_DATA[11],
               MSG_DATA[12],MSG_DATA[13],MSG_DATA[14],MSG_DATA[15]
                );
    
          if(DEBUG_OUTPUT)  {
            Serial.print("\n[TX] Response frame:\t  buffer size: "); Serial.println(buff_s);           }
            
          //Serial.println(buff);
          endloop = 0;

          while(!endloop)
          {
            // *** Try send response Loop
            // *
            if(radio.sendWithRetry(MSG_SENDER_ID, buff, buff_s, SEND_RETRIES, WAIT_FOR_ACK_SL ) )
            {
              // ** If Send Acknowledged
              endloop = 1;
              if(DEBUG_OUTPUT)  {
                Serial.println(buff);
                sprintf(buff,"\t-- Response ACKnowledged on %d. attempt", counter+1);
                Serial.println(buff);
              }
            }
            else
            {
              // Count failures
              if(counter >= SEND_MSG_ATTEMPTS_SL)
              {
                // If failed too many times
                if(DEBUG_OUTPUT)  Serial.println("\t[Tx]  --Response FAILED!");
                endloop = 1;
                radio.receiveDone();  // Put radio in Rx mode immediately
              }
              else
              {
                // Count After (instead of) loop, counter = number of RETRIES
                counter++;
                
                // *** Repeat
                if(DEBUG_OUTPUT)  Serial.println("\t-- NOT Acknl.  Repeating Response:");
                buff_s = sprintf(buff,"[%d,%d,%d,%d,%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c]",
                   rcv_count, counter, MSG_RSSI, MSG_LEN,
                   MSG_DATA[0],MSG_DATA[1],MSG_DATA[2],MSG_DATA[3],
                   MSG_DATA[4],MSG_DATA[5],MSG_DATA[6],MSG_DATA[7],
                   MSG_DATA[8],MSG_DATA[9],MSG_DATA[10],MSG_DATA[11],
                   MSG_DATA[12],MSG_DATA[13],MSG_DATA[14],MSG_DATA[15]
                    );
                } //Eo else
              
            } // Eo if send failed
            //counter++;
            // Exit --> with endloop
            // ***  Eo while  
          }

          // Succeeded or failed.

          // * Countdistribution of retries
          //stat_attmpts[counter-1]++;
          
          // Eo STATE - testing
      }
      else
      {

            // ****   idle
            // save power when just waiting
            sprintf(buff, "Slave in Idle mode, whats this msg?");
            Serial.println(buff);
            
            //STATE = 'i';
            Serial.flush();
            radio.receiveDone();
            LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON); 
          
      }
      
      //Serial.flush();
      
      // ********************    Eo if Receive
    }
    else{
        // ***** * * * * *  If no data received
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

    // ** Slave loop
    // *
    Serial.flush();
    radio.receiveDone(); //put radio in RX mode
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_ON); 
    
    // *********************** Eo SLAVE If
  }
  
  if(NODEID==MASTER)
  {
    /* ************************** * **  * * * * * * *  * * *  MASTER Code    *    *    *   *   *  *
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
     *  ************************** * * * * * * * * * * * 
     */
    
    ABC = Serial.read();
    
    if( ABC != -1 ){
      Serial.print("Reading serial comm:\t");
      //Serial.print(NUM);
      //Serial.print("\t");
      //Serial.print( char(NUM) );
     // ABC = char(NUM);
      //ABC = 't';

      switch(ABC){
        case 't':
          sprintf(buff,"CMD : %c\t--Begin test sequence", ABC);
          Serial.println(buff);
          STATE = 't';
          initialise_sequence('A', 'Z' );
          delay(1000);
          time_begin = millis();
        break;
        case 's':
          sprintf(buff,"CMD : %c\t--Re-SET test sequence", ABC);
          Serial.println(buff);
          STATE = 't';
          initialise_sequence(Serial.read(), Serial.read() );
          delay(1000);
          time_begin = millis();
        break;
        case 'd':
           DEBUG_OUTPUT = char_to_digit( Serial.read() );
           CSV_DEBUG = DEBUG_OUTPUT;
        break;
        case 'c':
          sprintf(buff,"CMD : %c\t--Begin continuous", ABC);
          Serial.println(buff);
          STATE = 'c';
          initialise_sequence('a', '~');
          delay(1000);
          time_begin = millis();
        break;
        case 'h':
          Serial.println("Hello");
          
        break;
        case 'i':
          sprintf(buff,"CMD : %c\t--Idle mode.", ABC);
          Serial.println(buff);
          STATE = 'i';
          
        break;
        default:
          Serial.print("Serial: what?\n");  
          
        break;
         
      }   // *** Serial CMD Switch
        
        // Eo if Serial read cmd
      }
    
    //Serial.print(STATE);
      
    if(STATE=='t' || STATE=='s' || STATE=='c')
    {
      if(CSV_DEBUG){
        sprintf(buff,"begin of frame: %ld",millis() );
        Serial.println(buff);      }
        
      //Serial.print("t");
      if(DEBUG_OUTPUT)  {
        sprintf(buff,"[TX] New packet %d.",test_length);
        Serial.print(buff);
      }

      // Variables for the send-Packet-loop
      counter = 0;
      endloop = 0;
      
      time_begin_transmit = millis();
      
      // ***** Send package with retries and timeout  
      while(!endloop)
      {
        if(radio.sendWithRetry(SLAVE, sequence, PACKET_SIZE, SEND_RETRIES, WAIT_FOR_ACK_TX ) )  
        {
          
          time_to_ack = millis() -time_begin_transmit;
          radio.receiveDone();    // Put Master into Rx mode
          endloop = 1;
          packet_successful = 1;
          
          if(DEBUG_OUTPUT || CSV_DEBUG)  {
            sprintf(buff, "\t- ACK Y! w %d retries", counter);
            Serial.print(buff);
            sprintf(buff, "   d T :  %ld ms after transm.", time_to_ack );
            Serial.println(buff);
          }
          // *** Eo successful transmit
        }
        if(!endloop)  // if NOT successful, check counter
          if(counter >= SEND_MSG_ATTEMPTS){
            time_to_ack = millis() -time_begin_transmit; // 0;
            
            if(CSV_DEBUG)  {
              sprintf(buff,"[Fail] Millis: %ld, Tx time: %ld,  failed: %ld",millis(), time_begin_transmit, time_to_ack);
              Serial.println(buff);
            }
            
            endloop = 1;
            packet_successful = 0;
            Tx_timeouts++;
            if(DEBUG_OUTPUT || CSV_DEBUG)  {
              sprintf(buff,"[Tx] --Failed %d attmpts, \tTotal %d.", counter, Tx_timeouts);
              Serial.println(buff);
              }
            
            counter++;
          } // Eo -- Send msg attempts

          // Count at END of loop, counter = Number of RETRIES
         
        // exit loop --> endloop 
        // * Eo while Tx loop
        counter++;
      }

      stat_attmpts[counter-1]++;
      
      /*buff_s = sprintf(buff,"Pkt: [%c%c%c%c:%c%c%c%c:%c%c%c%c:%c%c%c%c]",
               MSG_DATA[0],MSG_DATA[1],MSG_DATA[2],MSG_DATA[3],
               MSG_DATA[4],MSG_DATA[5],MSG_DATA[6],MSG_DATA[7],
               MSG_DATA[8],MSG_DATA[9],MSG_DATA[10],MSG_DATA[11],
               MSG_DATA[12],MSG_DATA[13],MSG_DATA[14],MSG_DATA[15]
                );        */
      //sequence[16] = '\0';
      if(DEBUG_OUTPUT)  {
        Serial.print("  [Packet: ");
        for(int a=0;a<PACKET_SIZE;a++){
          Serial.print(sequence[a]);
        }
        Serial.println(" ]");
      }
      //sprintf(buff, "[Packet: %s ]\n", sequence);
      //Serial.print(buff);
      //Serial.println(sequence);
      
      
      // ************* Packet Acknowledged
      // *
      // *
      if(!packet_successful){
        // ** Failed Packet
        if(DEBUG_OUTPUT)  Serial.println("[TX] ### Transmit failed");
        
        // Send report of failed pack
        
        RSP_RSSI[0] = '0';
        RSP_RSSI[1] = '\0';
        //RSP_RCV_COUNT - leave as before
        RSP_ATT_COUNT[0] = '0';
        RSP_ATT_COUNT[1] = '\0';
        sequence[PACKET_SIZE]='\0';
        RSP_RSSI[0]='0';
        RSP_RSSI[1]='\0';
        RSP_RCV_COUNT[0]='0';
        RSP_RCV_COUNT[1]='\0';
        packets_equal = 0;
        RSP_ATT_COUNT[0] = '0';
        RSP_ATT_COUNT[1] = '\0';
        MSG_RSSI=0;
        time_to_response = 0;
        
        if(CSV_OUTPUT){
            sprintf(buff,"%d,%s,%c,%d,%ld,%s,%s,%c,%s,%d,%ld,%ld",
                   test_length,sequence,packet_successful?'A':'F',counter,time_to_ack, 
                    RSP_RSSI,RSP_RCV_COUNT,packets_equal?'M':'E',RSP_ATT_COUNT,MSG_RSSI,
                     time_to_response, millis()-time_begin   );
            Serial.println(buff);
        }
        
        //delay(DELAY_FAILED_PACKET);
        // *** Eo unsuccessful packet routine
        Serial.flush();
        //radio.receiveDone();  // why?? 
        //LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_ON); 
      }
      else
      {
          // ***** Paket sent successfull
          // *    > wait for response
          endloop = 0;
          
          // ** Wait for Response loop
          // *
          while( !endloop )  {
            
            if(radio.receiveDone() )
            {
              time_to_response = millis() -time_begin_transmit -time_to_ack;
              
              // *** If received something
              // *
              if(DEBUG_OUTPUT)  Serial.println("[RX] Packet Response ");
              
              endloop = 1;
              MSG_SENDER_ID = radio.SENDERID;
              MSG_RSSI = radio.RSSI;
              MSG_LEN = radio.DATALEN;
              
              if(DEBUG_OUTPUT)  Serial.print(" [Rx Msg]\t"); 
              
              for(int i=0;i<MSG_LEN;i++)
              {
                MSG_DATA[i] = radio.DATA[i];
                if(DEBUG_OUTPUT )  Serial.print( char(MSG_DATA[i]) );    //!   
              }
              if(DEBUG_OUTPUT )  Serial.println();//!
              
              if (radio.ACKRequested()){
                radio.sendACK();      
                if(DEBUG_OUTPUT)   Serial.println("\t-- ACK Sent");
              }


              // ########################### Unpack
               
              // read var 1
              p_index = 1;
              p_length = 0;
              
              while(char(MSG_DATA[p_index])!=',' && p_index<MSG_LEN)              {
                RSP_RCV_COUNT[p_length] = char(MSG_DATA[p_index]);
                p_index++;               p_length++;
              }
              
              p_index++;
              RSP_RCV_COUNT[p_length]='\0';
              p_length=0;
              
              while(char(MSG_DATA[p_index])!=',' && p_index<MSG_LEN)              {
                RSP_ATT_COUNT[p_length] = char(MSG_DATA[p_index]);
                p_index++;                p_length++;
              }
              
              p_index++;
              RSP_ATT_COUNT[p_length]='\0';
              p_length=0;
              //RSP_VAR3="";
              
              while(char(MSG_DATA[p_index])!=',' && p_index<MSG_LEN)              {
                RSP_RSSI[p_length] = char(MSG_DATA[p_index]);
                p_index++;                p_length++;
              }

              p_index++;
              RSP_RSSI[p_length]='\0';
              p_length=0;
              
              while(char(MSG_DATA[p_index])!=',' && p_index<MSG_LEN)              {
                RSP_MSG_LEN[p_length] = char(MSG_DATA[p_index]);
                p_index++;               p_length++;
              }

              p_index++;
              RSP_MSG_LEN[p_length]='\0';
              p_length=0;

              while(char(MSG_DATA[p_index])!=']' && p_index<MSG_LEN)              {
                RSP_MSG_DATA[p_length] = char(MSG_DATA[p_index]);
                p_index++;               p_length++;
              }
              
              RSP_MSG_DATA[p_length]='\0';
              //p_index++;
              //p_length=0;
              
              if(DEBUG_OUTPUT )  Serial.print("\n");
              
              if(DEBUG_OUTPUT)  
              {
                sprintf(buff,"\n  [id:%d]",MSG_SENDER_ID);
                Serial.print(buff);
                sprintf(buff,"\t[Rxrssi:%d]",MSG_RSSI);
                Serial.print(buff);
                sprintf(buff,"\t[sequ:%d]",test_length);
                Serial.print(buff);
                sprintf(buff,"\t[len:%d]",MSG_LEN);
                Serial.println(buff);
              }

              // ******** * * * * * * * Print successful packet CSV
              if(DEBUG_OUTPUT){
                sprintf(buff,"Unpacked response\n Rcv cnt:%s ,Ack Att:%s, Rx_RSSI:%s, Rsp Len:%s, Rsp Data:%s",
                          RSP_RCV_COUNT,RSP_ATT_COUNT,RSP_RSSI,RSP_MSG_LEN,RSP_MSG_DATA);
                Serial.println(buff);
              }
              if(DEBUG_OUTPUT){
                sprintf(buff,"Tx Rssi: %d\t Rx Rssi: %d", var1, MSG_RSSI);
                Serial.println(buff);
              }
                

              // * quick point average
              if(CSV_OUTPUT){
                
                // add to average
                s_rx_rssi += MSG_RSSI;
                var1 = string_to_int(RSP_RSSI);
                s_tx_rssi += var1;
                avrg_count++;
                
                // get mins & maxs
                if(s_rx_rssi_min==0)                  s_rx_rssi_min = MSG_RSSI;
                if(s_rx_rssi_max==0)                  s_rx_rssi_max = MSG_RSSI;
                if(s_tx_rssi_min==0)                  s_tx_rssi_min = var1;
                if(s_tx_rssi_max==0)                  s_tx_rssi_max = var1;
                
                // update mins & max
                //if(MSG_RSSI!=0)
                if(MSG_RSSI<s_rx_rssi_min)   {
                  s_rx_rssi_min = MSG_RSSI;
                }
                if(MSG_RSSI>s_rx_rssi_max) {
                  s_rx_rssi_max = MSG_RSSI;
                }
                
                //if(var1!=0)  
                  if(var1<s_tx_rssi_min){
                    s_tx_rssi_min = var1;
                  }
                if(var1>s_tx_rssi_max)  {
                  s_tx_rssi_max = var1;
                }
                
                //  s_tx_rssi, s_tx_rssi_min, s_tx_rssi_max;
                //  s_rx_rssi, s_rx_rssi_min, s_rx_rssi_max;
              }
              if(CSV_OUTPUT){
                
                /* Print format
                 *  Sequence ID, Packet chars, ACK attempts, time to ack, Tx RSSI (slave), 
                 *    ... slave rcv count, slave resp attempts, Rx RSSI (master), time to response
                 */
                
                sequence[16]='\0';

                /*  char RSP_RCV_COUNT[20]; var1
                  char RSP_ATT_COUNT[20]; var2 
                  char RSP_RSSI[20];      var3
                  char RSP_MSG_LEN[20];   var4
                  char RSP_MSG_DATA[20];  variables5
               */
               
                p_index=0;
                endloop=1;  // used as boolean to show messages are equal
                // equal until found a difference
                packets_equal = 1;
                
                while(endloop==1 && p_index<PACKET_SIZE){
                  
                  // if any of the two is too short
                  if(MSG_DATA[p_index]=='\0' || RSP_MSG_DATA[p_index]=='\0'){
                    packets_equal = 0;
                    endloop = 0;                  }

                  // if not identical
                  //if(char(MSG_DATA[p_index]) != RSP_MSG_DATA[p_index])
                  if(sequence[p_index] != RSP_MSG_DATA[p_index])
                  {   endloop = 0;
                      packets_equal = 0;          }
                  
                  p_index++;
                  // Eo while loop
                }

                sprintf(buff,"%d,%s,%c,%d,%ld,%s,%s,%c,%s,%d,%ld,%ld",
                   test_length,sequence,packet_successful?'A':'F',counter,time_to_ack, 
                    RSP_RSSI,RSP_RCV_COUNT,packets_equal?'M':'E',RSP_ATT_COUNT,MSG_RSSI,
                     time_to_response, millis()-time_begin  );
                Serial.println(buff);
            
                /*  sprintf(buff,"%d,%s,%d,%d,%ld,%s,%s,%s,%d,%ld,%ld",
                           test_length,sequence,packets_equal,counter,time_to_ack, 
                            RSP_RSSI,RSP_RCV_COUNT,RSP_ATT_COUNT,MSG_RSSI,
                             time_to_response,millis()   );
                Serial.println(buff);           */
                
                endloop=1;
                
              } // Eo CSV Output

              
                // Eo received packet
              }
            else
            {
              // If not received a packet
              // How much time has passed since acknowledgement
              time_to_response = millis() -time_begin_transmit -time_to_ack;
              
              if(time_to_response >= WAIT_FOR_RESPONSE) {
                // *** Timeout while waiting for response (Dispite Acknowledgement) 
                // * print Tx msg details
                Rx_timeouts++;
                endloop = 1;

                packets_equal = 0;
                RSP_RSSI[0] = '0';
                RSP_RSSI[1] = '\0';
                //RSP_RCV_COUNT - leave as before
                RSP_ATT_COUNT[0] = '0';
                RSP_ATT_COUNT[1] = '\0';
                
                if(DEBUG_OUTPUT)  
                {
                  sprintf(buff,"\n[Rx] --Failed #Response Timeout. No of timeouts:  %d", Rx_timeouts);
                  Serial.println(buff);
                }

                if(CSV_OUTPUT){
                    sprintf(buff,"%d,%s,%c,%d,%ld,%s,%s,%c,%s,%d,%ld,%ld",
                         test_length,sequence,packet_successful?'A':'F',counter,time_to_ack, 
                          RSP_RSSI,RSP_RCV_COUNT,packets_equal?'M':'E',RSP_ATT_COUNT,MSG_RSSI,
                           time_to_response, millis()-time_begin   );
                    Serial.println(buff);
                    
                   /* sprintf(buff,"%d,%s,%d,%d,%ld,%s,%s,%s,%d,%ld,%ld",
                           test_length,sequence,packets_equal,counter,time_to_ack, 
                            RSP_RSSI,RSP_RCV_COUNT,RSP_ATT_COUNT,MSG_RSSI,
                             time_to_response, millis()   );
                    Serial.println(buff);   */
                }
                //delay(DELAY_FAILED_PACKET);
                Serial.flush();
                //radio.receiveDone();
                //LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_ON); 
              }
                
            } // Eo - else
            
          }   // ***** * *  Eo Receive Response Loop

          
          // ***Eo Successful packet routine
      }

      // Modify package for next frame
      sequence[iki] += 0x01;
      
      if(sequence[iki]=='[')
        sequence[iki] = 'a';

      if(sequence[iki]=='|')
        sequence[iki] = 'a';

      iki++;
      if(iki>=PACKET_SIZE)
        iki=0;
      
      test_length++;
      
      if( sequence[12]==end_char && sequence[13]==end_char && sequence[14]==end_char && sequence[15]==end_char )
      {
        // If End of Sequance packet
        time_end = millis() -time_begin;
        
        STATE = 'i';    // Change state
        
        // Reset sequence
        /*for(int a=0; a<PACKET_SIZE; a++){
          sequence[a] = begin_char;
        } */
        sprintf(buff,"CSV DATA FORMAT\nID, sequence, Ackwledged, Ack Retries, T_Ack,Tx RSSI, ...");
          Serial.println(buff);
        sprintf(buff,"\t... Rcv count, Matching?, Rsp Ack retries, Rsp RSSI, t Resp, t millis");
          Serial.println(buff);
        
        s_tx_rssi = int( s_tx_rssi/avrg_count );
        s_rx_rssi = int( s_rx_rssi/avrg_count );
        sprintf(buff,"Reached end of sequence.\nErrors:\nRx timeouts:  ");
          Serial.print(buff);
        sprintf(buff,"%d,\tTx timeouts: %d", Rx_timeouts, Tx_timeouts);
          Serial.println(buff);
        sprintf(buff,"Tx RSSO Avrg: %ld\tmin: %d, max: %d", s_tx_rssi, s_tx_rssi_min, s_tx_rssi_max );
          Serial.println(buff);
        sprintf(buff,"Rx RSSO Avrg: %ld\tmin: %d, max: %d", s_rx_rssi, s_rx_rssi_min, s_rx_rssi_max );
          Serial.println(buff);
        
        finger = (1.0*time_end) / test_length;
        sprintf(buff,"Sequence of %d values, %ld ms,  > ms per packet: ", test_length, time_end);
          Serial.print(buff);
          Serial.println(finger );

        sprintf(buff, "Test result CSV :\ntest length, Tx_timeouts,");
          Serial.print(buff);
        sprintf(buff, "Tx RSSI min, average, max, Rx_timeouts,");
          Serial.print(buff);
        sprintf(buff, "Rx RSSI min, average, max, ");
          Serial.println(buff);
          
        sprintf(buff,"%d,%d,%d,%ld,%d,%d,%d,%ld,%d,%ld",
              test_length, Tx_timeouts, s_tx_rssi_min, s_tx_rssi, s_tx_rssi_max, 
               Rx_timeouts, s_rx_rssi_min, s_rx_rssi, s_rx_rssi_max, 
                time_end  );
          Serial.print(buff);

        
        Serial.print("\n");
        for(int a=0; a<SEND_MSG_ATTEMPTS+2;a++)
        {
          sprintf(buff,"%d, ",stat_attmpts[a]);
          Serial.print(buff);
        }
        Serial.print("\n");
        
          //sprintf(buff,"%ld,", );
          //Serial.println(buff);
            //  s_tx_rssi, s_tx_rssi_min, s_tx_rssi_max;
            //  s_rx_rssi, s_rx_rssi_min, s_rx_rssi_max;
          
        // Eo if End sequence
      }
      // **** Eo     Test mode
    }
    else{
      // **** If not in test sequence mode
      // be idle, read serial every 2 s
      Serial.flush();
      radio.receiveDone();
      //LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_ON); 
      LowPower.idle(SLEEP_2S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_OFF);
    }
    //
     
    // *************** Eo Master If
  }
  
  //Eo *************************************      * * * * * * * * * * * * *    LOOP
}

void initialise_sequence( char first_char, char last_char){
    begin_char=first_char;
    end_char=last_char;
    iki=0;
    test_length = 0;
    rcv_count = 0;
    Rx_timeouts = 0;
    Tx_timeouts = 0;
    for(int a=0; a<PACKET_SIZE; a++)
    {          sequence[a] = begin_char;         }
    for(int a=0; a<SEND_MSG_ATTEMPTS+2;a++)
    {           stat_attmpts[a]= 0;     }
    avrg_count = 0;
    s_tx_rssi = 0;
    s_tx_rssi_min = 0;
    s_tx_rssi_max = 0;
    s_rx_rssi = 0;
    s_rx_rssi_min = 0;
    s_rx_rssi_max = 0;
}

    
  
  
          
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



/* 
 *  #################################################################################
 */
              
