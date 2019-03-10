# MapReduce

Implementation for MapReduce programming model which is associated with processing and generating big data sets with a parallel and distributed algorithm on a platform.

## Usage
```
git clone https://github.com/ysyesilyurt/OS-2019
cd mapreduce
make all
./mapreduce N MapperPath ReducerPath
```
Where ```N``` is the number of mappers and reducers, ```Mapper``` is the name of the Mapper executable and ```Reducer``` is the name of the Mapper executable.

There are sample Map & Reduce algorithms with sample i/o and Makefiles under ```samples/src```

#### Example usage
```
./mapreduce 5 samples/WC/src/WC_Mapper samples/WC/src/WC_Reducer > out.txt < samples/WC/input/input.txt
```