#!/bin/bash
#
# Run script for the micro benchmark experiments.
#

source ../config.mk

trials=3
duration=5000

# cols="%6s %12s %12s %12s %8s %6s %6s %8s %6s %6s %8s %12s %12s %12s %12s"
# headers="step machine ds alg k u rq rqsize nrq nwork trial throughput rqs updates finds"
machine=`hostname`
echo "machine = $machine"

# note: one of "u", "phased", OR "desg" populated each time; -1 when not applicable for a run
cols="%6s %12s %20s %6s %6s %12s %6s %26s %6s %6s %6s %12s %12s %12s %12s %12s %12s %12s %12s"
headers="step machine ds bench offset k u~ phased~ desg~ nwork trial throughput thpt_ins thpt_del lat_ins lat_del slowest slow fast"

echo "Generating 'experiment_list.txt' according to settings in '/config.mk'..."
./experiment_list_generate.sh

skip_steps_before=0
skip_steps_after=1000000

outdir=data
fsummary=$outdir/summary.txt

rm -r -f $outdir.old
if [ -d "${outdir}" ]; then
    mv -f $outdir $outdir.old
fi
mkdir $outdir

rm -f warnings.txt

# calculate expected running time
cnt2=`cat experiment_list.txt | wc -l`
cnt2=`expr $cnt2 \* $trials`
echo "Performing $cnt2 trials..."

secs=`expr $duration / 1000`
estimated_secs=`expr $cnt2 \* $secs`
estimated_hours=`expr $estimated_secs / 3600`
estimated_mins=`expr $estimated_secs / 60`
estimated_mins=`expr $estimated_mins % 60`
echo "Estimated running time: ${estimated_hours}h${estimated_mins}m" > $fsummary

#cnt1=10000
cnt1=20000
cnt2=`expr $cnt2 + 10000`

printf "${cols}\n" ${headers} >> $fsummary
cat $fsummary

counter_threshold=10
counter_max=100
heap_list_size=5000000
max_key=100000000
max_offset=32
prefill=1000000
ops_per_thread=1000000
binding_policy="0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,65,69,73,77,81,85,89,93,2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,66,70,74,78,82,86,90,94,3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63,67,71,75,79,83,87,91,95,96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,97,101,105,109,113,117,121,125,129,133,137,141,145,149,153,157,161,165,169,173,177,181,185,189,98,102,106,110,114,118,122,126,130,134,138,142,146,150,154,158,162,166,170,174,178,182,186,190,99,103,107,111,115,119,123,127,131,135,139,143,147,151,155,159,163,167,171,175,179,183,187,191"
    
currdir=""
exp_type=""

#while read u rq rqsize k nrq nwork ds alg ; do
while read bench ds nwork var; do
    if [ ${var} == "prepare" ]; then
        currdir=${outdir}/${nwork} # nwork stores the experiment type
        exp_type=${nwork}
        continue
    fi

    # Create a directory to organize all runs for a specific DS
    if [[ ! -d "${currdir}" ]]; then
        mkdir -p "${currdir}"
    fi

    for ((trial=0;trial<$trials;++trial)); do
        cnt1=`expr $cnt1 + 1`
        if ((cnt1 < skip_steps_before)); then continue; fi
        if ((cnt1 > skip_steps_after)); then continue; fi

        ins=-1
        phased=-1
        desg=-1

        if [ $bench -eq 3 ]; then
            # microbench - var is ins (-i)
            if [ $exp_type == "paths" ]; then
                fname="${currdir}/step$cnt1.$machine.${ds}.PATHS.bench$bench.k$max_key.nwork$nwork.ins$ins.maxoffset$max_offset.trial$trial.out"
            else
                fname="${currdir}/step$cnt1.$machine.${ds}.bench$bench.k$max_key.nwork$nwork.ins$ins.maxoffset$max_offset.trial$trial.out"
            fi
            ins=$var
            if [ $ins -eq 0 ]; then
                bench_del=4
                del=100
                pf=6000000
                num_ops=3000000
                cmd="./${machine}.${ds}.out -k $max_key -m $max_offset -b $bench_del -p $pf -o $num_ops -n $nwork -i $ins -d $del -ct $counter_threshold -cm $counter_max -h $heap_list_size -bind $binding_policy"
            else
                del=$((100-ins))
                cmd="./${machine}.${ds}.out -k $max_key -m $max_offset -b $bench -p $prefill -t $duration -n $nwork -i $ins -d $del -ct $counter_threshold -cm $counter_max -h $heap_list_size -bind $binding_policy"
            fi
        elif [ $bench -eq 5 ]; then
            # phased - var is phased_workload string (-phased)
            phased=$var
            fname="${currdir}/step$cnt1.$machine.${ds}.bench$bench.k$max_key.nwork$nwork.phased$phased.maxoffset$max_offset.trial$trial.out"
            cmd="./${machine}.${ds}.out -k $max_key -m $max_offset -b $bench -phased $phased -p $prefill -t $duration -n $nwork -ct $counter_threshold -cm $counter_max -h $heap_list_size -bind $binding_policy"
        
        elif [ $bench -eq 6 ] || [ $bench -eq 8 ] || [ $bench -eq 13 ]; then
            # designated threads - var is del min threads per zone (-dz)
            desg=$var
            fname="${currdir}/step$cnt1.$machine.${ds}.bench$bench.k$max_key.nwork$nwork.desg$desg.maxoffset$max_offset.trial$trial.out"
            cmd="./${machine}.${ds}.out -k $max_key -m $max_offset -b $bench -dz $desg -p $prefill -t $duration -n $nwork -ct $counter_threshold -cm $counter_max -h $heap_list_size -bind $binding_policy"

        elif [ $bench -eq 9 ]; then
            # insert highest priority only - var is num ops 
            o=$var
            fname="${currdir}/step$cnt1.$machine.${ds}.bench$bench.k$max_key.maxoffset$max_offset.trial$trial.out"
            cmd="./${machine}.${ds}.out -k $max_key -m $max_offset -b $bench -p $prefill -o $ops_per_thread -h $heap_list_size -t $duration -n $nwork -bind $binding_policy"
        fi

        # run the command
        echo "$cmd" > $fname
        $cmd >> $fname

        if [ $exp_type == "paths" ]; then
            printf "${cols}" $cnt1 $machine $ds "paths" $max_offset $max_key $ins $phased $desg $nwork $trial "`cat $fname | grep 'update throughput' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'insert throughput' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'delmin throughput' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'latency ins/op' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'latency del/op' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'slowEST-path' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'slow-path' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'fast-path' | cut -d':' -f2 | tr -d ' '`" >> $fsummary
        else
            printf "${cols}" $cnt1 $machine $ds $bench $max_offset $max_key $ins $phased $desg $nwork $trial "`cat $fname | grep 'update throughput' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'insert throughput' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'delmin throughput' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'latency ins/op' | cut -d':' -f2 | tr -d ' '`" "`cat $fname | grep 'latency del/op' | cut -d':' -f2 | tr -d ' '`" -1 -1 -1 >> $fsummary
        fi
        tail -1 $fsummary
        echo
        printf "%120s          %s\n" "$fname" "`head -1 $fname`" >> $fsummary

        throughput=`cat $fname | grep "update throughput" | cut -d":" -f2 | tr -d " "`
        if [ "$throughput" == "" ] || [ "$throughput" -le "0" ] ; then echo "WARNING: thoughput $throughput in file $fname" >> warnings.txt ; cat warnings.txt | tail -1 ; fi
    done
done < experiment_list.txt

if [ "`cat warnings.txt | wc -l`" -ne 0 ]; then
    echo "NOTE: THERE WERE WARNINGS. PRINTING THEM..."
    cat warnings.txt
fi
