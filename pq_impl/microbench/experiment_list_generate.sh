#!/bin/bash

source ../config.mk
rm -f experiment_list.txt > /dev/null

# variables that change
max_key=100000000
prefill=1000000
heap_list_size=100000000
duration=5000
binding_policy="0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92"
ins="0 50 95 100"
ins_paths="50 95"
threads_paths="1 24 48 72 96"
threads="1 12 24 36 48 60 72 84 96"
threads_desg="2 12 24 48 72 96"
ops_per_thread="1000 10000 100000 1000000"
datastructures="numa_pq_lin linden lotan_shavit spraylist smq numa_pq_rel" # note: the ds names must match their executable name
datastructures_strict="pipq linden lotan_shavit"
# datastructures_relaxed="numa_pq_rel spraylist smq"
# datastructures_strict_plus_spray="numa_pq_lin linden lotan_shavit spraylist"

# $1 - name of experiment ("micro", "desg", "phased")
prepare_exp() {
  echo 0 0 $1 prepare
}

run_microbenchmark() {
    echo "Preparing experiment 1: Microbenchmark"
    prepare_exp "micro" >> experiment_list.txt

    benchmark=3
    for u in $ins ; do
    for ds in $datastructures_strict ; do
    for n in $threads ; do
        echo $benchmark $ds $n $u  >> experiment_list.txt
    done
    done
    done
}

run_phased() {
    echo "Preparing experiment 2: Phased Experiment"
    prepare_exp "phased" >> experiment_list.txt

    benchmark=5
    #phased_workload="2,100:50000000,0:1000000 2,100:50000000,0:5000000 2,100:50000000,0:10000000"
    phased_workload="2,100:50000000,0:50000000 2,100:50000000,0:25000000"
    for w in $phased_workload ; do
    for ds in $datastructures_strict ; do
    for n in $threads ; do
        echo $benchmark $ds $n $w >> experiment_list.txt
    done
    done
    done
}

run_desg() {
    echo "Preparing experiment 3: Designated Threads Experiment"
    prepare_exp "desg" >> experiment_list.txt

    benchmark=13
    delmin_threads_per_zone="4 8 48"
    for dz in $delmin_threads_per_zone ; do
    for ds in $datastructures_strict ; do
    for n in $threads_desg ; do
        echo $benchmark $ds $n $dz >> experiment_list.txt
    done
    done
    done
}

run_insert_highest() {
    echo "Preparing experiment: Inserting highest priority element only"
    prepare_exp "highest" >> experiment_list.txt

    benchmark=9
    for ds in $datastructures ; do
    for n in $threads ; do
    for o in $ops_per_thread ; do
        echo $benchmark $ds $n $o >> experiment_list.txt
        # echo $benchmark $ds $n $o $max_key $prefill $heap_list_size $duration $binding_policy >> experiment_list.txt
    done
    done
    done
}

run_desg_us() {
    echo "Preparing experiment 4: Designated Threads Experiment Us Only"
    prepare_exp "desg" >> experiment_list.txt

    #benchmark=6
    benchmark=8
    #delmin_threads_per_zone="2 4 6 8 10 12 14 16 18 20"
    delmin_threads_tot="4 8 12 16 24 32 48" # num desg threads at 96 threads
    for dz in $delmin_threads_tot ; do
    for n in $threads_desg ; do
        echo $benchmark "pipq" $n $dz >> experiment_list.txt
    done
    done
}

run_paths() {
    echo "Preparing experiment 5: Path Followed"
    prepare_exp "paths" >> experiment_list.txt

    benchmark=3
    for u in $ins_paths; do
    for n in $threads_paths ; do
        echo $benchmark "pipq_cntrs" $n $u  >> experiment_list.txt
    done
    done
}

run_latency() {
    echo "Preparing experiment 6: Latency"
    prepare_exp "latency" >> experiment_list.txt

    benchmark=3
    for u in $ins ; do
    for ds in $datastructures_strict ; do
    for n in $threads ; do
        echo $benchmark $ds $n $u  >> experiment_list.txt
    done
    done
    done
}

# run_microbenchmark
# run_insert_highest
# run_phased
run_desg
# run_desg_us
# run_paths
# run_latency

echo "Total experiment lines generated:" $(cat experiment_list.txt | wc -l)