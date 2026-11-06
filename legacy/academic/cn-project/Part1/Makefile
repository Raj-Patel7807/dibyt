CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread

all: server server_mt client

server: server.cpp common.hpp
	$(CXX) $(CXXFLAGS) server.cpp -o server

server_mt: server.cpp common.hpp
	$(CXX) $(CXXFLAGS) -DMULTITHREADED_DEFAULT=1 server.cpp -o server_mt

client: client.cpp common.hpp
	$(CXX) $(CXXFLAGS) client.cpp -o client

clean:
	rm -f server server_mt client

test: all
	python3 test_part1.py

.PHONY: all clean test
