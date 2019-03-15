# MapReduce

C Implementation for MapReduce programming model which is associated with processing and generating big data sets with a parallel and distributed algorithm on a platform.

## Usage
```
git clone https://github.com/ysyesilyurt/OS-2019
cd mapreduce
make all
./mapreduce N MapperPath ReducerPath
```
Where ```N``` is the number of mappers and reducers, ```MapperPath``` is the path of the Mapper executable and ```ReducerPath``` is the path of the Mapper executable. 

It can perform just the Map model rather than MapReduce if you provide just ```MapperPath``` and ```N``` as the following:

```
./mapreduce N MapperPath 
```

Note that, in both models, executing ```Mapper``` & ```Reducer``` are given their corresponding ```MapperID``` & ```ReducerID``` as parameters respectively.

There are sample MapReduce algorithms with sample i/o and Makefiles under ```samples``` folder.

#### Example usage
```
./mapreduce 5 samples/word_count/src/WC_Mapper samples/word_count/src/WC_Reducer > out.txt < samples/WC/input/input.txt
```