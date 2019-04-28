#include <algorithm>
#include <queue>
#include <unistd.h>

#include "monitor.hpp"
#include "miner.hpp"
#include "transporter.hpp"
#include "ingoters.hpp"

void *MinerMain(void *);
void *TransporterMain(void *);
void *SmelterMain(void *);
void *FoundryMain(void *);
void *sTimer(void *);
void *fTimer(void *);

/* global variables */

pthread_mutex_t minerMut =  PTHREAD_MUTEX_INITIALIZER;
int lastMiner = 0;
pthread_mutex_t smelterMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t iSmelterMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cSmelterMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t foundryMut =  PTHREAD_MUTEX_INITIALIZER;

std::vector<Miner*> miners;
std::vector<Transporter*> transporters;
std::vector<Smelter*> smelters;
std::vector<Smelter*> ironSmelters;
std::vector<Smelter*> copperSmelters;
std::vector<Foundry*> foundries;

pthread_mutex_t mExitMut =  PTHREAD_MUTEX_INITIALIZER;
std::vector<bool> minerExitted;
bool allMinersExitted = false;

sem_t semTransMiners;
sem_t semTransIron;
sem_t semTransCopper;
sem_t semTransCoal;

pthread_mutex_t iSmelterPrioMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cSmelterPrioMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t iFoundryPrioMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cFoundryPrioMut =  PTHREAD_MUTEX_INITIALIZER;

std::queue<Smelter*> prioritizedIronSmelters;
std::queue<Smelter*> prioritizedCopperSmelters;
std::queue<Foundry*> ironPrioritizedFoundries;
std::queue<Foundry*> coalPrioritizedFoundries;

struct TransParam {
    Transporter *t;
    int NumMiners;
};
struct StimerParam {
    int id;
    int timerId;
};
struct FtimerParam {
    int id;
    int timerId;
};

int main(int argc, const char* argv[]) {

    int numMines, numTrans, numSmelters, numFoundries;
    int period, cap, tA;
    unsigned int oT;
    pthread_t *minerThreads, *transThreads, *smeltThreads, *foundThreads;

    sem_init(&semTransMiners, 0, 0);
    sem_init(&semTransIron, 0, 0);
    sem_init(&semTransCopper, 0, 0);
    sem_init(&semTransCoal, 0, 0);

    InitWriteOutput();
    std::cin >> numMines;
    if (numMines) {
        minerThreads = new pthread_t[numMines];
        minerExitted.insert(minerExitted.end(), numMines, false);
        for (int i = 0; i < numMines; ++i) {
            std::cin >> period >> cap >> oT >> tA;
            Miner * miner = new Miner(i+1, period, cap, (OreType) oT, tA);
            miners.emplace_back(miner);
            pthread_create(&minerThreads[i], NULL, MinerMain, (void *) miner);
        }
    }

    std::cin >> numTrans;
    if (numTrans) {
        TransParam *tparams = new TransParam[numTrans] ;
        transThreads = new pthread_t[numTrans];
        for (int i = 0; i < numTrans; ++i) {
            std::cin >> period;
            Transporter * transporter = new Transporter(i+1, period);
            transporters.emplace_back(transporter);
            tparams[i].t = transporter;
            tparams[i].NumMiners = numMines;
            pthread_create(&transThreads[i], NULL, TransporterMain, (void *) (tparams + i));
        }
    }

    std::cin >> numSmelters;
    if (numSmelters) {
        smeltThreads = new pthread_t[numSmelters];
        for (int i = 0; i < numSmelters; ++i) {
            std::cin >> period >> cap >> oT;
            Smelter * smelter = new Smelter(i+1, period, cap, (OreType) oT);
            smelters.emplace_back(smelter);
            if (oT) {
                // oT = copper
                copperSmelters.emplace_back(smelter);
            }
            else {
                // oT = iron
                ironSmelters.emplace_back(smelter);
            }
            pthread_create(&smeltThreads[i], NULL, SmelterMain, (void *) smelter);
        }
    }

    std::cin >> numFoundries;
    if (numFoundries) {
        foundThreads = new pthread_t[numFoundries];
        for (int i = 0; i < numFoundries; ++i) {
            std::cin >> period >> cap;
            Foundry * foundry = new Foundry(i+1, period, cap);
            foundries.emplace_back(foundry);
            pthread_create(&foundThreads[i], NULL, FoundryMain, (void *) foundry);
        }
    }

    /* Reap Agents */
    for (int i = 0; i < numMines; i++)
        pthread_join(minerThreads[i], NULL);

    for (int i = 0; i < numTrans; i++)
        pthread_join(transThreads[i], NULL);

    for (int i = 0; i < numSmelters; i++)
        pthread_join(smeltThreads[i], NULL);

    for (int i = 0; i < numFoundries; i++)
        pthread_join(foundThreads[i], NULL);

    if (numMines)
        delete [] minerThreads;
    if (numTrans)
        delete [] transThreads;
    if (numSmelters)
        delete [] smeltThreads;
    if (numFoundries)
        delete [] foundThreads;

    for (int i = 0; i < miners.size(); ++i)
        delete miners[i];
    for (int i = 0; i < transporters.size(); ++i)
        delete transporters[i];
    for (int i = 0; i < ironSmelters.size(); ++i)
        delete ironSmelters[i];
    for (int i = 0; i < copperSmelters.size(); ++i)
        delete copperSmelters[i];
    for (int i = 0; i < foundries.size(); ++i)
        delete foundries[i];

    return 0;
}

void *MinerMain(void *p) {
    /* Miner main routine */
    Miner *miner = (Miner *) p;
    double mPeriod = miner->getPeriod() - (miner->getPeriod()*0.01) + (rand()%(int)(miner->getPeriod()*0.02));

    MinerInfo minerInfo;
    FillMinerInfo(&minerInfo, miner->getMinerId(), miner->getOreType(), miner->getCapacity(), miner->getCurrOreCount());
    WriteOutput(&minerInfo, NULL, NULL, NULL, MINER_CREATED);

    while (true) {
        if (miner->checkQuit()) {
            /* Maximum Ore Count reached, Quit. */
            miner->setQuit();
            FillMinerInfo(&minerInfo, miner->getMinerId(), miner->getOreType(), miner->getCapacity(), miner->getCurrOreCount());
            WriteOutput(&minerInfo, NULL, NULL, NULL, MINER_STOPPED);
            break;
        }
        else {
            while (miner->isFull())
                miner->wait();

            /* ProduceOre */
            FillMinerInfo(&minerInfo, miner->getMinerId(), miner->getOreType(), miner->getCapacity(), miner->getCurrOreCount());
            WriteOutput(&minerInfo, NULL, NULL, NULL, MINER_STARTED);
            usleep(mPeriod);
            miner->mineOre();

            sem_post(&semTransMiners);

            FillMinerInfo(&minerInfo, miner->getMinerId(), miner->getOreType(), miner->getCapacity(), miner->getCurrOreCount());
            WriteOutput(&minerInfo, NULL, NULL, NULL, MINER_FINISHED);
            usleep(mPeriod);
        }
    }
    return NULL;
}

void *TransporterMain(void *p) {
    /* Transporter main routine */
    TransParam *tp = (TransParam *) p;
    Transporter *transporter = tp->t;
    int numMiners = tp->NumMiners;
    bool gotOre, unloadedOre;
    int minerCount, index;
    double transPeriod = transporter->getPeriod() - (transporter->getPeriod()*0.01) + (rand()%(int)(transporter->getPeriod()*0.02));

    FoundryInfo foundryInfo;
    SmelterInfo smelterInfo;
    MinerInfo minerInfo;
    TransporterInfo transporterInfo;
    FillTransporterInfo(&transporterInfo, transporter->getTransporterId(), &(transporter->getOre()->type));
    WriteOutput(NULL, &transporterInfo, NULL, NULL, TRANSPORTER_CREATED);

    while (true) {
        pthread_mutex_lock (&minerMut);
        gotOre = false;
        minerCount = 0;
        index = lastMiner;
        while (minerCount <= numMiners) {
            /* Transporter thread Miner routine */

            if (index == numMiners)
                index = 0;
            if (!miners[index]->isEmpty()) {
                lastMiner = index + 1;
                FillMinerInfo(&minerInfo, miners[index]->getMinerId(), (OreType) 0,0,0);
                FillTransporterInfo(&transporterInfo, transporter->getTransporterId(), &(transporter->getOre()->type));
                WriteOutput(&minerInfo, &transporterInfo, NULL, NULL, TRANSPORTER_TRAVEL);
                usleep(transPeriod);

                // load an ore
                miners[index]->removeOre();
                Ore ore;
                ore.type = miners[index]->getOreType();
                transporter->loadOre(&ore);
                FillMinerInfo(&minerInfo, miners[index]->getMinerId(), miners[index]->getOreType(),
                              miners[index]->getCapacity(), miners[index]->getCurrOreCount());
                FillTransporterInfo(&transporterInfo, transporter->getTransporterId(), &(transporter->getOre()->type));
                WriteOutput(&minerInfo, &transporterInfo, NULL, NULL, TRANSPORTER_TAKE_ORE);
                usleep(transPeriod);
                miners[index]->notify();
                gotOre = true;
                break;
            }
            else if (miners[index]->checkQuit()) {
                /* If Empty Storage in Miner + its agent exitted, mark this one as Exitted */
                pthread_mutex_lock (&mExitMut);
                minerExitted[index] = true;
                if (std::find(minerExitted.begin(), minerExitted.end(), false) == minerExitted.end())
                    allMinersExitted = true;
                pthread_mutex_unlock (&mExitMut);
            }
            minerCount++;
            index++;
        }
        pthread_mutex_unlock (&minerMut);

        pthread_mutex_lock (&mExitMut);
        if (gotOre) {
            /* Transporter thread Producer routine */

            unloadedOre = false;
            pthread_mutex_unlock (&mExitMut);
            while (true) {
                if (transporter->getOre()->type == 0) {
                    /* IRON */

                    /* First Check Prioritized Iron Producers */

                    pthread_mutex_lock(&iFoundryPrioMut);
                    if (!ironPrioritizedFoundries.empty()) {
                        Foundry * prioFoundry = ironPrioritizedFoundries.front();
                        ironPrioritizedFoundries.pop();
                        pthread_mutex_unlock(&iFoundryPrioMut);

                        // travel
                        FillFoundryInfo(&foundryInfo, prioFoundry->getFoundryId(), 0, 0, 0, 0);
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            &(transporter->getOre()->type));
                        WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_TRAVEL);
                        usleep(transPeriod);

                        // unload ore
                        FillFoundryInfo(&foundryInfo, prioFoundry->getFoundryId(), prioFoundry->getCapacity(),
                                        prioFoundry->getWaitingIronCount(), prioFoundry->getWaitingCoalCount(),
                                        prioFoundry->getIngotCount());
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            &(transporter->getOre()->type));
                        WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_DROP_ORE);
                        usleep(transPeriod);
                        prioFoundry->receiveIron();
                        transporter->unloadOre();
                        prioFoundry->notify();
                        break;
                    }
                    pthread_mutex_unlock(&iFoundryPrioMut);

                    pthread_mutex_lock(&iSmelterPrioMut);
                    if (!prioritizedIronSmelters.empty()) {
                        Smelter * prioSmelter = prioritizedIronSmelters.front();
                        prioritizedIronSmelters.pop();
                        pthread_mutex_unlock(&iSmelterPrioMut);

                        // travel
                        FillSmelterInfo(&smelterInfo, prioSmelter->getSmelterId(), (OreType) 0, 0, 0, 0);
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            &(transporter->getOre()->type));
                        WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_TRAVEL);
                        usleep(transPeriod);

                        // unload ore
                        FillSmelterInfo(&smelterInfo, prioSmelter->getSmelterId(), prioSmelter->getOreType(),
                                        prioSmelter->getCapacity(), prioSmelter->getWaitingOreCount(),
                                        prioSmelter->getIngotCount());
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            &(transporter->getOre()->type));
                        WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_DROP_ORE);
                        usleep(transPeriod);
                        prioSmelter->receiveOre();
                        transporter->unloadOre();
                        prioSmelter->notify();
                        break;
                    }
                    pthread_mutex_unlock(&iSmelterPrioMut);

                    /* If no Prioritized agent is present, Check for Producers with empty storage space */

                    pthread_mutex_lock(&foundryMut);
                    for (int j = 0; j < foundries.size(); ++j) {
                        if (!foundries[j]->checkQuit() and !foundries[j]->isFullIron()) {

                            // travel
                            FillFoundryInfo(&foundryInfo, foundries[j]->getFoundryId(), 0, 0, 0, 0);
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                &(transporter->getOre()->type));
                            WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_TRAVEL);
                            usleep(transPeriod);

                            // unload ore
                            FillFoundryInfo(&foundryInfo, foundries[j]->getFoundryId(), foundries[j]->getCapacity(),
                                            foundries[j]->getWaitingIronCount(), foundries[j]->getWaitingCoalCount(),
                                            foundries[j]->getIngotCount());
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                &(transporter->getOre()->type));
                            WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_DROP_ORE);
                            usleep(transPeriod);
                            foundries[j]->receiveIron();
                            transporter->unloadOre();
                            foundries[j]->notify();
                            unloadedOre = true;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&foundryMut);
                    if (unloadedOre)
                        break;

                    pthread_mutex_lock(&iSmelterMut);
                    for (int j = 0; j < ironSmelters.size(); ++j) {
                        if (!ironSmelters[j]->checkQuit() and !ironSmelters[j]->isFull()) {

                            // travel
                            FillSmelterInfo(&smelterInfo, ironSmelters[j]->getSmelterId(), (OreType) 0, 0, 0, 0);
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                &(transporter->getOre()->type));
                            WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_TRAVEL);
                            usleep(transPeriod);

                            // unload ore
                            FillSmelterInfo(&smelterInfo, ironSmelters[j]->getSmelterId(), ironSmelters[j]->getOreType(),
                                            ironSmelters[j]->getCapacity(), ironSmelters[j]->getWaitingOreCount(),
                                            ironSmelters[j]->getIngotCount());
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                &(transporter->getOre()->type));
                            WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_DROP_ORE);
                            usleep(transPeriod);
                            ironSmelters[j]->receiveOre();
                            transporter->unloadOre();
                            ironSmelters[j]->notify();
                            unloadedOre = true;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&iSmelterMut);
                    if (unloadedOre)
                        break;

                    /* If no Iron Producer available then wait on them */
                    sem_wait(&semTransIron);
                }
                else if (transporter->getOre()->type == 1) {
                    /* COPPER */

                    /* First Check for Prioritized Copper Producers */

                    pthread_mutex_lock(&cSmelterPrioMut);
                    if (!prioritizedCopperSmelters.empty()) {
                        Smelter * prioSmelter = prioritizedCopperSmelters.front();
                        prioritizedCopperSmelters.pop();
                        pthread_mutex_unlock(&cSmelterPrioMut);

                        // travel
                        FillSmelterInfo(&smelterInfo, prioSmelter->getSmelterId(), (OreType) 0, 0, 0, 0);
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            &(transporter->getOre()->type));
                        WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_TRAVEL);
                        usleep(transPeriod);

                        // unload ore
                        FillSmelterInfo(&smelterInfo, prioSmelter->getSmelterId(), prioSmelter->getOreType(),
                                        prioSmelter->getCapacity(), prioSmelter->getWaitingOreCount(),
                                        prioSmelter->getIngotCount());
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            &(transporter->getOre()->type));
                        WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_DROP_ORE);
                        usleep(transPeriod);
                        prioSmelter->receiveOre();
                        transporter->unloadOre();
                        prioSmelter->notify();
                        break;
                    }
                    pthread_mutex_unlock(&cSmelterPrioMut);

                    /* If no Prioritized agent is present, Check for Producers with empty storage space */

                    pthread_mutex_lock(&cSmelterMut);
                    for (int j = 0; j < copperSmelters.size(); ++j) {
                        if (!copperSmelters[j]->checkQuit() and !copperSmelters[j]->isFull()) {
                            // travel
                            FillSmelterInfo(&smelterInfo, copperSmelters[j]->getSmelterId(), (OreType) 0, 0, 0, 0);
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                &(transporter->getOre()->type));
                            WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_TRAVEL);
                            usleep(transPeriod);
                            // unload it
                            FillSmelterInfo(&smelterInfo, copperSmelters[j]->getSmelterId(), copperSmelters[j]->getOreType(),
                                            copperSmelters[j]->getCapacity(), copperSmelters[j]->getWaitingOreCount(),
                                            copperSmelters[j]->getIngotCount());
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                &(transporter->getOre()->type));
                            WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_DROP_ORE);
                            usleep(transPeriod);
                            copperSmelters[j]->receiveOre();
                            transporter->unloadOre();
                            copperSmelters[j]->notify();
                            unloadedOre = true;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&cSmelterMut);
                    if (unloadedOre)
                        break;

                    /* If no Copper Producer available then wait on them */
                    sem_wait(&semTransCopper);
                }
                else if (transporter->getOre()->type == 2) {
                    /* COAL */

                    /* First Check Prioritized Coal Producers */

                    pthread_mutex_lock(&cFoundryPrioMut);
                    if (!coalPrioritizedFoundries.empty()) {
                        Foundry * prioFoundry = coalPrioritizedFoundries.front();
                        coalPrioritizedFoundries.pop();
                        pthread_mutex_unlock(&cFoundryPrioMut);

                        // travel
                        FillFoundryInfo(&foundryInfo, prioFoundry->getFoundryId(), 0, 0, 0, 0);
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            &(transporter->getOre()->type));
                        WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_TRAVEL);
                        usleep(transPeriod);

                        // unload ore
                        FillFoundryInfo(&foundryInfo, prioFoundry->getFoundryId(), prioFoundry->getCapacity(),
                                        prioFoundry->getWaitingIronCount(), prioFoundry->getWaitingCoalCount(),
                                        prioFoundry->getIngotCount());
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            &(transporter->getOre()->type));
                        WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_DROP_ORE);
                        usleep(transPeriod);
                        prioFoundry->receiveIron();
                        transporter->unloadOre();
                        prioFoundry->notify();
                        break;
                    }
                    pthread_mutex_unlock(&cFoundryPrioMut);

                    /* If no Prioritized agent is present, Check for Producers with empty storage space */

                    pthread_mutex_lock(&foundryMut);
                    for (int j = 0; j < foundries.size(); ++j) {
                        if (!foundries[j]->checkQuit() and !foundries[j]->isFullCoal()) {
                            // travel
                            FillFoundryInfo(&foundryInfo, foundries[j]->getFoundryId(), 0, 0, 0, 0);
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                &(transporter->getOre()->type));
                            WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_TRAVEL);
                            usleep(transPeriod);
                            // unload it
                            FillFoundryInfo(&foundryInfo, foundries[j]->getFoundryId(), foundries[j]->getCapacity(),
                                            foundries[j]->getWaitingIronCount(), foundries[j]->getWaitingCoalCount(),
                                            foundries[j]->getIngotCount());
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                &(transporter->getOre()->type));
                            WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_DROP_ORE);
                            usleep(transPeriod);
                            foundries[j]->receiveCoal();
                            transporter->unloadOre();
                            foundries[j]->notify();
                            unloadedOre = true;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&foundryMut);
                    if (unloadedOre)
                        break;

                    /* if no Coal Producer available then wait on them */
                    sem_wait(&semTransCoal);
                }
            }
        }
        else if (allMinersExitted) {
            /* If all miners exitted, quit. */

            pthread_mutex_unlock (&mExitMut);
            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(), &(transporter->getOre()->type));
            WriteOutput(NULL, &transporterInfo, NULL, NULL, TRANSPORTER_STOPPED);

            // Dont forget to notify next sleeping transporter
            sem_post(&semTransMiners);
            break;
        }
        else {
            pthread_mutex_unlock (&mExitMut);

            /* oreNotAvailable in any of the miners so wait */
            sem_wait(&semTransMiners);
        }
    }
    return NULL;
}

void *SmelterMain(void *p) {
    /* Smelter main routine */
    Smelter *smelter = (Smelter *) p;
    double sPeriod = smelter->getPeriod() - (smelter->getPeriod()*0.01) + (rand()%(int)(smelter->getPeriod()*0.02));
    bool qFlag = false;

    SmelterInfo smelterInfo;
    FillSmelterInfo(&smelterInfo, smelter->getSmelterId(), smelter->getOreType(), smelter->getCapacity(),
                    smelter->getWaitingOreCount(), smelter->getIngotCount());
    WriteOutput(NULL, NULL, &smelterInfo, NULL, SMELTER_CREATED);

    while (true) {
        if (!smelter->hasEnoughOres()) {
            if (!smelter->isEmpty()) {
                /* if smelter waits for only 1 ore add it to prioritized smelter queue */
                if (smelter->getOreType() == IRON) {
                    pthread_mutex_lock (&iSmelterPrioMut);
                    prioritizedIronSmelters.push(smelter);
                    pthread_mutex_unlock (&iSmelterPrioMut);
                }
                else {
                    pthread_mutex_lock (&cSmelterPrioMut);
                    prioritizedCopperSmelters.push(smelter);
                    pthread_mutex_unlock (&cSmelterPrioMut);
                }
            }

            /* create a timer thread for 5 second timeout */

            StimerParam * stp = new StimerParam;
            stp->id = smelter->getSmelterId() - 1;
            int timerId = smelter->addTimer();
            stp->timerId = timerId;
            pthread_t tid;
            pthread_create(&tid, NULL, sTimer, (void *) stp);
            pthread_detach(tid);
            smelter->ClearTimeout();

            // wait on smelter
            while (!smelter->hasEnoughOres()) {
                smelter->wait();
                if (smelter->CheckTimeout()) {
                    /* Timeout -- quit */
                    smelter->setQuit();
                    qFlag = true;
                    FillSmelterInfo(&smelterInfo, smelter->getSmelterId(), smelter->getOreType(), smelter->getCapacity(),
                                    smelter->getWaitingOreCount(), smelter->getIngotCount());
                    WriteOutput(NULL, NULL, &smelterInfo, NULL, SMELTER_STOPPED);
                    break;
                }
            }
            smelter->getTimers()[timerId] = false;
        }
        if (qFlag)
            break;

        // ProduceIngot
        FillSmelterInfo(&smelterInfo, smelter->getSmelterId(), smelter->getOreType(), smelter->getCapacity(),
                        smelter->getWaitingOreCount(), smelter->getIngotCount());
        WriteOutput(NULL, NULL, &smelterInfo, NULL, SMELTER_STARTED);
        usleep(sPeriod);
        smelter->produceIngot();

        if (smelter->getOreType() == IRON)
            sem_post(&semTransIron);
        else
            sem_post(&semTransCopper);

        FillSmelterInfo(&smelterInfo, smelter->getSmelterId(), smelter->getOreType(), smelter->getCapacity(),
                        smelter->getWaitingOreCount(), smelter->getIngotCount());
        WriteOutput(NULL, NULL, &smelterInfo, NULL, SMELTER_FINISHED);
    }
    return NULL;
}

void *FoundryMain(void *p) {
    /* Foundry main routine */
    Foundry *foundry = (Foundry *) p;
    double fPeriod = foundry->getPeriod() - (foundry->getPeriod()*0.01) + (rand()%(int)(foundry->getPeriod()*0.02));
    bool qFlag = false;

    FoundryInfo foundryInfo;
    FillFoundryInfo(&foundryInfo, foundry->getFoundryId(), foundry->getCapacity(), foundry->getWaitingIronCount(),
            foundry->getWaitingCoalCount(), foundry->getIngotCount());
    WriteOutput(NULL, NULL, NULL, &foundryInfo, FOUNDRY_CREATED);

    while (true) {
        if (!foundry->hasEnoughOres()) {
            if (!foundry->isEmptyIron() and foundry->isEmptyCoal()) {
                /* if foundry has 1 iron already and waits for 1 coal add it to cPrioritized foundry queue */
                pthread_mutex_lock (&cFoundryPrioMut);
                coalPrioritizedFoundries.push(foundry);
                pthread_mutex_unlock (&cFoundryPrioMut);
            }
            else if (!foundry->isEmptyCoal() and foundry->isEmptyIron()) {
                /* if foundry has 1 coal already and waits for 1 iron add it to iPrioritized foundry queue */
                pthread_mutex_lock (&iFoundryPrioMut);
                ironPrioritizedFoundries.push(foundry);
                pthread_mutex_unlock (&iFoundryPrioMut);
            }

            /* create a timer thread for 5 second timeout */

            FtimerParam * ftp = new FtimerParam;
            ftp->id = foundry->getFoundryId() - 1;
            int timerId = foundry->addTimer();
            ftp->timerId = timerId;
            pthread_t tid;
            pthread_create(&tid, NULL, fTimer, (void *) ftp);
            pthread_detach(tid);
            foundry->ClearTimeout();

            // wait on foundry
            while (!foundry->hasEnoughOres()) {
                foundry->wait();
                if (foundry->CheckTimeout()) {
                    /* Timeout -- quit */
                    foundry->setQuit();
                    qFlag = true;
                    FillFoundryInfo(&foundryInfo, foundry->getFoundryId(), foundry->getCapacity(),
                                    foundry->getWaitingIronCount(), foundry->getWaitingCoalCount(), foundry->getIngotCount());
                    WriteOutput(NULL, NULL, NULL, &foundryInfo, FOUNDRY_STOPPED);
                    break;
                }
            }
            foundry->getTimers()[timerId] = false;
        }
        if (qFlag)
            break;

        // ProduceIngot
        FillFoundryInfo(&foundryInfo, foundry->getFoundryId(), foundry->getCapacity(), foundry->getWaitingIronCount(),
                foundry->getWaitingCoalCount(), foundry->getIngotCount());
        WriteOutput(NULL, NULL, NULL, &foundryInfo, FOUNDRY_STARTED);
        usleep(fPeriod);
        foundry->produceIngot();

        sem_post(&semTransIron);
        sem_post(&semTransCoal);

        FillFoundryInfo(&foundryInfo, foundry->getFoundryId(), foundry->getCapacity(), foundry->getWaitingIronCount(),
                foundry->getWaitingCoalCount(), foundry->getIngotCount());
        WriteOutput(NULL, NULL, NULL, &foundryInfo, FOUNDRY_FINISHED);
    }
    return NULL;
}

void *sTimer(void *p) {
    /* 5 Seconds Timer for smelters */
    StimerParam *stp = (StimerParam *) p;
    sleep(5);
    pthread_mutex_lock (&smelterMut);
    if (smelters[stp->id] and smelters[stp->id]->getTimers()[stp->timerId]) {
        smelters[stp->id]->SetTimeout();
        smelters[stp->id]->notify();
    }
    pthread_mutex_unlock(&smelterMut);
    return NULL;
}

void *fTimer(void *p) {
    /* 5 Seconds Timer for foundries */
    FtimerParam *ftp = (FtimerParam *) p;
    sleep(5);
    pthread_mutex_lock (&foundryMut);
    if (foundries[ftp->id] and foundries[ftp->id]->getTimers()[ftp->timerId]) {
        foundries[ftp->id]->SetTimeout();
        foundries[ftp->id]->notify();
    }
    pthread_mutex_unlock (&foundryMut);
    return NULL;
}
