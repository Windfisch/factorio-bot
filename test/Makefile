include ../config.mk

COMPILER ?= GCC
ifeq ($(COMPILER),GCC)
	CC=gcc
	CXX=g++
	CXXFLAGS=--std=c++14 -Wall -Wextra -pedantic -g
else
	CC=clang
	CXX=clang++
	CXXFLAGS=--std=c++14 -Weverything -pedantic -Wno-c++98-compat -Wno-padded -Wno-sign-conversion -Wno-global-constructors -g
endif

all: worldlist.test clean

clean:
	@rm -f *.bin *.out

%.test: %.bin
	@./$< > $@.out
	@if ! diff -u $@.out.desired $@.out; then echo "TEST $@ FAILED, see above"; false; else echo "TEST $@ PASSED"; true; fi

%.bin: %.cpp
	$(CXX) $(CXXFLAGS) $< ../area.o -o $@
