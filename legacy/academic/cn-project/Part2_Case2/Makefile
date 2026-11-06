CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread
all: server client
server: server.cpp common.hpp
	$(CXX) $(CXXFLAGS) server.cpp -o server
client: client.cpp common.hpp
	$(CXX) $(CXXFLAGS) client.cpp -o client
clean:
	rm -f server client
.PHONY: all clean
