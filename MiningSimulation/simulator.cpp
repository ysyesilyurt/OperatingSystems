#include <pthread.h>
#include <unistd.h>
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

pthread_mutex_t transCond_lock =  PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t transCond = PTHREAD_COND_INITIALIZER;
int lastMiner = 0;

/* maybe pQ + its mutex */

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
    minerThreads = new pthread_t[numMines];
    for (int i = 0; i < numMines; ++i) {
        std::cin >> period >> cap >> oT >> tA;
        Miner miner = Miner(i, period, cap, (OreType) oT, tA);
        miners.emplace_back(miner);
        pthread_create(&minerThreads[i], NULL, MinerMain, (void *) &miner);
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
    Miner *miner = (Miner *) p;
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
            pthread_mutex_lock (&transCond_lock);
            pthread_cond_signal (&transCond);
            pthread_mutex_unlock (&transCond_lock);

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
    bool interiorFlag, gotOre;
    int exitCount = 0, minerCount, index;
    double transPeriod = transporter->getPeriod() - (transporter->getPeriod()*0.01) + (rand()%(int)(transporter->getPeriod()*0.02));

    TransporterInfo transporterInfo;
    FillTransporterInfo(&transporterInfo, transporter->getTransporterId(), transporter->getOre());
    WriteOutput(NULL, &transporterInfo, NULL, NULL, TRANSPORTER_CREATED);

    while (exitCount != numMiners) {
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
                MinerInfo minerInfo;
                TransporterInfo transporterinfo;
                FillMinerInfo(&minerInfo, miners[index].getMinerId(), (OreType) 0,0,0);
                FillTransporterInfo(&transporterinfo, transporter->getTransporterId(), transporter->getOre());
                WriteOutput(&minerInfo, &transporterinfo, NULL, NULL, TRANSPORTER_TRAVEL);
                usleep(transPeriod);

                // load an ore
                miners[index].removeOre();
                transporter->loadOre(OreType(miners[index].getOreType()));
                FillMinerInfo(&minerInfo, miners[index].getMinerId(), miners[index].getOreType(), miners[index].getCapacity(), miners[index].getCurrOreCount());
                FillTransporterInfo(&transporterinfo, transporter->getTransporterId(), transporter->getOre());
                WriteOutput(&minerInfo, &transporterinfo, NULL, NULL, TRANSPORTER_TAKE_ORE);
                usleep(transPeriod);
                miners[index].notify();
                gotOre = true;
                break;
            }
            else if (miners[index].checkQuit())
                exitCount++;

            minerCount++;
            index++;
        }
        pthread_mutex_unlock (&minerMut);

        if (gotOre) {
            // TODO: Implement Transporter producers Main routines!!
        }
        else {
            pthread_mutex_lock (&oreAvailMut);
            oreNotAvailable = true;
            pthread_mutex_lock (&transCond_lock);
            // While oreNotAvailable -- wait
            while (oreNotAvailable)
                pthread_cond_wait (&transCond, &transCond_lock);
            pthread_mutex_unlock (&transCond_lock);
            oreNotAvailable = true;
            pthread_mutex_unlock (&oreAvailMut);
        }
    }
    FillTransporterInfo(&transporterInfo, transporter->getTransporterId(), transporter->getOre());
    WriteOutput(NULL, &transporterInfo, NULL, NULL, TRANSPORTER_STOPPED);
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
