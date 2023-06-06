CXX=g++
# CXX=clang++-12
# close snprintf warning: directive output may be truncated writing 
# between 1 and 11 bytes into a region of size between 4 and 14
CXX_FLAG=-std=c++14 -lpthread -Wformat-truncation=0

LOG=./log/block_queue.hpp ./log/log.cpp
LOCKER=./locker/locker.hpp
THREADPOOL=./threadPool/threadPool.hpp
TIMER=./timer/lst_timer.cpp
HTTP=./http_conn/http_conn.cpp

SRC=$(LOG) $(LOCKER) $(THREADPOOL) $(HTTP) $(TIMER) main.cpp
OBJ=server.out


# options
RELEASE=$(CXX_FLAG) -O2 -o $(OBJ)
DEBUG=$(CXX_FLAG) -g -o $(OBJ)

# non-object
.PHONY: compile debug run clean

all: compile run

compile:
	$(CXX) $(SRC) $(RELEASE)

run:
	./$(OBJ)

debug:
	$(CXX) $(SRC) $(DEBUG)


clean:
	rm *.out


