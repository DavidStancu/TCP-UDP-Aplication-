#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <poll.h>
#include "common.h"

using namespace std;
// my subscriber
int main(int argc, char* argv[]) {
	//checks if all arguments are in place given the task structure
    if (argc != 4)
        return 1;

	//parses client id, server and port and memorises it
    string client_id = argv[1];
    const char* server_ip = argv[2];
    int port = atoi(argv[3]);

	// i m disabling buffering to not have outputs such as [timeout]
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	// creating TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &serv.sin_addr);

	//attempts to connect to server
    if (connect(sock, (sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect"); //sends a connection error
        return 1;
    }

	//send my cliend id as initial message
    send_packet(sock, client_id);
	
    vector<pollfd> fds = {{STDIN_FILENO, POLLIN, 0}, {sock, POLLIN, 0}};

    string buf;
    bool running = true;

	// start of the fun stuff
    while (running) {
		//put a wait for the stdin or socket
        poll(fds.data(), fds.size(), -1);

        for (size_t i = 0; i < fds.size(); i++) {
            if (!(fds[i].revents & POLLIN)) //if no data available
                continue;

            if (fds[i].fd == STDIN_FILENO) { //handles user input
                string line;

                if (!getline(cin, line)) { //checks if still open
                    running = false;
                    break;
                }

                send_packet(sock, line); //sends my command

                if (line == "exit") { // if user "exits"
                    running = false;
                    break;
                }
            } else {
				//handling message from server
                char tmp[4096];
                ssize_t n = recv(sock, tmp, sizeof(tmp), 0);

                if (n <= 0) { //error or connection has closed
                    running = false;
                    break;
                }

                buf.append(tmp, n);
				
				//displays all complete messages 
                string msg;
                while (extract_message(buf, msg)) {
                    cout << msg;
                }
            }
        }
    }

	//closes socket before exit
    close(sock);
    return 0;
}
