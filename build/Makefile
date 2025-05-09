
CXX = g++
SRC = ../src
OBJ = obj
CFLAGS = -g -Wall -march=native -msse4.2 -std=c++20
SOURCES = $(wildcard $(SRC)/*.cpp)
OBJECTS = $(patsubst $(SRC)/%.cpp, $(OBJ)/%.o, $(SOURCES))
INCLUDE = -I../include

all: main

main: $(OBJECTS)
	$(CXX) -o main $^

clean:
	rm -r obj main
.PHONY: clean

$(OBJECTS): $(OBJ)/%.o: $(SRC)/%.cpp
	mkdir -p $(OBJ)
	$(CXX) $(CFLAGS) $(INCLUDE) -c -o '$@' '$<'
