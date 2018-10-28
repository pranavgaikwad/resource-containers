#!/bin/bash

#!/bin/bash

FILE_BNMARK='benchmark/benchmark.c'
FILE_MODULE='kernel_module/src/ioctl.c'

HASH_BNMARK='.benchmark.sha1'
HASH_MODULE='.kernel_module.sha1'

if [ ! -f ${HASH_BNMARK} ]; then
    touch ${HASH_BNMARK}
fi
if [ ! -f ${HASH_MODULE} ]; then
    touch ${HASH_MODULE}
fi

sha1sum -c ${HASH_BNMARK}

change_status="$?"

if [ ${change_status} -ne 0 ]; then
    sha1sum ${FILE_BNMARK} > ${HASH_BNMARK}
    cd benchmark && make && cd ..
fi

sha1sum -c ${HASH_MODULE}

change_status="$?"

output=0

if [ "$change_status" -ne 0 ]; then
    sha1sum ${FILE_MODULE} > ${HASH_MODULE}
    cd kernel_module
    sudo make > /dev/null
    output="$?"
    if [ ${output} -eq 0 ]; then
        sudo make install > /dev/null
        output="$?"
    else
        >&2 echo "Error Occurred. Exiting..."
        exit
    fi
    cd ..
fi

if [ ${output} -eq 0 ]; then

    # Parse input
    if [ $# -ne 4 ]; then
        echo "Usage: $0 <# of objects> <max size of objects> <# of tasks> <# of containers>"
        exit
    fi

    number_of_objects=$1 
    max_size_of_objects=$2 
    number_of_processes=$3
    number_of_containers=$4

    sudo insmod kernel_module/memory_container.ko
    sudo chmod 777 /dev/mcontainer
    ./benchmark/benchmark $1 $2 $3 $4
    cat *.log > trace
    sort -n -k 4 trace > sorted_trace
    ./benchmark/validate $1 $2 $4 < sorted_trace

    # if you want to see the log for debugging, comment out the following line.
    # sudo rm -f *.log trace sorted_trace

    sudo rmmod memory_container
else
    >&2 echo "Error Occurred"
    exit
fi