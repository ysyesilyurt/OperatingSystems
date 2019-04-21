#ifndef _OS_2019_TRANSPORTER_HPP
#define _OS_2019_TRANSPORTER_HPP

#include "monitor.hpp"

class Transporter: public Monitor {
    int tpID;
    unsigned int tpPeriod;
    OreType * currOre;

public:

    Transporter(int tpID, unsigned int tP) {
        this->tpID = tpID;
        tpPeriod = tP;
        currOre = NULL;
    }

    void loadOre(OreType ore) {
        __synchronized__;
        *currOre = ore;
    }

    void unloadOre() {
        __synchronized__;
        currOre = NULL;
    }

    OreType* getOre() {
        __synchronized__;
        return currOre;
    }

    int getPeriod() {
        __synchronized__;
        return tpPeriod;
    }

    int getTransporterId() {
        __synchronized__;
        return tpID;
    }
};

#endif /* _OS_2019_TRANSPORTER_HPP */
