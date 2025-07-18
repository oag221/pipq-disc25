/* 
 * File:   debugprinting.h
 * Author: trbot
 *
 * Created on June 24, 2016, 12:49 PM
 */

#ifndef DEBUGPRINTING_H
#define	DEBUGPRINTING_H

#include <atomic>
#include <sstream>
#include <iostream>

#define COUTATOMIC(coutstr) /*cout<<coutstr*/ \
{ \
    stringstream ss; \
    ss<<coutstr; \
    cout<<ss.str(); \
}
#define COUTATOMICTID(coutstr) /*cout<<"tid="<<(tid<10?" ":"")<<tid<<": "<<coutstr*/ \
{ \
    stringstream ss; \
    ss<<"tid="<<tid<<(tid<10?" ":"")<<": "<<coutstr; \
    cout<<ss.str(); \
}

// define USE_TRACE if you want many paths through the code to be traced with cout<<"..." statements
#ifdef USE_TRACE
std::atomic_bool ___trace(1);
std::atomic_bool ___validateops(1);
#define TRACE_TOGGLE {bool ___t = ___trace; ___trace = !___t;}
#define TRACE_ON {___trace = true;}
#define TRACE if(___trace)
#else
#define TRACE if(0)
#define TRACE_TOGGLE
#define TRACE_ON 
#endif

#endif	/* DEBUGPRINTING_H */
