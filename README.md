PCom Assignment 2: TCP-UDP client messaging system
Stancu David-Ioan 322CA

This project implement a client messaging system between TCP and UDP using C++ 
socket coding. This project has been implemented to further assimilate the 
information and get us familiar with implementing a TCP-based subscriber connecting 
to a central broker server to subscribe to certain topics sent in by UDP publishers.
The system implements message sending, subscribing to desired topics, pattern 
matching to said topics, wilcards and decding of the messages.


I. The common module - common.h/.cpp
- standard utility file that contains various functions used by other files
- containt the logic for message decoding, topic matching and tcp message 
intentification

Functions/structures used:
* UDPMessage: standard UDP message strcture, with the topic name, type of data and 
actual payload
* decode_message: converts the UDP message and its address into a string and handles 
the formatting of the messages
* write_all: sends a buffer to a file descriptor, in case of partial writes
* send_packet: sends a string to a TCP socket
* extract_message: extracts a complete message from a buffer
* split_levels: small function that just splits topics into levels
* topic_matches: checks if a topic matches a description pattern


II. The Subscriber file - subscriber.cpp
- represents the TCP client that connects to the server and allows the user to send
commands from the terminal, such as sunscribe/unsubscribe from topics. it uses poll
as a method of listening to the input and the TCP socket

Behaviour:
* sends client id as a first message
* listens to commands suck ass subscribe or unsubscribe
* prints incoming messages from the server
* it receives the messages in a loop using extract_message to parse framed messages

III. The server file - server.cpp
- reprezents the broker between the client and the publisher, listening to both TCP and UDP
- uses pol to manage multiple TCP and UDP connections as well as the input

Behaviour:
* accepts TCP connections and registers new clients based on their IDs
* handles incoming messages from TCP clients (cliend based commands such as 
un/subscribe or exit)
* receives udp messages, decodes them, then sends tehm to the matching TCP clients
* manages the lis of topics that each client sunscribed to

Why 'poll'?
I have used poll due to the fact that i can manage and monitor multile clients/
sockets/input. it allows me to get rid of blocking calls that i ve dealt with uding
'select'. Given its scalability, i also dont need t limit the number of clients!
