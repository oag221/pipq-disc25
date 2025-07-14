# PIPQ Priority Queue Repo #

### Description ###

* This repo contains code for PIPQ: a strict and linearizable concurrent priority queue

### Important Files ###

##### STRICT PIPQ (As described in the paper) #####
* `pq_impl/pipq-strict`: directory contains the strict, linearizable implementation of PIPQ as described in the paper.
  * `pipq_strict.h`: definitions for the priority queue (class, structs), method definitions, simple helper functions, etc.
  * `pipq_strict_impl.h`: the implementation code - contains initial allocations/setup of PIPQ, PIPQ Insert and Del-min APIs + supporting code, as well as the worker-level insert/del-min implementations
* `pq_impl/harris_ll/harris.h`: header file for leader level implementations
* `pq_impl/microbench/harris.cc`: leader level implementations for strict PIPQ

##### Test Files #####
* `pq_impl/microbench/run_single.sh`: script to run a single microbenchmark, phased, or designated thread experiment for a specified data structure - configurable parameters in file (comment/uncomment things to run various data structures or workloads)
  * Run as so: `./run_single.sh <executable to run> <threads>` (second parameter is optional, defaults to 96); executable options: "pipq", "linden", "lotan"
    * Example: `./run_single.sh pipq 96`
* `pq_impl/microbench/experiment_list_generate.sh`: generates a text file containing one line per experiment to run; 
  * NOTE: uncomment / comment out lines 90-93 based on which experiment to run (microbench, phased, designated experiments)
  * [Line 90] `run_microbenchmark`: to run the microbenchmark experiments (Figure 3 in paper)
  * [Line 91] `run_phased`: to run the phased experiments (Figure 6 in paper)
  * [Line 92] `run_desg`: to run the designated thread experiment, including competitors (Figure 5 in paper)
  * [Line 93] `run_desg_us`: to run the designated thread experiment for PIPQ specifically (Figure 4 in paper)
* `pq_impl/microbench/run_script.sh`: calls `experiment_list_generate.sh`, and then reads from the generated text file to run the specified experiments (for all experiments except SSSP). Results for each trial are written to directories within `pq_impl/microbench/data`
* `pq_impl/sssp/run_sssp.sh`: use to run all sssp experiments; trials are written to `pq_impl/sssp/data`.
* `pq_impl/plot.py`: File used to generate all plots as seen in the paper. It first calls a function which generates a csv file from the trials, and then creates the plots. Plots are written to `pq_impl/figures`
  * run with command: `python3 plot.py --save_plots --microbench --exp_type micro`
  * `--exp_type` options are: "micro", "desg", "phased", "desg_usonly", "sssp"
