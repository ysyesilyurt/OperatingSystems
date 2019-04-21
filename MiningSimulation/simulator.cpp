#include <pthread.h>
#include <unistd.h>
#include <queue>

#include "monitor.hpp"
#include "miner.hpp"
#include "transporter.hpp"
#include "ingoters.hpp"

void initSimulation(pthread_t *, pthread_t *, pthread_t *, pthread_t *, int &, int &, int &, int &);
void *MinerMain(void *);
void *TransporterMain(void *);
void *SmelterMain(void *);
void *FoundryMain(void *);
void *sTimer(void *);
void *fTimer(void *);

/* global variables */

pthread_mutex_t minerMut =  PTHREAD_MUTEX_INITIALIZER;
int lastMiner = 0;
pthread_mutex_t iSmelterMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cSmelterMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t foundryMut =  PTHREAD_MUTEX_INITIALIZER;

std::vector<Miner> miners;
std::vector<Transporter> transporters;
std::vector<Smelter> ironSmelters;
std::vector<Smelter> copperSmelters;
std::vector<Foundry> foundries;

pthread_mutex_t oreAvailMut =  PTHREAD_MUTEX_INITIALIZER;
bool oreNotAvailable = false;

pthread_mutex_t mExitMut =  PTHREAD_MUTEX_INITIALIZER;
int minerExitCount = 0;
bool allMinersExitted = false;

pthread_mutex_t minerTransCond_lock =  PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t minerTransCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t prodAvailMut =  PTHREAD_MUTEX_INITIALIZER;
bool prodNotAvailable = false;

pthread_mutex_t producerTransCond_lock =  PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t producerTransCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t iSmelterPrioMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cSmelterPrioMut =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t foundryPrioMut =  PTHREAD_MUTEX_INITIALIZER;

std::queue<Smelter *> prioritizedIronSmelters;
std::queue<Smelter *> prioritizedCopperSmelters;
std::queue<Foundry *> prioritizedFoundries;

struct MinerParam {
    Miner *m;
    int NumMiners;
};
struct TransParam {
    Transporter *t;
    int NumMiners;
};

int main(int argc, const char* argv[]) {

    int numMines, numTrans, numSmelters, numFoundries;
    pthread_t *minerThreads, *transThreads, *smeltThreads, *foundThreads;
    initSimulation(minerThreads, transThreads, smeltThreads, foundThreads, numMines, numTrans, numSmelters, numFoundries);

    // reap them
    for (int i = 0; i < numMines; i++)
        pthread_join(minerThreads[i], NULL);

    for (int i = 0; i < numTrans; i++)
        pthread_join(transThreads[i], NULL);

    for (int i = 0; i < numSmelters; i++)
        pthread_join(smeltThreads[i], NULL);

    for (int i = 0; i < numFoundries; i++)
        pthread_join(foundThreads[i], NULL);

    delete [] minerThreads;
    delete [] transThreads;
    delete [] smeltThreads;
    delete [] foundThreads;

    return 0;
}

void initSimulation(pthread_t *minerThreads, pthread_t *transThreads, pthread_t *smeltThreads, pthread_t *foundThreads,
        int & numMines, int & numTrans, int & numSmelters, int & numFoundries) {
    int period, cap, tA;
    unsigned int oT;
    std::cin >> numMines;
    MinerParam *mparams = new MinerParam[numMines] ;
    minerThreads = new pthread_t[numMines];
    for (int i = 0; i < numMines; ++i) {
        std::cin >> period >> cap >> oT >> tA;
        Miner miner = Miner(i, period, cap, (OreType) oT, tA);
        miners.emplace_back(miner);
        mparams[i].m = &miner;
        mparams[i].NumMiners = numMines;
        pthread_create(&minerThreads[i], NULL, MinerMain, (void *) (mparams + i));
    }

    std::cin >> numTrans;
    TransParam *tparams = new TransParam[numTrans] ;
    transThreads = new pthread_t[numTrans];
    for (int i = 0; i < numTrans; ++i) {
        std::cin >> period;
        Transporter transporter = Transporter(i, period);
        transporters.emplace_back(transporter);
        tparams[i].t = &transporter;
        tparams[i].NumMiners = numMines;
        pthread_create(&transThreads[i], NULL, TransporterMain, (void *) (tparams + i));
    }

    std::cin >> numSmelters;
    smeltThreads = new pthread_t[numSmelters];
    for (int i = 0; i < numSmelters; ++i) {
        std::cin >> period >> cap >> oT;
        Smelter smelter = Smelter(i, period, cap, (OreType) oT);
        if (oT) {
            // oT = copper
            copperSmelters.emplace_back(smelter);
        }
        else {
            // oT = iron
            ironSmelters.emplace_back(smelter);
        }
        pthread_create(&smeltThreads[i], NULL, SmelterMain, (void *) &smelter);
    }

    std::cin >> numFoundries;
    foundThreads = new pthread_t[numFoundries];
    for (int i = 0; i < numFoundries; ++i) {
        std::cin >> period >> cap;
        Foundry foundry = Foundry(i, period, cap);
        foundries.emplace_back(foundry);
        pthread_create(&foundThreads[i], NULL, FoundryMain, (void *) &foundry);
    }
}

void *MinerMain(void *p) {
    MinerParam *mp = (MinerParam *) p;
    Miner *miner = mp->m;
    int numMiners = mp->NumMiners;
    double mPeriod = miner->getPeriod() - (miner->getPeriod()*0.01) + (rand()%(int)(miner->getPeriod()*0.02));
    MinerInfo minerInfo;
    FillMinerInfo(&minerInfo, miner->getMinerId(), miner->getOreType(), miner->getCapacity(), miner->getCurrOreCount());
    WriteOutput(&minerInfo, NULL, NULL, NULL, MINER_CREATED);
    while (true) {
        if (miner->checkQuit()) {
            // Quit
            miner->setQuit();
            FillMinerInfo(&minerInfo, miner->getMinerId(), miner->getOreType(), miner->getCapacity(), miner->getCurrOreCount());
            WriteOutput(&minerInfo, NULL, NULL, NULL, MINER_STOPPED);
            pthread_mutex_lock (&mExitMut);
            if (minerExitCount == numMiners-1) {
                allMinersExitted = true;
                // Notify sleeping threads

                pthread_mutex_lock (&oreAvailMut);
                oreNotAvailable = false;
                pthread_mutex_unlock (&oreAvailMut);

                pthread_mutex_lock (&minerTransCond_lock);
                pthread_cond_signal (&minerTransCond);
                pthread_mutex_unlock (&minerTransCond_lock);
            }
            pthread_mutex_unlock (&mExitMut);
            break;
        }
        else {
            while (miner->isFull())
                miner->wait();

            // ProduceOre
            FillMinerInfo(&minerInfo, miner->getMinerId(), miner->getOreType(), miner->getCapacity(), miner->getCurrOreCount());
            WriteOutput(&minerInfo, NULL, NULL, NULL, MINER_STARTED);
            usleep(mPeriod);
            miner->mineOre();

            pthread_mutex_lock (&oreAvailMut);
            oreNotAvailable = false;
            pthread_mutex_unlock (&oreAvailMut);

            pthread_mutex_lock (&minerTransCond_lock);
            pthread_cond_signal (&minerTransCond);
            pthread_mutex_unlock (&minerTransCond_lock);

            FillMinerInfo(&minerInfo, miner->getMinerId(), miner->getOreType(), miner->getCapacity(), miner->getCurrOreCount());
            WriteOutput(&minerInfo, NULL, NULL, NULL, MINER_FINISHED);
            usleep(mPeriod);
        }
    }
}

void *TransporterMain(void *p) {
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
    FillTransporterInfo(&transporterInfo, transporter->getTransporterId(), transporter->getOre());
    WriteOutput(NULL, &transporterInfo, NULL, NULL, TRANSPORTER_CREATED);

    while (true) {
        pthread_mutex_lock (&minerMut);
        gotOre = false;
        minerCount = 0;
        index = lastMiner;
        while (minerCount <= numMiners) {
            /* Transporter thread miner routine */
            if (index == numMiners)
                index = 0;
            if (!miners[index].isEmpty()) {
                lastMiner = index + 1;
                TransporterInfo transporterinfo;
                FillMinerInfo(&minerInfo, miners[index].getMinerId(), (OreType) 0,0,0); // 0 0 0 ?
                FillTransporterInfo(&transporterinfo, transporter->getTransporterId(), transporter->getOre());
                WriteOutput(&minerInfo, &transporterinfo, NULL, NULL, TRANSPORTER_TRAVEL);
                usleep(transPeriod);

                // load an ore
                miners[index].removeOre();
                transporter->loadOre(OreType(miners[index].getOreType()));
                FillMinerInfo(&minerInfo, miners[index].getMinerId(), miners[index].getOreType(),
                        miners[index].getCapacity(), miners[index].getCurrOreCount());
                FillTransporterInfo(&transporterinfo, transporter->getTransporterId(), transporter->getOre());
                WriteOutput(&minerInfo, &transporterinfo, NULL, NULL, TRANSPORTER_TAKE_ORE);
                usleep(transPeriod);
                miners[index].notify();
                gotOre = true;
                break;
            }
            else if (miners[index].checkQuit()) {
                // Empty storage + exitted miner
                pthread_mutex_lock (&mExitMut);
                minerExitCount++;
                pthread_mutex_unlock (&mExitMut);
            }
            minerCount++;
            index++;
        }
        pthread_mutex_unlock (&minerMut);

        pthread_mutex_lock (&mExitMut);
        if (gotOre) {
            unloadedOre = false;
            pthread_mutex_unlock (&mExitMut);
            while (true) {
                if (*(transporter->getOre()) == 0) {
                    // IRON

                    /* First Check Prioritized Producers */
                    pthread_mutex_lock(&foundryPrioMut);
                    if (!prioritizedFoundries.empty()) {
                        Foundry * prioFoundry = prioritizedFoundries.front();
                        prioritizedFoundries.pop();
                        // travel
                        FillFoundryInfo(&foundryInfo, prioFoundry->getFoundryId(), 0, 0, 0, 0); // 0000?
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            transporter->getOre());
                        WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_TRAVEL);
                        usleep(transPeriod);
                        // unload it
                        prioFoundry->receiveIron();
                        transporter->unloadOre();
                        FillFoundryInfo(&foundryInfo, prioFoundry->getFoundryId(), prioFoundry->getCapacity(),
                                        prioFoundry->getWaitingIronCount(), prioFoundry->getWaitingCoalCount(),
                                        prioFoundry->getIngotCount());
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            transporter->getOre());
                        WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_DROP_ORE);
                        usleep(transPeriod);
                        prioFoundry->notify();
                        break;
                    }
                    pthread_mutex_unlock(&foundryPrioMut);

                    pthread_mutex_lock(&iSmelterPrioMut);
                    if (!prioritizedIronSmelters.empty()) {
                        Smelter * prioSmelter = prioritizedIronSmelters.front();
                        prioritizedIronSmelters.pop();
                        // travel
                        FillSmelterInfo(&smelterInfo, prioSmelter->getSmelterId(), (OreType) 0, 0, 0, 0); // 000?
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            transporter->getOre());
                        WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_TRAVEL);
                        usleep(transPeriod);
                        // unload it
                        prioSmelter->receiveOre();
                        transporter->unloadOre();
                        FillSmelterInfo(&smelterInfo, prioSmelter->getSmelterId(), prioSmelter->getOreType(),
                                        prioSmelter->getCapacity(), prioSmelter->getWaitingOreCount(),
                                        prioSmelter->getIngotCount());
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            transporter->getOre());
                        WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_DROP_ORE);
                        usleep(transPeriod);
                        prioSmelter->notify();
                        break;
                    }
                    pthread_mutex_unlock(&iSmelterPrioMut);

                    /* Then Check for Producers with empty storage space */
                    pthread_mutex_lock(&foundryMut);
                    for (int j = 0; j < foundries.size(); ++j) {
                        if (!foundries[j].checkQuit() and !foundries[j].isFullIron()) {
                            // travel
                            FillFoundryInfo(&foundryInfo, foundries[j].getFoundryId(), 0, 0, 0, 0); // 0000?
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                transporter->getOre());
                            WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_TRAVEL);
                            usleep(transPeriod);
                            // unload it
                            foundries[j].receiveIron();
                            transporter->unloadOre();
                            FillFoundryInfo(&foundryInfo, foundries[j].getFoundryId(), foundries[j].getCapacity(),
                                            foundries[j].getWaitingIronCount(), foundries[j].getWaitingCoalCount(),
                                            foundries[j].getIngotCount());
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                transporter->getOre());
                            WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_DROP_ORE);
                            usleep(transPeriod);
                            foundries[j].notify();
                            unloadedOre = true;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&foundryMut);
                    if (unloadedOre)
                        break;

                    pthread_mutex_lock(&iSmelterMut);
                    for (int j = 0; j < ironSmelters.size(); ++j) {
                        if (!ironSmelters[j].checkQuit() and !ironSmelters[j].isFull()) {
                            // travel
                            FillSmelterInfo(&smelterInfo, ironSmelters[j].getSmelterId(), (OreType) 0, 0, 0, 0); // 000?
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                transporter->getOre());
                            WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_TRAVEL);
                            usleep(transPeriod);
                            // unload it
                            ironSmelters[j].receiveOre();
                            transporter->unloadOre();
                            FillSmelterInfo(&smelterInfo, ironSmelters[j].getSmelterId(), ironSmelters[j].getOreType(),
                                            ironSmelters[j].getCapacity(), ironSmelters[j].getWaitingOreCount(),
                                            ironSmelters[j].getIngotCount());
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                transporter->getOre());
                            WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_DROP_ORE);
                            usleep(transPeriod);
                            ironSmelters[j].notify();
                            unloadedOre = true;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&iSmelterMut);
                    if (unloadedOre)
                        break;
                }
                else if (*(transporter->getOre()) == 1) {
                    // COPPER

                    /* First Check Prioritized Producers */
                    pthread_mutex_lock(&cSmelterPrioMut);
                    if (!prioritizedCopperSmelters.empty()) {
                        Smelter * prioSmelter = prioritizedCopperSmelters.front();
                        prioritizedCopperSmelters.pop();
                        // travel
                        FillSmelterInfo(&smelterInfo, prioSmelter->getSmelterId(), (OreType) 0, 0, 0, 0); // 000?
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            transporter->getOre());
                        WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_TRAVEL);
                        usleep(transPeriod);
                        // unload it
                        prioSmelter->receiveOre();
                        transporter->unloadOre();
                        FillSmelterInfo(&smelterInfo, prioSmelter->getSmelterId(), prioSmelter->getOreType(),
                                        prioSmelter->getCapacity(), prioSmelter->getWaitingOreCount(),
                                        prioSmelter->getIngotCount());
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            transporter->getOre());
                        WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_DROP_ORE);
                        usleep(transPeriod);
                        prioSmelter->notify();
                        break;
                    }
                    pthread_mutex_unlock(&cSmelterPrioMut);

                    /* Then Check for Producers with empty storage space */
                    pthread_mutex_lock(&cSmelterMut);
                    for (int j = 0; j < copperSmelters.size(); ++j) {
                        if (!copperSmelters[j].checkQuit() and !copperSmelters[j].isFull()) {
                            // travel
                            FillSmelterInfo(&smelterInfo, copperSmelters[j].getSmelterId(), (OreType) 0, 0, 0, 0); // 000?
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                transporter->getOre());
                            WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_TRAVEL);
                            usleep(transPeriod);
                            // unload it
                            copperSmelters[j].receiveOre();
                            transporter->unloadOre();
                            FillSmelterInfo(&smelterInfo, copperSmelters[j].getSmelterId(), copperSmelters[j].getOreType(),
                                            copperSmelters[j].getCapacity(), copperSmelters[j].getWaitingOreCount(),
                                            copperSmelters[j].getIngotCount());
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                transporter->getOre());
                            WriteOutput(NULL, &transporterInfo, &smelterInfo, NULL, TRANSPORTER_DROP_ORE);
                            usleep(transPeriod);
                            copperSmelters[j].notify();
                            unloadedOre = true;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&cSmelterMut);
                    if (unloadedOre)
                        break;
                }
                else if (*(transporter->getOre()) == 2) {
                    // COAL

                    /* First Check Prioritized Producers */
                    pthread_mutex_lock(&foundryPrioMut);
                    if (!prioritizedFoundries.empty()) {
                        Foundry * prioFoundry = prioritizedFoundries.front();
                        prioritizedFoundries.pop();
                        // travel
                        FillFoundryInfo(&foundryInfo, prioFoundry->getFoundryId(), 0, 0, 0, 0); // 0000?
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            transporter->getOre());
                        WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_TRAVEL);
                        usleep(transPeriod);
                        // unload it
                        prioFoundry->receiveIron();
                        transporter->unloadOre();
                        FillFoundryInfo(&foundryInfo, prioFoundry->getFoundryId(), prioFoundry->getCapacity(),
                                        prioFoundry->getWaitingIronCount(), prioFoundry->getWaitingCoalCount(),
                                        prioFoundry->getIngotCount());
                        FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                            transporter->getOre());
                        WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_DROP_ORE);
                        usleep(transPeriod);
                        prioFoundry->notify();
                        break;
                    }
                    pthread_mutex_unlock(&foundryPrioMut);

                    /* Then Check for Producers with empty storage space */
                    pthread_mutex_lock(&foundryMut);
                    for (int j = 0; j < foundries.size(); ++j) {
                        if (!foundries[j].checkQuit() and !foundries[j].isFullCoal()) {
                            // travel
                            FillFoundryInfo(&foundryInfo, foundries[j].getFoundryId(), 0, 0, 0, 0); // 0000?
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                transporter->getOre());
                            WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_TRAVEL);
                            usleep(transPeriod);
                            // unload it
                            foundries[j].receiveCoal();
                            transporter->unloadOre();
                            FillFoundryInfo(&foundryInfo, foundries[j].getFoundryId(), foundries[j].getCapacity(),
                                            foundries[j].getWaitingIronCount(), foundries[j].getWaitingCoalCount(),
                                            foundries[j].getIngotCount());
                            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(),
                                                transporter->getOre());
                            WriteOutput(NULL, &transporterInfo, NULL, &foundryInfo, TRANSPORTER_DROP_ORE);
                            usleep(transPeriod);
                            foundries[j].notify();
                            unloadedOre = true;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&foundryMut);
                    if (unloadedOre)
                        break;
                }

                /* if no producer available then wait on them */
                pthread_mutex_lock (&prodAvailMut);
                prodNotAvailable = true;
                pthread_mutex_lock (&producerTransCond_lock);
                // While prodNotAvailable -- wait
                while (prodNotAvailable)
                    pthread_cond_wait (&producerTransCond, &producerTransCond_lock);
                pthread_mutex_unlock (&producerTransCond_lock);
                prodNotAvailable = true;
                pthread_mutex_unlock (&prodAvailMut);
            }
        }
        else if (allMinersExitted) {
            // if all miners exitted and transporter in here, then quit
            pthread_mutex_unlock (&mExitMut);
            FillTransporterInfo(&transporterInfo, transporter->getTransporterId(), transporter->getOre());
            WriteOutput(NULL, &transporterInfo, NULL, NULL, TRANSPORTER_STOPPED);

            // notify next sleeping transporter
            pthread_mutex_lock (&oreAvailMut);
            oreNotAvailable = false;
            pthread_mutex_unlock (&oreAvailMut);

            pthread_mutex_lock (&minerTransCond_lock);
            pthread_cond_signal (&minerTransCond);
            pthread_mutex_unlock (&minerTransCond_lock);

            break;
        }
        else {
            pthread_mutex_lock (&oreAvailMut);
            oreNotAvailable = true;
            pthread_mutex_lock (&minerTransCond_lock);
            // While oreNotAvailable -- wait
            while (oreNotAvailable)
                pthread_cond_wait (&minerTransCond, &minerTransCond_lock);
            pthread_mutex_unlock (&minerTransCond_lock);
            oreNotAvailable = true;
            pthread_mutex_unlock (&oreAvailMut);
        }
    }
}

void *SmelterMain(void *p) {
    Smelter *smelter = (Smelter *) p;
    double sPeriod = smelter->getPeriod() - (smelter->getPeriod()*0.01) + (rand()%(int)(smelter->getPeriod()*0.02));
    bool qFlag = false;
    SmelterInfo smelterInfo;
    FillSmelterInfo(&smelterInfo, smelter->getSmelterId(), smelter->getOreType(), smelter->getCapacity(), smelter->getWaitingOreCount(), smelter->getIngotCount());
    WriteOutput(NULL, NULL, &smelterInfo, NULL, SMELTER_CREATED);
    while (true) {
        smelter->timerStop = false;
        while (!smelter->hasEnoughOres()) {
            if (!smelter->isEmpty()) {
                // if smelter waits for only 1 ore add it to prioritized smelter queue
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
            // create a timer thread
            pthread_t tid;
            pthread_create(&tid, NULL, sTimer, (void *) &smelter);
            pthread_detach(tid); // no &tid !!
            // wait on smelter
            smelter->wait();
            smelter->timerStop = true;
            if (smelter->checkQuit()) {
                // Quit due to timer
                qFlag = true;
                FillSmelterInfo(&smelterInfo, smelter->getSmelterId(), smelter->getOreType(), smelter->getCapacity(), smelter->getWaitingOreCount(), smelter->getIngotCount());
                WriteOutput(NULL, NULL, &smelterInfo, NULL, SMELTER_STOPPED);
                break;
            }
        }
        if (qFlag)
            break;
        // ProduceIngot
        FillSmelterInfo(&smelterInfo, smelter->getSmelterId(), smelter->getOreType(), smelter->getCapacity(), smelter->getWaitingOreCount(), smelter->getIngotCount());
        WriteOutput(NULL, NULL, &smelterInfo, NULL, SMELTER_STARTED);
        usleep(sPeriod);
        smelter->produceIngot();

        pthread_mutex_lock (&prodAvailMut);
        prodNotAvailable = false;
        pthread_mutex_unlock (&prodAvailMut);

        pthread_mutex_lock (&producerTransCond_lock);
        pthread_cond_signal (&producerTransCond);
        pthread_mutex_unlock (&producerTransCond_lock);

        FillSmelterInfo(&smelterInfo, smelter->getSmelterId(), smelter->getOreType(), smelter->getCapacity(), smelter->getWaitingOreCount(), smelter->getIngotCount());
        WriteOutput(NULL, NULL, &smelterInfo, NULL, SMELTER_FINISHED);
    }
}

void *FoundryMain(void *p) {
    Foundry *foundry = (Foundry *) p;
    double fPeriod = foundry->getPeriod() - (foundry->getPeriod()*0.01) + (rand()%(int)(foundry->getPeriod()*0.02));
    bool qFlag = false;
    FoundryInfo foundryInfo;
    FillFoundryInfo(&foundryInfo, foundry->getFoundryId(), foundry->getCapacity(), foundry->getWaitingIronCount(), foundry->getWaitingCoalCount(), foundry->getIngotCount());
    WriteOutput(NULL, NULL, NULL, &foundryInfo, FOUNDRY_CREATED);
    while (true) {
        foundry->timerStop = false;
        while (!foundry->hasEnoughOres()) {
            if ((!foundry->isEmptyIron() and foundry->isEmptyCoal()) or (foundry->isEmptyIron() and !foundry->isEmptyCoal()) ) {
                // if foundry waits for only 1 ore add it to prioritized foundry queue
                pthread_mutex_lock (&foundryPrioMut);
                prioritizedFoundries.push(foundry);
                pthread_mutex_unlock (&foundryPrioMut);
            }
            // create a timer thread
            pthread_t tid;
            pthread_create(&tid, NULL, fTimer, (void *) &foundry);
            pthread_detach(tid); // no &tid !!
            // wait on foundry
            foundry->wait();
            foundry->timerStop = true;
            if (foundry->checkQuit()) {
                // Quit due to timer
                qFlag = true;
                FillFoundryInfo(&foundryInfo, foundry->getFoundryId(), foundry->getCapacity(), foundry->getWaitingIronCount(), foundry->getWaitingCoalCount(), foundry->getIngotCount());
                WriteOutput(NULL, NULL, NULL, &foundryInfo, FOUNDRY_STOPPED);
                break;
            }
        }
        if (qFlag)
            break;
        // ProduceIngot
        FillFoundryInfo(&foundryInfo, foundry->getFoundryId(), foundry->getCapacity(), foundry->getWaitingIronCount(), foundry->getWaitingCoalCount(), foundry->getIngotCount());
        WriteOutput(NULL, NULL, NULL, &foundryInfo, FOUNDRY_STARTED);
        usleep(fPeriod);
        foundry->produceIngot();

        pthread_mutex_lock (&prodAvailMut);
        prodNotAvailable = false;
        pthread_mutex_unlock (&prodAvailMut);

        pthread_mutex_lock (&producerTransCond_lock);
        pthread_cond_signal (&producerTransCond);
        pthread_mutex_unlock (&producerTransCond_lock);

        FillFoundryInfo(&foundryInfo, foundry->getFoundryId(), foundry->getCapacity(), foundry->getWaitingIronCount(), foundry->getWaitingCoalCount(), foundry->getIngotCount());
        WriteOutput(NULL, NULL, NULL, &foundryInfo, FOUNDRY_FINISHED);
    }
}

void *sTimer(void *p) {
    Smelter *s = (Smelter *) p;
    usleep(s->getPeriod());
    if (!s->timerStop) {
        s->setQuit();
        s->notify();
    }
}

void *fTimer(void *p) {
    Foundry *f = (Foundry *) p;
    usleep(f->getPeriod());
    if (!f->timerStop) {
        f->setQuit();
        f->notify();
    }
}
