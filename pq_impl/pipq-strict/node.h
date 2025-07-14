#include <mutex>
#include <thread>
#include <iostream>
#include <chrono>
#include <string>
#include "scxrecord.h"

#ifndef NODE_H
#define	NODE_H

namespace pq_ns {

    template <class V>
    class Node {
    public:
        volatile int key; // i.e., priority
        volatile int group;
        volatile int id;
        volatile bool marked;
        V value;

        std::mutex lock;
        atomic_uintptr_t scxRecord; // TODO: how to use this / should I ??

        Node* next;
        Node* prev; // TODO: how is this used?

        Node() {}
    };

    class List {
    public:
        volatile int size;
        Node* head;
        Node* tail;

        List() {}
    };
}

#endif	/* NODE_H */