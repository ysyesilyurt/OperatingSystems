#ifndef _OS_2019_INGOTERS_HPP
#define _OS_2019_INGOTERS_HPP

class Smelter: public Monitor {
    int sID;
    int ingotCount;
    unsigned int sPeriod;
    unsigned int capacity;
    unsigned int oreType;
    bool quit;

    Condition cv;
    std::vector<OreType> storage;

public:

    Smelter(int sID, unsigned int sP, unsigned int cap, unsigned int oT) : cv(this) {
        this->sID = sID;
        sPeriod = sP;
        capacity = cap;
        oreType = oT;
        ingotCount = 0;
        quit = false;
    }

    void produceIngot() {
        __synchronized__;
        storage.pop_back();
        storage.pop_back();
        ingotCount++;
    }

    void receiveOre() {
        __synchronized__;
        storage.emplace_back(oreType);
    }

    void wait() {
        __synchronized__;
        cv.wait();
    }

    void notify() {
        __synchronized__;
        cv.notify();
    }

    bool checkQuit() {
        __synchronized__;
        return quit;
    }

    void setQuit() {
        __synchronized__;
        quit = true;
    }

    bool isFull() {
        __synchronized__;
        return storage.size() == capacity;
    }

    bool isEmpty() {
        __synchronized__;
        return storage.empty();
    }

    bool hasEnoughOres() {
        __synchronized__;
        return storage.size() >= 2;
    }

    int getPeriod() {
        __synchronized__;
        return sPeriod;
    }

    int getSmelterId() {
        __synchronized__;
        return sID;
    }

    unsigned int getOreType() {
        __synchronized__;
        return oreType;
    }

    int getCapacity() {
        __synchronized__;
        return capacity;
    }

    int getIngotCount() {
        __synchronized__;
        return ingotCount;
    }

    int getWaitingOreCount() {
        __synchronized__;
        return storage.size();
    }

    bool timerStop = false;
};

class Foundry: public Monitor {
    int fID;
    int ingotCount;
    unsigned int fPeriod;
    unsigned int capacity;
    bool quit;

    Condition cv;
    std::vector<OreType> ironStorage;
    std::vector<OreType> coalStorage;

public:

    Foundry(int fID, unsigned int fP, unsigned int cap) : cv(this) {
        this->fID = fID;
        fPeriod= fP;
        capacity = cap;
        ingotCount = 0;
        quit = false;
    }

    void produceIngot() {
        __synchronized__;
        ironStorage.pop_back();
        coalStorage.pop_back();
        ingotCount++;
    }

    void receiveIron() {
        __synchronized__;
        ironStorage.emplace_back(IRON);
    }

    void receiveCoal() {
        __synchronized__;
        coalStorage.emplace_back(COAL);
    }

    void wait() {
        __synchronized__;
        cv.wait();
    }

    void notify() {
        __synchronized__;
        cv.notify();
    }

    bool checkQuit() {
        __synchronized__;
        return quit;
    }

    void setQuit() {
        __synchronized__;
        quit = true;
    }

    bool isFullIron() {
        __synchronized__;
        return ironStorage.size() == capacity;
    }

    bool isFullCoal() {
        __synchronized__;
        return coalStorage.size() == capacity;
    }

    bool hasEnoughOres() {
        __synchronized__;
        return !ironStorage.empty() and !coalStorage.empty();
    }

    bool isEmptyIron() {
        __synchronized__;
        return ironStorage.empty();
    }

    bool isEmptyCoal() {
        __synchronized__;
        return ironStorage.empty();
    }

    int getPeriod() {
        __synchronized__;
        return fPeriod;
    }

    int getFoundryId() {
        __synchronized__;
        return fID;
    }

    int getCapacity() {
        __synchronized__;
        return capacity;
    }

    int getIngotCount() {
        __synchronized__;
        return ingotCount;
    }

    int getWaitingIronCount() {
        __synchronized__;
        return ironStorage.size();
    }

    int getWaitingCoalCount() {
        __synchronized__;
        return coalStorage.size();
    }

    bool timerStop = false;
};

#endif /* _OS_2019_INGOTERS_HPP */
