# RFM-tester

Platform for testing RFM69 based modules with atmega328 based microcontrollers

V1 to 5 were saved during development to remain executable

# V6
However lets focus on v6

Its just one code for  Master and slave and needs no Hardware.

### Definitions
Main adjustments need to made to #DEFINITION variables

## Slave
Select NODEID SLAVE for the slave
and you can turn off all the ....._OUTPUT variables as I set up the slave to just echo messages.
It is reset by the master at the beginnning of a test sequence so only battery power is needed

## Master
Select NODEID MASTER for the master
and SET CSV_OUTPUT to get a .CSV output on the serial monitor.
BAUD 115200

Additionally Set DEBUG_OUTPUT for more info on sent and received packages
it lists Packages sent, Acknowledgments send and received 

CSV_DEBUG gives some printouts on timings of packets sent,recevied and waiting for response

I mean there's comments in the code, but Output of the CSV list is

Sequence ID, Packet characterss, boolean - SENT==RECEIVED, Master send (& ACK) attempts, time to Ack (from first transmit), Tx RSSI (received by slave), slave receive count, slave resp (& Ack) attempts, Rx RSSI (at master), time to response (from transmit)

Reading commands form serial doesnt work atm so just push the button on the Master to RESET and start a new sequence.

.