CXX=clang++
CC=clang

#CC=gcc
#CXX=g++

#FLAGS=-Wall -Wextra
FLAGS=-Weverything -Wno-c++98-compat -Wno-c++98-c++11-compat -Wno-sign-conversion -g

CXXFLAGS=-std=c++14 $(FLAGS)

test: worldmap.o
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $^ -o $@
