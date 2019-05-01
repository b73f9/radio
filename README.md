# Overview
This is a fairly simple network radio - data is ingested / outputted via stdin / stdout.

Communication happens mostly via multicast packets. 

# Transmitter
Sends the ingested data to a multicast address in packets of predetermined size.

Also, listens on CTRL_PORT for two types of packets:
 1. LOOKUP - request for information about the transmitter (name, multicast info)
 2. REXMIT - request for packet retransmission

Data retransmissions happen every RTIME seconds (requests are collected between the retransmission windows).

Example usage: ``sox -S "song.mp3" -r 44100 -b 16 -e signed-integer -c 2 -t raw - repeat - | pv -q -L $((44100*4)) | ./radio-sender -a 239.10.11.20 -n "Radio name here"``

# Receiver
Receives the broadcast data, sends retransmission requests and provides a telnet user interface 
(so that the user can choose which station they want to listen to). By default, the telnet interface is available on port 11111.

To figure out what stations are available, the receiver sends a LOOKUP broadcast every 5 seconds. 

Example usage: `` ./radio-receiver | play -t raw -c 2 -r 44100 -b 16 -e signed-integer --buffer 32768 -``

