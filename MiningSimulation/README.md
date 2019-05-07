# MiningSimulation

Implementation for a Mining Simulation in which several agents of type _Miner_ produce _Ores_ in accordance with their production times and capacities and several agents of type _Transporter_
carries these _Ores_ with their transportation conditions to several agents of type _Smelter_ or _Foundry_ according to _Ore_ type of its current _Ore_. Agent of type _Smelter_ produce 
_Iron_ or _Copper_ ingots and _Foundries_ produce _Steel_ ingots using different combinations of these _Ores_ with their production times and capacities respectively.

_Miners_ quit if they produce their maximum number of _Ores_, _Transporters_ quit if no _Miner_ left and no _Ores_ left in any of the _Miner's_ storage, _Smelters_ and _Foundries_ quit if
they can't produce _Ingots_ (due to lack of _Ores_) for 5 seconds.

#### Keywords
```
Synchronization, Thread, Semaphore, Mutex, Condition Variable
```

## Usage
```
git clone https://github.com/ysyesilyurt/OS-2019
cd MiningSimulation
make all
./simulator < inp.txt > out.txt
```
Input format is given under ```tests/inputs``` folder.

There are sample i/o with a tester under ```tests``` folder. You can just go ```tests``` folder and run tester:
```
./tester.py
```

It will run all the inputs in the form ```inp#.txt``` under ```tests/inputs``` folder and output to ```tests/outputs``` folder with its correponding number.
