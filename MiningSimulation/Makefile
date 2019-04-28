CC = g++

all: clean compile

compile: simulator.cpp monitor.hpp miner.hpp ingoters.hpp transporter.hpp writeOutput.c writeOutput.h 
		$(CC) simulator.cpp -o simulator -lpthread

clean:
		rm -f simulator

run: clean compile simulate

simulate: simulator
		./simulator
