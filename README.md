# Overview
This is a fairly simple network radio - data is ingested / outputted via stdin / stdout.

Communication happens mostly via multicast packets. 

# Transmitter
Sends the ingested data to a multicast address in packets of predetermined size.

Also, listens on CTRL_PORT for two types of packets:
 1. LOOKUP - request for information about the transmitter (name, multicast info)
 2. REXMIT - request for packet retransmission

Data retransmissions happen every RTIME seconds (requests are collected between the retransmission windows).

# Receiver
Receives the broadcast data, sends retransmission requests and provides a telnet user interface 
(so that the user can choose which station they want to listen to).

To figure out what stations are available, the receiver sends a LOOKUP broadcast every 5 seconds. 

