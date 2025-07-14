#! /bin/bash

if [[ $# -ne 3 ]]; then
  echo "Incorrect number of arguments (expected=4, actual=$#)."
  echo "Usage: $0 <datadir> <num_trials> <listname>."
  exit
fi

datadir=$1 # microbenchmark/data
ntrials=$2 # 5
listname=$3 # micro

# Each algorithm should be isolated in its own directory.
# algos=$(ls ${datadir})
# echo "ALGOS"
# echo $algos

cd ${datadir}
outfile=${listname}.csv
echo "OUTFILE"
echo ${outfile}

currfile=""

totthrupt=0
insthrupt=0
delthrupt=0
ulat=0
del_lat=0
ins_lat=0
fast=0
slower=0
slowest=0
track_thrupt=()

trialcount=0
samplecount=0

if [ $listname == "micro" ]; then
    echo "list,max_key,threads,ins_rate,tot_thruput,ins_thruput,del_thruput,ins_lat,del_lat,std_dev" >${outfile}
elif [ $listname == "desg" ]; then
    echo "list,max_key,threads,desg_tds,tot_thruput,ins_thruput,del_thruput,ins_lat,del_lat,std_dev" >${outfile}
elif [ $listname == "phased" ]; then
    echo "list,max_key,threads,phase_str,tot_thruput,ins_thruput,del_thruput,ins_lat,del_lat,std_dev" >${outfile}
elif [ $listname == "latency" ]; then
    echo "list,max_key,threads,ins_rate,tot_thruput,ins_thruput,del_thruput,ins_lat,del_lat,std_dev" >${outfile}
elif [ $listname == "paths" ]; then
    echo "list,max_key,threads,ins_rate,tot_thruput,ins_thruput,del_thruput,ins_lat,del_lat,fast,slower,slowest,std_dev" >${outfile}
fi

files=$(ls ${listname})
for f in ${files}; do
    filename=${listname}/${f}
    # Only parse lines for given listname.
    # if [[ "${listname}" != "" ]] && [[ "$(echo ${filename} | grep ${listname})" == "" ]]; then
    #     exit
    # elif [[ "$(echo ${filename} | sed 's/.*[.]csv/.csv/')" == ".csv" ]]; then
    #     # Skip any generated .csv files.
    #     continue
    # fi

    # "Us-Lin", "Us-Rel", "Linden", "Spray", "SMQ", "Lotan"

    # Assumes trials are consecutive.
    rootname=$(echo ${filename} | sed -E 's/step[0-9]+[.]//' | sed -e 's/[.]trial.*//') # filename without "step37821" and "trialx.out" - EX: luigi.skiplistlock.bundle.ts.k1000000.u50.rq0.rqsize50.nrq0.nwork1

    if [[ ! "${rootname}" == "${currfile}" ]]; then
        echo ""
        trialcount=0
        samplecount=0
        currfile=${rootname}

        config=$(cat ${filename} | grep -B 10000 'BEGIN RUNNING')

        list=$(echo "${config}" | grep '^DS='| sed -e 's/.*=//')
        #list+="-${listname}"
        maxkey=$(echo "${config}" | grep 'MAXKEY=' | sed -e 's/.*=//')
        threads=$(echo "${config}" | grep 'THREADS='| sed -e 's/.*=//')
        rrate=$(echo "${config}" | grep 'DEL='| sed -e 's/.*=//')

        if [ $listname == "micro" ]; then
            irate=$(echo "${config}" | grep 'INS='| sed -e 's/.*=//')
        elif [ $listname == "desg" ]; then
            #del_min_per_zone=$(echo "${config}" | grep 'DELMIN_THREADS_PER_ZONE: '| sed -e 's/.*: //') # TODO: check that this works
            
            del_min_per_zone=$(head -1 ${filename} | awk 'BEGIN { RS=" -" ; FS=" " } $1 ~ /dz/ {print $2}')
            echo "del_min_per_zone: ${del_min_per_zone}"
        elif [ $listname == "phased" ]; then
            phased_str=$(head -1 ${filename} | awk 'BEGIN { RS=" -" ; FS=" " } $1 ~ /phased/ {print $2}')
            echo "Phased string is ${phased_str}"
        elif [ $listname == "latency" ]; then
            irate=$(echo "${config}" | grep 'INS='| sed -e 's/.*=//')
            echo "INS = ${irate}"
        elif [ $listname == "paths" ]; then
            irate=$(echo "${config}" | grep 'INS='| sed -e 's/.*=//')
        fi
    fi

    trialcount=$((trialcount + 1))
    filecontents=$(cat ${filename} | grep -A 10000 'END RUNNING')
    if [[ $(echo "${filecontents}" | grep -c 'end delete ds') != 1 ]]; then
        # Skip if not a completed run.
        continue
    fi

    # Latency statistics.
    ulat=$(($(echo "${filecontents}" | grep 'average latency_updates' | sed -e 's/.*=//') + ${ulat}))
    # ins_lat=$(($(echo "${filecontents}" | grep 'latency ins/op' | sed -e 's/.*: //') + ${ins_lat}))
    # del_lat=$(($(echo "${filecontents}" | grep 'latency del/op' | sed -e 's/.*: //') + ${del_lat}))

    ins_lat_tot=$(($(echo "${filecontents}" | grep 'total latency ins' | sed -e 's/.*: //')))
    num_ins_ops=$(($(echo "${filecontents}" | grep 'total insertions' | sed -e 's/.*: //')))
    del_lat_tot=$(($(echo "${filecontents}" | grep 'total latency del' | sed -e 's/.*: //')))
    num_del_ops=$(($(echo "${filecontents}" | grep 'total delmin' | sed -e 's/.*: //')))
    if [ $num_ins_ops -gt 0 ]; then
        ins_lat_temp=$(echo "scale=7; ${ins_lat_tot} / ${num_ins_ops}" | bc -l)
        ins_lat=$(echo "scale=7; ${ins_lat_temp} + ${ins_lat}" | bc -l)
    else
        ins_lat_temp=0
    fi
    if [ $num_del_ops -gt 0 ]; then
        del_lat_temp=$(echo "scale=7; ${del_lat_tot} / ${num_del_ops}" | bc -l)
        del_lat=$(echo "scale=7; ${del_lat_temp} + ${del_lat}" | bc -l)
    else
        del_lat_temp=0
    fi


    # Throughput statistics.
    insthrupt=$(($(echo "${filecontents}" | grep 'insert throughput' | sed -e 's/.*: //') + ${insthrupt}))
    delthrupt=$(($(echo "${filecontents}" | grep 'delmin throughput' | sed -e 's/.*: //') + ${delthrupt}))
    thpt_tot_cur=`cat $filename | grep 'update throughput' | sed -e 's/.*: //'`
    #thpt_tot_cur=$((echo "${filecontents}" | grep 'update throughput' | sed -e 's/.*: //'))
    totthrupt=$(($(echo "${filecontents}" | grep 'update throughput' | sed -e 's/.*: //') + ${totthrupt}))
    tot_thpt_now=$(echo "${filecontents}" | grep 'update throughput' | sed -e 's/.*: //')
    track_thrupt+=(${tot_thpt_now})

    if [ $listname == "paths" ]; then
        varfast=$(echo "${filecontents}" | grep 'fast-path' | sed -e 's/.*: //')
        varslower=$(echo "${filecontents}" | grep 'slow-path' | sed -e 's/.*: //')
        varslowest=$(echo "${filecontents}" | grep 'slowEST-path' | sed -e 's/.*: //')
        fast=$(echo "${varfast} + ${fast}" | bc)
        slower=$(echo "${varslower} + ${slower}" | bc)
        slowest=$(echo "${varslowest} + ${slowest}" | bc)
    fi

    echo "rootname: ${rootname}: ${ins_lat_temp}, ${del_lat_temp}"

    samplecount=$((${samplecount} + 1))

    # Output previous averages.
    if [[ ${trialcount} == ${ntrials} ]]; then
        if [[ ${samplecount} != ${ntrials} ]]; then
        echo "Warning: unexpected number of samples (${samplecount}). Computing averages anyway: ${rootname}"
        fi

        if [[ ${samplecount} == 0 ]]; then
        echo "Error: No samples collected: ${rootname}"
        else 
        
        if [ $listname == "micro" ]; then
            printf "%s,%d,%d,%d" ${list} ${maxkey} ${threads} ${irate}  >> ${outfile}
        elif [ $listname == "desg" ]; then
            printf "%s,%d,%d,%d" ${list} ${maxkey} ${threads} ${del_min_per_zone}  >> ${outfile}
        elif [ $listname == "phased" ]; then
            printf "%s,%d,%d,\"%s\"" ${list} ${maxkey} ${threads} ${phased_str}  >> ${outfile}
        elif [ $listname == "latency" ]; then
            printf "%s,%d,%d,%d" ${list} ${maxkey} ${threads} ${irate}  >> ${outfile}
        elif [ $listname == "paths" ]; then
            printf "%s,%d,%d,%d" ${list} ${maxkey} ${threads} ${irate}  >> ${outfile}
        fi
        
        #printf ",%d" $((${ulat} / ${samplecount})) >>${outfile}
        printf ",%d" $((${totthrupt} / ${samplecount})) >>${outfile}
        printf ",%d" $((${insthrupt} / ${samplecount})) >>${outfile}
        printf ",%d" $((${delthrupt} / ${samplecount})) >>${outfile}
        if [ $irate -eq 0 ]; then
            printf ",%d" 0 >> ${outfile}
        else
            printf ",%.5f" $(echo "scale=5; ${ins_lat} / ${samplecount}" | bc -l) >>${outfile}
        fi

        if [ $irate -eq 100 ]; then
            printf ",%d" 0 >> ${outfile}
        else
            printf ",%.5f" $(echo "scale=5; ${del_lat} / ${samplecount}" | bc -l) >>${outfile}
        fi

        printf "INS LATENCY AVG: %.5f\n" $( echo "${ins_lat} / ${samplecount}" | bc -l)
        printf "DEL LATENCY AVG: %.5f\n" $(echo "${del_lat} / ${samplecount}" | bc -l)

        if [ $listname == "paths" ]; then
            fasttt=$(echo "scale=2; ${fast} / ${samplecount}" | bc -l)
            printf ",%f" $(echo "scale=2; ${fast} / ${samplecount}" | bc -l) >>${outfile}
            printf ",%f" $(echo "scale=2; ${slower} / ${samplecount}" | bc -l) >>${outfile}
            printf ",%f" $(echo "scale=2; ${slowest} / ${samplecount}" | bc -l) >>${outfile}

            echo "fast: ${fasttt}"

        fi

        # calculate the standard deviation for the similar group of runs
        sum=0
        avg=$(bc -l <<< $totthrupt/$samplecount)
        #echo "${totthrupt} / ${samplecount} = ${avg}"
        for value in "${track_thrupt[@]}"
        do
            temp1=$(bc -l <<< $value-$avg)
            temp2=$(bc -l <<< $temp1*$temp1)
            sum=$(bc -l <<< $temp2+$sum)
        done
        variance=$(bc -l <<< $sum/$samplecount)
        std_dev=$(echo "$variance" | awk '{print sqrt($1)}')
        std_dev=$(echo ${std_dev} | awk -F"E" 'BEGIN{OFMT="%.2f"} {print $1 * (10 ^ $2)}')

        printf ",%.2f" $(echo "scale=4; ${std_dev}" | bc) >>${outfile}
        printf "\n" >>${outfile}

        fi

        totthrupt=0
        insthrupt=0
        delthrupt=0
        ulat=0
        track_thrupt=()
        ins_lat=0
        del_lat=0
        fast=0
        slower=0
        slowest=0
    fi
done
