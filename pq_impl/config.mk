## Note: as a convenience hack, this file is included in Makefiles,
##       AND in bash scripts, to set some variables.
##       So, don't write anything that isn't valid in both make AND bash.

## Set the desired maximum thread count (maxthreads),
## an upper bound on the maximum thread count that is a power of 2 (maxthreads_powerof2),
## the maximum range query thread count (maxrqthreads) for use in experiments,
## the number of threads to increment by in the graphs produced by experiments (threadincrement),
## and the CPU frequency in GHz (cpu_freq_ghz) used for timing measurements with RDTSC.

# maxthreads=48
# maxthreads_powerof2=64
# maxrqthreads=6
# threadincrement=8
# cpu_freq_ghz=2.1

# ## Configure the thread pinning/binding policy (see README.txt)
# ## Blank means no thread pinning. (Threads can run wherever they want.)
# pinning_policy=""

## The policy commented out below is what we used on our 48 thread 2-socket
## Intel machine (where we pinned threads to alternating sockets).
#pinning_policy="-bind 0,24,12,36,1,25,13,37,2,26,14,38,3,27,15,39,4,28,16,40,5,29,17,41,6,30,18,42,7,31,19,43,8,32,20,44,9,33,21,45,10,34,22,46,11,35,23,47"
maxthreads=96
maxthreads_powerof2=256
threadincrement=12
cpu_freq_ghz=2.1
pinning_policy="-bind 0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,65,69,73,77,81,85,89,93,97,101,105,109,113,117,121,125,129,133,137,141,145,149,153,157,161,165,169,173,177,181,185,189,2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,66,70,74,78,82,86,90,94,98,102,106,110,114,118,122,126,130,134,138,142,146,150,154,158,162,166,170,174,178,182,186,190,3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63,67,71,75,79,83,87,91,95,99,103,107,111,115,119,123,127,131,135,139,143,147,151,155,159,163,167,171,175,179,183,187,191"
numa_0="0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188"
numa_1="1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,65,69,73,77,81,85,89,93,97,101,105,109,113,117,121,125,129,133,137,141,145,149,153,157,161,165,169,173,177,181,185,189"
numa_2="2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,66,70,74,78,82,86,90,94,98,102,106,110,114,118,122,126,130,134,138,142,146,150,154,158,162,166,170,174,178,182,186,190"
numa_3="3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63,67,71,75,79,83,87,91,95,99,103,107,111,115,119,123,127,131,135,139,143,147,151,155,159,163,167,171,175,179,183,187,191"