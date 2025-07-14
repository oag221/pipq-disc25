#! /bin/bash

if [[ $# -ne 3 ]]; then
  echo "Incorrect number of arguments (expected=4, actual=$#)."
  echo "Usage: $0 <datadir> <num_trials> <listname>."
  exit
fi

datadir=$1 # sssp/data
ntrials=$2 # 5
listname=$3 # sssp
#graph_name=$4 # livejournal.txt or roadnetCA.txt

cd ${datadir}
outfile=${listname}.csv # sssp.csv
echo "OUTFILE"
echo ${outfile}

currfile=""

avg_duration=0
# totthrupt=0
# insthrupt=0
# delthrupt=0
# ulat=0
# track_thrupt=()

trialcount=0
samplecount=0

echo "graph,ds_name,threads,duration" >${outfile}


files=$(ls ${listname})
for f in ${files}; do
    filename=${listname}/${f}

    # Assumes trials are consecutive.
    rootname=$(echo ${filename} | sed -E 's/step[0-9]+[.]//' | sed -e 's/[.]trial.*//') # filename without "step37821" and "trialx.out" - EX: luigi.skiplistlock.bundle.ts.k1000000.u50.rq0.rqsize50.nrq0.nwork1

    if [[ ! "${rootname}" == "${currfile}" ]]; then
        echo ""
        trialcount=0
        samplecount=0
        currfile=${rootname}

        line=$(head -n 1 ${filename})
        if [[ $line == *"livejournal"* ]]; then
            graph_name="livejournal.txt"
        elif [[ $line == *"roadnetCA"* ]]; then
            graph_name="roadnetCA.txt"
        elif [[ $line == *"orkut"* ]]; then
            graph_name="com-orkut.ungraph.txt"
        fi
        echo "graph_name = ${graph_name}"
        #config=$(cat ${filename} | grep -B 10000 'BEGIN RUNNING')
        threads=`cat $filename | grep "Nb threads" | sed -e 's/.*: //'`
        ds_name=`cat $filename | grep "PQ Type" | sed -e 's/.*: //'`
    fi
    
    trialcount=$((trialcount + 1))

    duration=`cat $filename | grep "duration" | grep '(?<= )[0-9]+\.?[0-9]*' -Po`
    echo "rootname: ${rootname}: ${duration}"
    avg_duration=$(($avg_duration + $duration))

    # filecontents=$(cat ${filename} | grep -A 10000 'END RUNNING')
    # if [[ $(echo "${filecontents}" | grep -c 'end delete ds') != 1 ]]; then
    #     # Skip if not a completed run.
    #     continue
    # fi

    # Latency statistics.
    # ulat=$(($(echo "${filecontents}" | grep 'average latency_updates' | sed -e 's/.*=//') + ${ulat}))

    # # Throughput statistics.
    # insthrupt=$(($(echo "${filecontents}" | grep 'insert throughput' | sed -e 's/.*: //') + ${insthrupt}))
    # delthrupt=$(($(echo "${filecontents}" | grep 'delmin throughput' | sed -e 's/.*: //') + ${delthrupt}))
    # totthrupt=$(($(echo "${filecontents}" | grep 'update throughput' | sed -e 's/.*: //') + ${totthrupt}))
    # tot_thpt_now=$(echo "${filecontents}" | grep 'update throughput' | sed -e 's/.*: //')
    # track_thrupt+=(${tot_thpt_now})

    samplecount=$((${samplecount} + 1))

    # Output previous averages.
    if [[ ${trialcount} == ${ntrials} ]]; then
        if [[ ${samplecount} != ${ntrials} ]]; then
            echo "Warning: unexpected number of samples (${samplecount}). Computing averages anyway: ${rootname}"
        elif [[ ${samplecount} == 0 ]]; then
            echo "Error: No samples collected: ${rootname}"
        else 
        
        avg=$((${avg_duration} / ${samplecount}))
        echo "${avg_duration} / ${samplecount} = ${avg}"

        printf "%s,%s,%d" ${graph_name} ${ds_name} ${threads}  >> ${outfile}
        printf ",%d" $((${avg_duration} / ${samplecount})) >>${outfile}
        printf "\n" >>${outfile}

        # calculate the standard deviation for the similar group of runs
        # sum=0
        # avg=$(bc -l <<< $totthrupt/$samplecount)
        # for value in "${track_thrupt[@]}"
        # do
        #     temp1=$(bc -l <<< $value-$avg)
        #     temp2=$(bc -l <<< $temp1*$temp1)
        #     sum=$(bc -l <<< $temp2+$sum)
        # done
        # variance=$(bc -l <<< $sum/$samplecount)
        # std_dev=$(echo "$variance" | awk '{print sqrt($1)}')
        # std_dev=$(echo ${std_dev} | awk -F"E" 'BEGIN{OFMT="%.2f"} {print $1 * (10 ^ $2)}')

        # printf ",%.2f" $(echo "scale=4; ${std_dev}" | bc) >>${outfile}
        # printf "\n" >>${outfile}

        fi
        # totthrupt=0
        # insthrupt=0
        # delthrupt=0
        # ulat=0
        # track_thrupt=()
        avg_duration=0
    fi
done
