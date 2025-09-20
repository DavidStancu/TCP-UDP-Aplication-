#include "common.h"
#include <iomanip>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

using namespace std;

//this function converts a UDP message to a string for the TCP client
string decode_message(const UDPMessage& message, const sockaddr_in& cli) {
	//creates initial string result
    string result;
    string ip = inet_ntoa(cli.sin_addr);
    int port = ntohs(cli.sin_port);
    string topic(message.topic_name);
    size_t null_pos = topic.find('\0');

    if (null_pos != string::npos) { //takes out null characters from topic if any
        topic = topic.substr(0, null_pos);
    }

    result = ip + ":" + to_string(port) + " - " + topic + " - ";

	//checks data type of message
    if (message.type == 0) { //if its int type
        uint8_t sign = message.payload[0];
        uint32_t val;
        memcpy(&val, message.payload + 1, 4);
        val = ntohl(val);
        int32_t int_val;

        if (sign) { //checks for sign
            int_val = -((int32_t)val);
        } else {
            int_val = (int32_t)val;
        }

        result += "INT - " + to_string(int_val);
    } else if (message.type == 1) { //if its SHORT_REAL
        uint16_t val;
        memcpy(&val, message.payload, 2);
        val = ntohs(val);
        double short_real = val / 100.0;

        char buf[32];
        snprintf(buf, sizeof(buf), "SHORT_REAL - %.2f", short_real);
        result += buf;
    } else if (message.type == 2) { //if its type float
        uint8_t sign = message.payload[0];
        uint32_t mantissa;
        memcpy(&mantissa, message.payload + 1, 4);
        mantissa = ntohl(mantissa);
        uint8_t exponent = message.payload[5];

        double val = mantissa;
		//divides mantissa to reconstruct float point numer
        for (int i = 0; i < exponent; i++) {
            val /= 10;
        }

        if (sign) { //checks for sign
            val = -val;
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "FLOAT - %.4f", val);
        result += buf;
    } else if (message.type == 3) { //if is of typs string
        result += "STRING - " + string(message.payload);
    }

    return result + "\n";
}

// function sends entire buffer through a socket in multiple calls if/when needed
bool write_all(int fd, const void *buf, size_t len) {
    const char *p = (const char*)buf;

    while (len > 0) { // loop to write all bytes
        ssize_t n = write(fd, p, len);
        if (n <= 0) { //checks tto see if the write failed
            return false;
        }

        p += n; //jumps pointer by number of bytes written
        len -= n; // decrease langth
    }

    return true;
}

//sends a mesage to a TCP socket
bool send_packet(int sockfd, const string &msg) {
	//converts to big endian
    uint32_t netlen = htonl((uint32_t)msg.size());

    bool success = write_all(sockfd, &netlen, 4);
    if (!success) //if length prefix send failed
        return false;

	//sending payload
    success = write_all(sockfd, msg.data(), msg.size());

    if (!success)// if payload send failed
        return false;

    return true;
}

// function that extracts a message from a buffer and checks 
// if the message was extracted in full
bool extract_message(string &buf, string &msg) {
	// checks if buffer has the correct length
    if (buf.size() < 4)
        return false;

	//reads first 4 bytes and converts to host byte
    uint32_t netlen;
    memcpy(&netlen, buf.data(), 4);
    uint32_t len = ntohl(netlen);

    if (buf.size() < 4 + len) // checks if buffer contains entire message
        return false;

    msg = buf.substr(4, len);
    buf.erase(0, 4 + len);

    return true;
}

//function that is used to split string in individual components
vector<string> split_levels(const string &s) {
    vector<string> v;
    stringstream ss(s);
    string item;

    while (getline(ss, item, '/')) //this while splits string ans stores into levels into v
        v.push_back(item);

    return v;
}

// function that compares topic patterns and handles wildcards
bool match_rec(const vector<string>& p, size_t pi, const vector<string>& t, size_t ti) {
    if (pi == p.size() && ti == t.size()) //if topic and pattern matches
        return true;

    if (pi == p.size()) // if pattern exhausted
        return false;

    if (p[pi] == "*") { // if * mathes any/zero levels of topic
        for (size_t k = ti; k <= t.size(); k++)
            if (match_rec(p, pi + 1, t, k))
                return true;

        return false;
    }

    if (ti == t.size()) //if topic exhausted
        return false;


    if (p[pi] == "+") // if + m,atches ONE topic
        return match_rec(p, pi + 1, t, ti + 1);


    if (p[pi] == t[ti])
        return match_rec(p, pi + 1, t, ti + 1);

    return false; //if patterns differ
}

// checks if a topic string matches a subscription pattern
bool topic_matches(const string &pattern, const string &topic) {
	//converts both stringsinto topics
    return match_rec(split_levels(pattern), 0, split_levels(topic), 0);
}
