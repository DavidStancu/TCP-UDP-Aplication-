#include <iostream>
#include <unistd.h>
#include <map>
#include <vector>
#include <string>
#include <set>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <fcntl.h>
#include "common.h"
#include <string.h>
#include <algorithm>

using namespace std;

//data structure for the client
struct Client {
    int sockfd = -1;
    vector<string> patterns;
    string buffer;
};

//maps for cliend id to state and flid descriptor to client id
map<string, Client> clients;
map<int, string> fd2id;

//removes a file descriptor for the pollfd vector
void remove_fd(vector<pollfd>& fds, int fd) {
    fds.erase(remove_if(fds.begin(), fds.end(),
        [fd](pollfd q) {
            return q.fd == fd;
        }),
        fds.end());
}

//shuts down server and disconnects clients
void shutdown_server(int tcp_sock, int udp_sock, vector<pollfd>& fds) {
    for (size_t i = 0; i < clients.size(); i++) {
        auto it = next(clients.begin(), i);
        if (it->second.sockfd != -1)
        {
            close(it->second.sockfd);
            cout << "Client " << it->first << " disconnected.\n";
        }
    }
    close(tcp_sock);
    close(udp_sock);
    fds.clear();
}

int main(int argc, char *argv[]) {
    if (argc != 2)// checks if enough arguments in command line
        return 1;

	//disabling buffering to avoid [timoeut]
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	//memorises the port number
    int port = atoi(argv[1]);

	// creates TCP and UDP sockets
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

	//binds the sockets and starts listening to the TCP connection
    bind(tcp_sock, (sockaddr*)&serv_addr, sizeof(serv_addr));
    bind(udp_sock, (sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(tcp_sock, SOMAXCONN);

	//making input non-blocking to detect exiting
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

	//monitors input and the sockets
    vector<pollfd> fds = {{STDIN_FILENO, POLLIN, 0}, {tcp_sock, POLLIN, 0}, {udp_sock, POLLIN, 0}};

    while (true) {
        poll(fds.data(), fds.size(), -1);
		
        for (size_t i = 0; i < fds.size(); i++) {
            int fd = fds[i].fd;

            if (!(fds[i].revents & POLLIN)) //if no data
                continue;

			//handles exit
            if (fd == STDIN_FILENO) {
                char cmd[32];
                if (fgets(cmd, sizeof(cmd), stdin) && strncmp(cmd, "exit", 4) == 0) {
                    shutdown_server(tcp_sock, udp_sock, fds);
                    return 0;
                }
			} else if (fd == tcp_sock) { //if of type tcp (tcp connection)
                sockaddr_in cli;
                socklen_t clen = sizeof(cli);
                int ns = accept(tcp_sock, (sockaddr*)&cli, &clen);

                if (ns < 0)
                    continue;

                int flag = 1;
                setsockopt(ns, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
				
				//adds to list
                fds.push_back({ ns, POLLIN, 0 });
                fd2id[ns] = "";
            } else if (fd == udp_sock) { //if of type udp (datagram)
                sockaddr_in cli;
                socklen_t clen = sizeof(cli);

				//gets message length
                UDPMessage message;
                ssize_t len = recvfrom(udp_sock, &message, sizeof(message), 0, (sockaddr*)&cli, &clen);

                if (len <= 0)
                    continue;

				//gets topic
                string topic(message.topic_name);
                topic = topic.substr(0, topic.find('\0'));

				//decodes and prepares message
                string full = decode_message(message, cli);
                set<string> done;

				//sends to all the sunscribers that match 
                for (size_t j = 0; j < clients.size(); j++) {
                    auto it = next(clients.begin(), j);
                    string cid = it->first;
                    Client& cl = it->second;

                    if (cl.sockfd == -1)
                        continue;

                    if (done.count(cid))
                        continue;

                    for (size_t k = 0; k < cl.patterns.size(); k++) {
                        if (topic_matches(cl.patterns[k], topic)) {
                            send_packet(cl.sockfd, full);
                            done.insert(cid);
                            break;
                        }
                    }
                }
            } else { //if message from tcp client
                string& client_id = fd2id[fd];
                Client& C = clients[client_id];

                char raw[4096];
                ssize_t n = recv(fd, raw, sizeof(raw), 0);

                if (n <= 0) { //if i detect disconnect/error occured
                    if (!client_id.empty())
                    {
                        cout << "Client " << client_id << " disconnected.\n";
                        C.sockfd = -1;
                    }

					//closes socket
                    close(fd);
                    fd2id.erase(fd);
                    remove_fd(fds, fd);
                    continue;
                }

				//append the data received
                C.buffer.append(raw, n);

                string msg;
                while (extract_message(C.buffer, msg)) { //extracts and handles message
                    if (client_id.empty()) { // if first message is cliend ID
                        if (clients.count(msg) && clients[msg].sockfd != -1) {
                            cout << "Client " << msg << " already connected.\n";
                            close(fd);
                            fd2id.erase(fd);
                            remove_fd(fds, fd);
                            break;
                        }

						//binds new id to connection
                        client_id = msg;
                        clients[client_id].sockfd = fd;

                        sockaddr_in peer;
                        socklen_t plen = sizeof(peer);
                        getpeername(fd, (sockaddr*)&peer, &plen);

                        cout << "New client " << client_id << " connected from " << inet_ntoa(peer.sin_addr) << ":" << ntohs(peer.sin_port) << "\n";
                    } else if (msg == "exit") { // if "exit"
                        cout << "Client " << client_id << " disconnected.\n";

                        close(fd);
                        clients[client_id].sockfd = -1;
                        fd2id.erase(fd);
                        remove_fd(fds, fd);
                        break;
                    } else if (msg.rfind("subscribe ", 0) == 0) { // if subscribe command
                        string topic = msg.substr(10);
                        topic.erase(topic.find_last_not_of("\n\r") + 1);

                        vector<string>& P = clients[client_id].patterns;

                        if (find(P.begin(), P.end(), topic) == P.end()) //adds new topic if not already subscribed
                            P.push_back(topic);

                        send_packet(fd, "Subscribed to topic " + topic + "\n");
                    } else if (msg.rfind("unsubscribe ", 0) == 0) { //if unsubscribe command
                        string topic = msg.substr(12);
                        topic.erase(topic.find_last_not_of("\n\r") + 1);

                        vector<string>& P = clients[client_id].patterns;

                        P.erase(remove(P.begin(), P.end(), topic), P.end()); //removes topic if existing

                        send_packet(fd, "Unsubscribed from topic " + topic + "\n");
                    }
                }
            }
        }
    }

    return 0;
}
