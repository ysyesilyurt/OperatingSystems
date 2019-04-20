#ifndef _OS_2019_TRANSPORTER_HPP
#define _OS_2019_TRANSPORTER_HPP

class Transporter: public Monitor {
    int tpID;
    unsigned int tpPeriod;
    unsigned int currOre;

public:

    Transporter(int tpID, unsigned int tP) {
        this->tpID = tpID;
        tpPeriod = tP;
        currOre = 99;
    }

    void loadOre(OreType ore) {
        __synchronized__;
        currOre = ore;
    }

    void unloadOre() {
        __synchronized__;
        currOre = 99;
    }

    unsigned int getOre() {
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
