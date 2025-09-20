all: server subscriber

server: server.cpp common.cpp common.h
	g++ -std=c++11 -Wall -Wextra server.cpp common.cpp -o server

subscriber: subscriber.cpp common.cpp common.h
	g++ -std=c++11 -Wall -Wextra subscriber.cpp common.cpp -o subscriber

clean:
	rm -f server subscriber
