#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <netinet/in.h>
#include <cstdint>

using namespace std;

//structure of a UDP message 
struct UDPMessage {
    char topic_name[50]; // topic name
    uint8_t type;   //signifies the data type
    char payload[1501];     //message payload
};

//headers bellow are used for functions which will be explained in common.cpp
string decode_message(const UDPMessage& message, const sockaddr_in& cli);
bool write_all(int fd, const void *buf, size_t len);
bool send_packet(int sockfd, const string &msg);
bool extract_message(string &buf, string &msg);
vector<string> split_levels(const string &s);
bool topic_matches(const string &pattern, const string &topic);

#endif
