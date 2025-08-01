/* 
 * File:   binding.h
 * Author: tabrown
 *
 * Created on June 23, 2016, 6:52 PM
 * 
 * Used to configure and implement a thread binding/pinning policy.
 * 
 * Instructions:
 * 1. invoke binding_configurePolicy, passing the number of logical processors.
 * 2. either #define one of the binding policies below, OR
 *    invoke binding_parseCustom, passing a string that describes the desired
 *    thread binding policy, e.g., "1,2,3,8-11,4-7,0".
 *    the string contains the ids of logical processors, or ranges of ids,
 *    separated by commas.
 * 3. have each thread invoke binding_bindThread.
 * 4. after your experiments run, you can confirm the binding for a given thread
 *    by invoking binding_getActualBinding.
 *    you can also check whether all logical processors had at most one thread
 *    mapped to them by invoking binding_isInjectiveMapping.
 */

#ifndef BINDING_H
#define	BINDING_H

#ifdef __CYGWIN__

void binding_parseCustom(string argv) {}

int binding_getActualBinding(const int tid, const int nprocessors) {
    return -1;
}

bool binding_isInjectiveMapping(const int nthreads, const int nprocessors) {
    return true;
}

void binding_bindThread(const int tid, const int nprocessors) {}

void binding_configurePolicy(const int nprocessors) {}

#else

#include <cassert>
#include <sched.h>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "plaf.h"
using namespace std;

// cpu sets for binding threads to cores
static cpu_set_t *cpusets[LOGICAL_PROCESSORS];

static int customBinding[LOGICAL_PROCESSORS];
static int numCustomBindings = 0;
//static vector<int> customBinding;

static void printBinding() {
    for (int i = 0; i < numCustomBindings; ++i) {
        cout << "thread " << i << " : " << customBinding[i] << endl;
    }
}

static unsigned digits(unsigned x) {
    int d = 1;
    while (x > 9) {
        x /= 10;
        ++d;
    }
    return d;
}

// parse token starting at argv[ix],
// place bindings for the token at the end of customBinding, and
// return the index of the first character in the next token,
//     or the size of the string argv if there are no further tokens.
static unsigned parseToken(string argv, int ix) {
    // token is of one of following forms:
    //      INT
    //      INT-INT
    // and is either followed by "," is the end of the string.
    // we first determine which is the case
    
    // read first INT
    int ix2 = ix;
    while (ix2 < argv.size() && argv[ix2] != ',') ++ix2;
    string token = argv.substr(ix, ix2-ix+1);
    int a = atoi(token.c_str());
    
    // check if the token is of the first form: INT
    ix = ix+digits(a);              // first character AFTER first INT
    if (ix >= argv.size() || argv[ix] == ',') {
        customBinding[numCustomBindings++] = a;
    // token is of the second form: INT-INT
    } else {
        assert(argv[ix] == '-');
        ++ix;                       // skip '-'
        
        // read second INT
        token = argv.substr(ix, ix2-ix+1);
        int b = atoi(token.c_str());
        //cout<<"a="<<a<<" b="<<b<<endl;
        
        // add range of bindings
        for (int i=a;i<=b;++i) {
            customBinding[numCustomBindings++] = i;
        }
        
        ix = ix+digits(b);          // first character AFTER second INT
    }
    // note: ix is the first character AFTER the last INT in the token
    // this is either a comma (',') or the end of the string argv.
    return (ix >= argv.size() ? argv.size() : ix+1 /* skip ',' */);
}

// argv contains a custom thread binding pattern, e.g., "1,2,3,8-11,4-7,0"
// threads will be bound according to this binding
void binding_parseCustom(string argv) {
    numCustomBindings = 0;
    
    unsigned ix = 0;
    while (ix < argv.size()) {
        ix = parseToken(argv, ix);
    }
}

static void doBindThread(const int tid, const int nprocessors) {
    if (sched_setaffinity(0, CPU_ALLOC_SIZE(nprocessors), cpusets[tid%nprocessors])) { // bind thread to core
        cout<<"ERROR: could not bind thread "<<tid<<" to cpuset "<<cpusets[tid%nprocessors]<<endl;
        exit(-1);
    }
}

int binding_getActualBinding(const int tid, const int nprocessors) {
    int result = -1;
#ifndef THREAD_BINDING
    if (numCustomBindings == 0) {
        return result;
    }
#endif
    unsigned bindings = 0;
    for (int i=0;i<nprocessors;++i) {
        if (CPU_ISSET_S(i, CPU_ALLOC_SIZE(nprocessors), cpusets[tid%nprocessors])) {
            result = i;
            ++bindings;
        }
    }
    if (bindings > 1) {
        cout<<"ERROR: "<<bindings<<" processor bindings for thread "<<tid<<endl;
        exit(-1);
    }
    if (bindings == 0) {
        cout<<"ERROR: "<<bindings<<" processor bindings for thread "<<tid<<endl;
        cout<<"DEBUG INFO: number of physical processors (set in Makefile)="<<nprocessors<<endl;
        exit(-1);
    }
    return result;
}

bool binding_isInjectiveMapping(const int nthreads, const int nprocessors) {
#ifndef THREAD_BINDING
    if (numCustomBindings == 0) {
        return true;
    }
#endif
    bool covered[nprocessors];
    for (int i=0;i<nprocessors;++i) covered[i] = 0;
    for (int i=0;i<nthreads;++i) {
        int ix = binding_getActualBinding(i, nprocessors);
        if (covered[ix]) {
            cout<<"thread i="<<i<<" bound to index="<<ix<<" but covered["<<ix<<"]="<<covered[ix]<<" already {function args: nprocessors="<<nprocessors<<" nthreads="<<nthreads<<"}"<<endl;
            return false;
        }
        covered[ix] = 1;
    }
    return true;
}

void binding_bindThread(const int tid, const int nprocessors) {
    if (numCustomBindings == 0) {
#ifdef THREAD_BINDING
        if (THREAD_BINDING != NONE) {
            doBindThread(tid, nprocessors);
        }
#endif
    } else {
        doBindThread(tid, nprocessors);
    }
}

// nthreads = 96
// nprocessors = 256
void binding_configurePolicy(const int nthreads, const int nprocessors) {
    // create cpu sets for binding threads to cores
    int size = CPU_ALLOC_SIZE(nprocessors);
    for (int i = 0; i < nprocessors; ++i) {
        cpusets[i] = CPU_ALLOC(nprocessors);
        CPU_ZERO_S(size, cpusets[i]);
        CPU_SET_S(customBinding[i%numCustomBindings], size, cpusets[i]);
    }
}

void binding_deinit(const int nprocessors) {
    for (int i=0;i<nprocessors;++i) {
        CPU_FREE(cpusets[i]);
    }
}

#endif /* __CYGWIN__ */

#endif	/* BINDING_H */

