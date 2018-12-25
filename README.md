# Memory Containers

## Overview

This is a kernel module that allows Linux kernel to allocate memory to containers. A new process can be assigned with one of the containers. The containers allocate and set limits on total memory that can be consumed by processes inside them.

### Kernel Compilation
```shell
cd kernel_module
sudo make clean
sudo make
sudo make install
cd ..
```

### User Space Library Compilation
```shell
cd library
sudo make clean
sudo make
sudo make install
cd ..
```

### Benchmark Compilation
```shell
cd benchmark
make clean
make
cd ..
```

### Run
```shell
./test.sh <num of objects> <max size of objects> <num of tasks> <num of containers>

# example
# growing on the number of processes 
./test.sh 128 4096 1 1
./test.sh 128 4096 2 1
./test.sh 128 4096 4 1

# growing on the number of objects
./test.sh 128 4096 1 1
./test.sh 512 4096 1 1
./test.sh 1024 4096 1 1
./test.sh 4096 4096 1 1

# growing on the object size
./test.sh 128 4096 1 1
./test.sh 128 8192 1 1

# growing on the number of containers
./test.sh 128 4096 2 2
./test.sh 128 4096 8 8
./test.sh 128 4096 64 64

# combination
./test.sh 256 8192 8 4
```