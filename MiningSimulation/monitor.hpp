#ifndef _OS_2019_MONITOR_HPP_
#define _OS_2019_MONITOR_HPP_

#include <iostream>
#include <vector>
#include <pthread.h>
#include <semaphore.h>

#include "writeOutput.c"

//! A base class to help deriving monitor like classes 
class Monitor {
    pthread_mutex_t  mut;   // this will protect the monitor
public:
    Monitor() {
        pthread_mutex_init(&mut, NULL);
    }
    class Condition {
        Monitor *owner;
        pthread_cond_t cond;
    public:
        Condition(Monitor *o) {     // we need monitor ptr to access the mutex
                owner = o;
                pthread_cond_init(&cond, NULL) ; 
        }
        void wait() {  pthread_cond_wait(&cond, &owner->mut);}
        void notify() { pthread_cond_signal(&cond);}
        void notifyAll() { pthread_cond_broadcast(&cond);}
    };
    class Lock {
        Monitor *owner;
    public:
        Lock(Monitor *o) { // we need monitor ptr to access the mutex
            owner = o;
            pthread_mutex_lock(&(owner->mut)); // lock on creation
        }
        ~Lock() { 
            pthread_mutex_unlock(&(owner->mut)); // unlock on destruct
        }
        void lock() { pthread_mutex_lock(&owner->mut);}
        void unlock() { pthread_mutex_unlock(&owner->mut);}
    };
};

#define __synchronized__ Lock mutex(this);

struct Ore {
    OreType type;
};


#endif /* _OS_2019_MONITOR_HPP_ */

