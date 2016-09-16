# RFM-tester

Platform for testing RFM69 based modules with atmega328 based microcontrollers

V1 to 5 were saved during development to remain executable

# QUICKSTART
The saved settings run fine, just change NODEIO to SLAVE for the SLAVE, and MASTER for the MASTER.
Power the SLAVE with a 3.3V battery adn the MASTER over Serial tty cable to read the output.
The slave will turn the onboard LED for every packet
on startup the MASTER starts a sequence of 144 values from AAA... to JJJ...

write Serial commands 
i - to pause at any time
t - to run the standard test sequence of 400 Values A to Z
d - to turn DEBUG ON or OFF during runtime, without programming by 'd1' or 'd0'
s - to run a sequence of custom length saz runs from 'aaa...' to 'zzz...'
c - to run continuously when you want to walk around with a node and observe signal strength and packet reception in realtime


# V7
V6 Was the first version that was working but there are quite a few additions in V7

Its just one code for  Master and slave and needs no buttons or LEDs attached

### Definitions
Main adjustments need to/ can be made to #DEFINITION variables

## Slave
Select NODEID SLAVE for the slave
and you can turn off all the ....._OUTPUT variables as I set up the slave to just echo messages.
It is reset by the master at the beginnning of a test sequence (through a specific message code) so only battery power is needed.
The on-board LED on the SLAVE is latched on every packet received.

## Master
Select NODEID MASTER for the master
and SET CSV_OUTPUT to get a .CSV output on the serial monitor.
Serial BAUD 115200

Additionally Set DEBUG_OUTPUT for more info on sent and received packages
it lists Packages sent, Acknowledgments send and received 
You can switch DEBUG ON or OFF now during runtime by sending d1 & d0 + enter over serial

CSV_DEBUG gives some printouts on timings of packets sent,recevied and waiting for response

# #Defines
SEND RETRIES - variable given to the RFM69> library sendWithRetry function. is set to 0 as we are counting and handling retries ourself

WAIT_FOR_ACK_TX - Time MASTER waits for ACK of transmitted msg, passed to sendWithRetry

WAIT_FOR_ACK_SL - same variable for SLAVE

SEND_MSG_ATTEMPTS - Master retries for sending each packet. = 1, means +1 retry = 2 in total

SEND_MSG_ATTMPTS_SL - same on the Slave side for its response, should be equal really.

PACKET_SIZE - characters of packet, could technically by changed flexible but a few functions handling the strings and chars would nee to be modified

WAIT_FOR_RESPONSE - the time the MASTER waits for a response from the slave, after a sent package is acknowledged.
					 according to program flow (illustrated in Statemachine charts)  this should be long enough to include all SEND MSG attempts of the slave plus the respective WAIT FOR ACK

FREQUENCY - of the RF module should not be changed

#BITRATE
Tests on lowering bitrate were inconclusive
I followed this thread for a Multistorey carpark[ https://lowpowerlab.com/forum/moteino/multistorey-indoor-range-performance-1-75km-range-(solution)/ ] 
on lowering bitrate successfully to 19kHz (the default is 55kHz)
With register settings by [Mr Felix himself] [ https://lowpowerlab.com/forum/moteino/long-range-low-bitrate-library-config/msg1137/#msg1137 ]

The multistorey carpark also discusses going down to 1.9kHz with discussion of [this thread]( https://lowpowerlab.com/forum/rf-range-antennas-rfm69-library/rfm69hw-range-test!/msg2664/#msg2664 )
and offers successful register settings.
The work but for me don't successfully, increase range
( I blame the antennas )

I did go through the datasheet and each register and double-checked the dependencies of these settings.
As felix desicusses in one of these posts the WAIT_FOR_ACK time mus tbe adjusted accordingly but he states 200ms should be the maximum needed. The multistorey thread states 200ms as successful value to receive all packets.

I added extra functions in the <RFM69.h> library for initialising the registers for these settings according to values discussed above and results work. 
The #BITRATE can thus be defined as BITRATE_55k5 , etc. according to the defined values and the program will initialise in the selected bitrate.




#CSV_OUTPUT
Is the standard output this test provides.
This will give you a CSV output for every packet, with details of:

packet ID, Packet characterss, boolean - SENT==RECEIVED?, Master send (& ACK) attempts, time to Ack (from first transmit), Tx RSSI (received by slave), slave receive count, slave resp (& Ack) attempts, Rx RSSI (at master), time to response (from transmit), time of total sequence

coming from variables and types as follows:
sprintf(buff,"%d,%s,%c,%d,%ld,%s,%s,%c,%s,%d,%ld,%ld",
  test_length,sequence,packet_successful?'A':'F',counter,time_to_ack, 
  RSP_RSSI,RSP_RCV_COUNT,packets_equal?'M':'E',RSP_ATT_COUNT,MSG_RSSI,
  time_to_response, millis()-time_begin   );

##In Detail
packet ID - integer counting linearly the number of packets sent
Packet characters - actual chars of packet from AAAA to BAAA to BBAA etc. steppign one char at a time
boolean - is the packet SENT==RECEIVED?
Master send (& ACK) attempts - how many send attempts were made until a msg is acknowledged, limited by SEND_MSG_ATTEMPTS
time to Ack  - from first transmit, in ms
Tx RSSI (received by slave) - Received signal strength indicater of message received by slave. (is sent back in slave response)
slave receive count - number of messages received by slave
slave resp (& Ack) attempts - number of Tx attempts by slave until ACK received
Rx RSSI (at master) - RSSI of slave message received at MASTER
time to response (from transmit) - time from transmit to successfu response
time elapsed since test begin

#End of sequence Summary
At the end of a sequence the folowing output is created::
"""
	CSV DATA FORMAT
	ID, sequence, Ackwledged, Ack Retries, T_Ack,Tx RSSI, ...
		... Rcv count, Matching?, Rsp Ack retries, Rsp RSSI, t Resp, t millis
	Reached end of sequence.
	Errors:
	Rx timeouts:  0,	Tx timeouts: 0
	Tx RSSO Avrg: -36	min: -38, max: -35
	Rx RSSO Avrg: -35	min: -36, max: -35
	Sequence of 144 values, 11195 ms,  > ms per packet: 77.74
	Test result CSV :
	test length, Tx_timeouts,Tx RSSI min, average, max, Rx_timeouts,Rx RSSI min, average, max, 
	144,0,-38,-36,-35,0,-36,-35,-35,11195
	144, 0, 0, 0, 
"""

The repeats the format of the CSV output for each packet, providing human readable summaries of timeouts and signal strengths, and speed of communicaiton (average time to successful packet response)

Followed by a 
#Test result CSV :
computer readable and easy to paste into the evaluation Excel sheet, in collumns L : test length
"""
	test length, Tx_timeouts,Tx RSSI min, average, max, Rx_timeouts,Rx RSSI min, average, max, 
	144,0,-38,-36,-35,0,-36,-35,-35,11195
	144, 0, 0, 0, 
"""
of the number of packets in this test sequence, the RSSI min, average and max values, and the time in ms this sequence took to complete
In the next line are 4 integers representing the number of occurence of packets successfully acknowledged on the 
1st, 2nd, 3rd attempt... as defined by SEND_MSG_ATTMPTS, this can be copied into Collumn V: attmps

