#ifndef _OS_2019_TRANSPORTER_HPP
#define _OS_2019_TRANSPORTER_HPP

#include "monitor.hpp"

class Transporter: public Monitor {
    int tpID;
    unsigned int tpPeriod;
    Ore * currOre;

public:

    Transporter(int tpID, unsigned int tP) {
        this->tpID = tpID;
        tpPeriod = tP;
        currOre = NULL;
    }

    void loadOre(Ore * ore) {
        __synchronized__;
        currOre = ore;
    }

    void unloadOre() {
        __synchronized__;
        currOre = NULL;
    }

    Ore* getOre() {
        __synchronized__;
        return currOre;
    }

    unsigned int getPeriod() {
        __synchronized__;
        return tpPeriod;
    }

    int getTransporterId() {
        __synchronized__;
        return tpID;
    }
};

#endif /* _OS_2019_TRANSPORTER_HPP */
