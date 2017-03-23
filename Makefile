# possible values: GCC, clang
COMPILER=clang
EXE=test
OBJECTS=factorio_io.o gui/gui.o
DEBUG=1


DEBUGFLAGS = -g -O0 -D_GLIBCXX_DEBUG #-fsanitize=undefined,address
FASTFLAGS = -O2
CXXFLAGS_BASE = -std=c++14
CFLAGS_BASE = -std=c99

GUIFLAGS = -w -O2 # disable all warnings

COMPILER ?= GCC
ifeq ($(COMPILER),GCC)
	CC=gcc
	CXX=g++
	WARNFLAGS=-Wall -Wextra -Werror=return-type -Wno-missing-field-initializers
else
	CC=clang
	CXX=clang++
	WARNFLAGS=-Weverything -Wno-c++98-compat -Wno-c++98-c++11-compat -Wno-sign-conversion -Wno-padded -Wno-exit-time-destructors -Wno-global-constructors
endif


DEBUG ?= 1
ifeq ($(DEBUG),1)
	FLAGS += $(DEBUGFLAGS)
else
	FLAGS += $(FASTFLAGS)
endif

FLAGS += $(WARNFLAGS)
CFLAGS = $(CFLAGS_BASE) $(FLAGS) `fltk-config --cflags`
CXXFLAGS = $(CXXFLAGS_BASE) $(FLAGS) `fltk-config --cxxflags`
LINK=$(CXX)
LINKFLAGS=$(CXXFLAGS) `fltk-config --ldflags`

# Pseudotargets
.PHONY: all clean run

all: $(EXE)

clean:
	rm -f $(EXE) $(OBJECTS) $(OBJECTS:.o=.d) depend

info:
	@echo DEBUGFLAGS = $(DEBUGFLAGS)
	@echo FASTFLAGS = $(FASTFLAGS)
	@echo DEBUG = $(DEBUG)
	@echo CXXFLAGS_BASE = $(CXXFLAGS_BASE)
	@echo CXXFLAGS = $(CXXFLAGS)
	@echo LDFLAGS= $(LDFLAGS)

run: $(EXE)
	./$(EXE)


# Build system

include depend

depend: $(OBJECTS:.o=.d)
	cat $^ > $@

%.d: %.cpp
	$(CXX) $(CXXFLAGS) -MM -MT $(@:.d=.o) $< -o $@

gui/%.d: gui/%.cpp
	echo foo
	$(CXX) $(CXXFLAGS) -MM -MT $(@:.d=.o) $(GUIFLAGS) $< -o $@

gui/%.o: gui/%.cpp
	echo foo
	$(CXX) $(CXXFLAGS) $(GUIFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(EXE): $(OBJECTS)
	$(LINK) $(LINKFLAGS) $(LDFLAGS) $^ -o $@


-include .dummy.mk

.dummy.mk: Makefile
	@echo Makefile changed, cleaning to rebuild
	@echo "# used to let all targets depend on Makefile" > $@
	make -s clean

