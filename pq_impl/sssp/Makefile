# Files to compile that don't have a main() function
CXXFILES = fraser linden pqueue skiplist gc ptst harris harris_4leaders

# Files to compile that do have a main() function
#
# NB: "test.c" produced the "spray" executable in the SprayList repository
TARGETS = sssp #test

# Let the programmer choose 32 or 64 bits, but default to 64
BITS ?= 64

# Directory names
ODIR := ./obj$(BITS)
output_folder := $(shell mkdir -p $(ODIR))

# Names of files that the compiler generates
EXEFILES  = $(patsubst %, $(ODIR)/%,   $(TARGETS))
OFILES    = $(patsubst %, $(ODIR)/%.o, $(CXXFILES))
EXEOFILES = $(patsubst %, $(ODIR)/%.o, $(TARGETS))
DEPS      = $(patsubst %, $(ODIR)/%.d, $(CXXFILES) $(TARGETS))

# Compiler and compiler-specific flags (CSFLAGS)
CXX = g++

# NB: use += so that we can add additional flags in the files that include this
CXXFLAGS += -mrtm -MMD -m$(BITS) -std=c++17 #-Wall -Wextra  -Wno-class-memaccess #-Werror
#CXXFLAGS += -O0 -ggdb # flags for debug
CXXFLAGS += -O3 -DNDEBUG -march=native # flags for performance
LDFLAGS += -m$(BITS) -pthread -lpthread -lnuma -lm -march=native -latomic

LDFLAGS += -L../lib
LDFLAGS += -I../
LDFLAGS += -I../common
LDFLAGS += -I../dcss_plus
LDFLAGS += -I../descriptors
LDFLAGS += -I../include
LDFLAGS += -I../microbench
LDFLAGS += -I../recordmgr
LDFLAGS += -I../pipq-strict
LDFLAGS += -I../harris_ll
#LDFLAGS += -I../pipq-relaxed
LDFLAGS += -I../harris_ll_4leaders
LDFLAGS += -I../stealing-multi-queue
LDFLAGS += -I../common

# NB: These are extras from SprayList... revisit eventually
CXXFLAGS += -DLOCKFREE
CXXFLAGS += -DSSSP
CXXFLAGS += -fno-strict-aliasing
CXXFLAGS += -I./atomic_ops
CXXFLAGS += -DCACHE_LINE_SIZE=`getconf LEVEL1_DCACHE_LINESIZE` -DINTEL

# Build 'all' by default, and don't clobber intermediate files
.DEFAULT_GOAL = all
.PRECIOUS: $(OFILES) $(EXEOFILES)
.PHONY: all clean

# Goal is to build all executables
all: $(EXEFILES)

# Rules for building C++ object files
$(ODIR)/%.o: %.cc
	echo "[CXX] $< --> $@"
	$(CXX) $< -o $@ -c $(CXXFLAGS)

# Rules for building executables... we assume an executable uses *all* of the 
# OFILES
$(ODIR)/%: $(ODIR)/%.o $(OFILES)
	echo "[LD] $< --> $@"
	$(CXX) $^ -o $@ $(LDFLAGS)

# clean by clobbering the build folder
clean:
	echo Cleaning up...
	rm -rf $(ODIR) $(DEPLOY) $(TURNIN)

# Include any dependencies we generated previously
-include $(DEPS)
