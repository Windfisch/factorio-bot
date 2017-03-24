# possible values: GCC, clang
COMPILER=GCC
EXE=test
OBJECTS=factorio_io.o area.o pathfinding.o gui/gui.o
DEBUG=1


MODNAME=Windfisch_0.0.1
MODSRCS=$(addprefix luamod/$(MODNAME)/,control.lua data.lua info.json prototypes/.emptydir)



DEBUGFLAGS = -g -pg -nopie -Og -D_GLIBCXX_DEBUG #-fsanitize=undefined,address
FASTFLAGS = -O2
CXXFLAGS_BASE = -std=c++14
CFLAGS_BASE = -std=c99

GUIFLAGS = -w -Og # disable all warnings

COMPILER ?= GCC
ifeq ($(COMPILER),GCC)
	CC=gcc
	CXX=g++
	WARNFLAGS=-Wall -Wextra -Wno-missing-field-initializers
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

include config.mk
MODDESTS=$(MODSRCS:luamod/%=$(FACTORIODIR)/mods/%)

# Pseudotargets
.PHONY: all clean run info mod help

all: $(EXE)

clean:
	rm -f $(EXE) $(OBJECTS) $(OBJECTS:.o=.d) depend

help:
	@echo "Targets:"
	@echo "    all      -> build the program"
	@echo "    run      -> run the program, using the datafile in the factorio installation (see Configuration below)"
	@echo "                Note: this will NOT install the mod or (re)create the datafile. Do this manually by"
	@echo "                      running 'make datafile' before."
	@echo "    mod      -> install the lua mod into the factorio installation (see Configuration below)"
	@echo "    datafile -> update the lua mod and re-create its output datafile
	@echo "    clean    -> all generated files (including depend. except config.mk)"
	@echo "    info     -> show an overview of the CFLAGS used"
	@echo "    help     -> show this help"
	@echo
	@echo "Configuration:"
	@echo "    Edit the 'config.mk' file and adjust the factorio path."
	@echo "    This path will be used by the 'run', 'mod' and 'outfile' targets"
	

info:
	@echo DEBUGFLAGS = $(DEBUGFLAGS)
	@echo FASTFLAGS = $(FASTFLAGS)
	@echo DEBUG = $(DEBUG)
	@echo CXXFLAGS_BASE = $(CXXFLAGS_BASE)
	@echo CXXFLAGS = $(CXXFLAGS)
	@echo LDFLAGS= $(LDFLAGS)

run: $(EXE)
	./$(EXE)

mod: $(MODDESTS)

DATAFILE=$(FACTORIODIR)/script-output/output1.txt
SAVEGAME=$(FACTORIODIR)/factorio-bot.zip
SERVERSETTINGS=luamod/server-settings.json

datafile: $(DATAFILE)
del_datafile:
	rm -f $(DATAFILE)
savegame: $(SAVEGAME)

$(SAVEGAME):
	$(FACTORIODIR)/bin/x64/factorio --create $@

.PRECIOUS: $(DATAFILE)

$(DATAFILE): $(MODDESTS) $(SAVEGAME) $(SERVERSETTINGS)
	rm -f $(DATAFILE)
	cp $(SAVEGAME) $(SAVEGAME:.zip=.tmp.zip)
	-$(FACTORIODIR)/bin/x64/factorio --start-server $(SAVEGAME:.zip=.tmp.zip) --server-settings $(SERVERSETTINGS)
	echo
	echo made $(DATAFILE)

# Build system

config.mk:
	echo 'FACTORIODIR=/path/to/factorio' > $@

$(FACTORIODIR)/mods/$(MODNAME)/%: luamod/$(MODNAME)/%
	mkdir -p $(dir $@)
	cp -a $< $@



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

