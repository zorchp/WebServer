CXX=g++

LOG=./log/log.cpp
LOCKER=./locker/locker.hpp
THREADPOOL=./threadPool/threadPool.hpp
HTTP=./http_conn/http_conn.cpp

SRC=$(LOG) $(LOCKER) $(THREADPOOL) $(HTTP) main.cpp
OBJ=server.out


# options
RELEASE=-std=c++14 -lpthread -O2 -o $(OBJ)
DEBUG=-std=c++14 -lpthread -g -o $(OBJ)

# non-object
.PHONY: compile debug run clean

all: compile run

compile:
	$(CXX)  $(SRC) $(RELEASE)

run:
	./$(OBJ)

debug:
	$(CXX)  $(SRC) $(DEBUG)


clean:
	rm *.out


