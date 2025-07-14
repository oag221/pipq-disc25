include ../config.mk
PLAF = -DCPU_FREQ_GHZ=$(cpu_freq_ghz)

GPP = g++
FLAGS = -std=c++20 -mcx16
FLAGS += -g
FLAGS += $(PLAF)
FLAGS += -DUSE_GSTATS
FLAGS += -O3 
FLAGS += -MMD -m64 -fPIC -march=native -O3 -DNDEBUG #-Werror -Wall
FLAGS += -I./atomic_ops


LINDENFLAGS = -DCACHE_LINE_SIZE=`getconf LEVEL1_DCACHE_LINESIZE` -DINTEL

LDFLAGS = -L../lib
LDFLAGS += -I../
LDFLAGS += -I../common
LDFLAGS += -I../dcss_plus
LDFLAGS += -I../descriptors
LDFLAGS += -I../include
LDFLAGS += -I../microbench
LDFLAGS += -I../recordmgr

LDFLAGS += -lpthread
LDFLAGS += -ldl
LDFLAGS += -lnuma
LDFLAGS += -lpapi

# CBPQDIR = ../ChunkBasedPQ
# CBPQ_LIBS = -lrt
# CBPQ_LDFLAGS = -lpthread `pkg-config --libs gsl`
# CBPQ_OBJECTS = Atomicable.o listNode.o skipListCommon.o skipList.o

machine=$(shell hostname)

all: strict

strict: pipq linden lotan_shavit

pipq: harris.o
	$(GPP) $(FLAGS) harris.o -o $(machine).$@$(filesuffix).out -DPIPQ_STRICT $(pinning) main.cpp $(LDFLAGS) -I../harris_ll -I../pipq-strict

linden: ptst.o gc.o
	$(GPP) $(FLAGS) ptst.o gc.o -o $(machine).$@$(filesuffix).out -DLINDEN $(pinning) main.cpp $(LDFLAGS) -I../linden

lotan_shavit: fraser.o skiplist.o
	$(GPP) $(FLAGS) fraser.o skiplist.o -o $(machine).$@$(filesuffix).out -DLOTAN $(pinning) main.cpp $(LDFLAGS) -I../lotan-shavit

harris.o: harris.cc
	$(GPP) $(FLAGS) -c harris.cc -o harris.o

fraser.o: fraser.cc
	$(GPP) $(FLAGS) -c fraser.cc -o fraser.o

skiplist.o: skiplist.cc
	$(GPP) $(FLAGS) -c skiplist.cc -o skiplist.o


# ChunkedPQ: ${CBPQ_OBJECTS}
# 	$(GPP) ${FLAGS} ${CBPQ_OBJECTS} -o $(machine).$@$(filesuffix).out -DCBPQ $(pinning) main.cpp $(CBPQ_LDFLAGS) $(CBPQ_LIBS) -I../ChunkBasedPQ

# listNode.o: $(CBPQDIR)/listNode.c $(CBPQDIR)/listNode.h $(CBPQDIR)/skipListCommon.h
# 	${GPP} ${FLAGS} -c $(CBPQDIR)/$*.c -o $@ ${LIBS}

# Atomicable.o: $(CBPQDIR)/Atomicable.cpp $(CBPQDIR)/Atomicable.h
# 	${GPP} ${FLAGS} -c $(CBPQDIR)/$*.cpp -o $@ ${LIBS}

# skipList.o: $(CBPQDIR)/skipList.c $(CBPQDIR)/skipList.h $(CBPQDIR)/listNode.h $(CBPQDIR)/skipListCommon.h $(CBPQDIR)/atomicMarkedReference.h
# 	${GPP} ${FLAGS} -c $(CBPQDIR)/$*.c -o $@ ${LIBS}

# skipListCommon.o: $(CBPQDIR)/skipListCommon.c $(CBPQDIR)/listNode.h $(CBPQDIR)/skipListCommon.h $(CBPQDIR)/rand.h
# 	${GPP} ${FLAGS} -c $(CBPQDIR)/$*.c -o $@ ${LIBS}

# ptst.o: ptst.cc
# 	$(GPP) $(FLAGS) -c ptst.cc -o ptst.o

# gc.o: gc.cc
# 	$(GPP) $(FLAGS) -c gc.cc -o gc.o


.PHONY: clean
clean:
	rm *.out
	rm *.o
	rm *.d