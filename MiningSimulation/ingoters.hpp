#ifndef _OS_2019_INGOTERS_HPP
#define _OS_2019_INGOTERS_HPP

#include "monitor.hpp"

class Smelter: public Monitor {
    int sID;
    int ingotCount;
    unsigned int sPeriod;
    unsigned int capacity;
    OreType oreType;
    bool quit;
    bool timeout;
    int pushedTimerCount;

    Condition cv;
    std::vector<Ore> storage;
    std::vector<bool> timers;

public:

    Smelter(int sID, unsigned int sP, unsigned int cap, OreType oT) : cv(this) {
        this->sID = sID;
        sPeriod = sP;
        capacity = cap;
        oreType = oT;
        ingotCount = 0;
        quit = false;
        timeout = false;
        pushedTimerCount = 0;
    }

    void produceIngot() {
        __synchronized__;
        storage.pop_back();
        storage.pop_back();
        ingotCount++;
    }

    void receiveOre() {
        __synchronized__;
        Ore ore;
        ore.type = oreType;
        storage.emplace_back(ore);
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

    OreType getOreType() {
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

    void SetTimeout() {
        __synchronized__;
        timeout = true;
    }

    void ClearTimeout() {
        __synchronized__;
        timeout = false;
    }

    bool CheckTimeout() {
        __synchronized__;
        return timeout;
    }

    int addTimer() {
        __synchronized__;
        timers.push_back(true);
        pushedTimerCount++;
        return pushedTimerCount-1;
    }

    std::vector<bool> & getTimers() {
        __synchronized__;
        return timers;
    }
};

class Foundry: public Monitor {
    int fID;
    int ingotCount;
    unsigned int fPeriod;
    unsigned int capacity;
    bool quit;
    bool timeout;
    int pushedTimerCount;

    Condition cv;
    std::vector<Ore> ironStorage;
    std::vector<Ore> coalStorage;
    std::vector<bool> timers;

public:

    Foundry(int fID, unsigned int fP, unsigned int cap) : cv(this) {
        this->fID = fID;
        fPeriod= fP;
        capacity = cap;
        ingotCount = 0;
        quit = false;
        timeout = false;
        pushedTimerCount = 0;
    }

    void produceIngot() {
        __synchronized__;
        ironStorage.pop_back();
        coalStorage.pop_back();
        ingotCount++;
    }

    void receiveIron() {
        __synchronized__;
        Ore ore;
        ore.type = IRON;
        ironStorage.emplace_back(ore);
    }

    void receiveCoal() {
        __synchronized__;
        Ore ore;
        ore.type = COAL;
        coalStorage.emplace_back(ore);
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
    void SetTimeout() {
        __synchronized__;
        timeout = true;
    }

    void ClearTimeout() {
        __synchronized__;
        timeout = false;
    }

    bool CheckTimeout() {
        __synchronized__;
        return timeout;
    }

    int addTimer() {
        __synchronized__;
        timers.push_back(true);
        pushedTimerCount++;
        return pushedTimerCount-1;
    }

    std::vector<bool> & getTimers() {
        __synchronized__;
        return timers;
    }
};

#endif /* _OS_2019_INGOTERS_HPP */
