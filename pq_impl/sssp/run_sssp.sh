#!/bin/bash
# USAGE: ./run.sh var
#  var: print statistics for indicated variable (default #txs)
#       need complete enough name for grep to find unambiguously (e.g. '#txs' works, 'txs' doesn't)


#obj64/sssp -i inputs/grid1000.txt -o out.txt -t 96 -D numa-pq -m 2000000
#obj64/sssp -i inputs/grid1000.txt -o out.txt -D numa-pq-multi -m 2000000 -t 12
#obj64/sssp -i inputs/grid1000.txt -o out.txt -t 96 -D linden -m 2000000
#obj64/sssp -i inputs/livejournal.txt -o out.txt -t 96 -D numa-pq -m 2000000
#obj64/sssp -i inputs/livejournal.txt -o out.txt -t 96 -D lotan -m 2000000
#obj64/sssp -i inputs/livejournal.txt -o out.txt -D numa-pq-multi -m 2000000 -t 12
#obj64/sssp r -i inputs/livejournal.txt -o out.txt -D smq -m 2000000 -t 1
#obj64/sssp r -i inputs/roadnetCA.txt -o out.txt -D numa_pq_lin -m 2000000 -t 1

#obj64/sssp -i inputs/roadnetCA.txt -o out.txt -t 96 -D numa_pq_lin -x 20 -z 35
#obj64/sssp -i inputs/roadnetCA.txt -o out.txt -t 96 -D linden -w 100

#obj64/sssp -i inputs/livejournal.txt -o out.txt -t 96 -D numa_pq_lin -x 20 -z 35
#obj64/sssp -i inputs/livejournal.txt -o out.txt -t 96 -D linden
#obj64/sssp -i inputs/livejournal.txt -o out.txt -t 96 -D numa_pq_rel

#obj64/sssp -i inputs/com-orkut.ungraph.txt -o out.txt -t 12 -D linden
#obj64/sssp -i inputs/com-orkut.ungraph.txt -o out.txt -t 96 -D numa_pq_lin -x 10 -z 100


#obj64/sssp -i inputs/roadnetCA.txt -o out.txt -t 12 -D numa_pq_lin -x 10 -z 100
#obj64/sssp -i inputs/livejournal.txt -o out.txt -t 12 -D numa_pq_lin -x 20 -z 35
#obj64/sssp -i inputs/roadnetCA.txt -o out.txt -t 96 -D numa_pq_rel -x 20 -z 35

#obj64/sssp -i inputs/facebook_combined.txt -o out.txt -t 96 -D numa_pq_lin -x 20 -z 35
#obj64/sssp -i inputs/facebook_combined.txt -o out.txt -t 96 -D linden

#facebook_combined.txt

# obj64/sssp -i inputs/twitter_rv.txt -o out.txt -t 96 -D linden -x 20 -z 35


# if [ -z $1 ]
# then
# input="input/soc-LiveJournal1.txt"
# else
# input=$1
# fi
# echo $1

machine=`hostname`
echo "machine = $machine"

#bin=bin/sssp
bin=obj64/sssp
var=duration

trials=3
#cnt1=10000
cnt1=30000

# cnt2=$((2 * 4 * 9 * $trials)) # 2 graphs * 4 ds's * 2 weights * 9 thread variants * 5 trials each
# echo "Performing $cnt2 trials..."

graphs="livejournal.txt roadnetCA.txt"
#graphs="com-orkut.ungraph.txt"
datastructures_strict="numa_pq_lin linden lotan_shavit"
#datastructures="numa_pq_rel smq numa_pq_lin spraylist"
#datastructures_all="numa_pq_lin linden lotan_shavit spraylist numa_pq_rel smq"
#data_structures="numa_pq_rel"
threads="1 12 24 36 48 60 72 84 96"



cols="%6s %12s %15s %20s %6s %6s %8s"
headers="step machine ds graph threads trial duration"

outdir=data
fsummary=$outdir/summary.txt

rm -r -f $outdir.old
if [ -d "${outdir}" ]; then
    mv -f $outdir $outdir.old
fi
mkdir $outdir
rm -f warnings.txt

printf "${cols}\n" ${headers} >> $fsummary
cat $fsummary

currdir=${outdir}/sssp
if [[ ! -d "${currdir}" ]]; then
    mkdir -p "${currdir}"
fi

weight=0 # 0 = no weights, else the value is the weight range
if [ $weight -eq 0 ]; then
  echo "Weight is 0"
fi

for g in $graphs ; do
for ds in $datastructures_strict ; do
for n in $threads ; do
  for ((trial=0;trial<$trials;++trial)); do
    cnt1=`expr $cnt1 + 1`

    if [ $weight -eq 0 ]; then
      fname="${currdir}/step$cnt1.$machine.${ds}.g$g.threads$n.trial$trial.out"
      cmd="$bin -i inputs/$g -t $n -D $ds"
    else
      fname="${currdir}/step$cnt1.$machine.${ds}.g$g.weight$weight.threads$n.trial$trial.out"
      cmd="$bin -i inputs/$g -t $n -D $ds -w $weight"
    fi

    # run command
    echo "$cmd" > $fname
    $cmd >> $fname

    duration=`cat $fname | grep $var | grep '(?<= )[0-9]+\.?[0-9]*' -Po`
    if [ "$duration" == "" ] || [ "$duration" -le "0" ] ; then echo "WARNING: duration $duration in file $fname" >> warnings.txt ; cat warnings.txt | tail -1 ; fi

    printf "${cols}" $cnt1 $machine $ds $g $n $trial $duration >> $fsummary
    tail -1 $fsummary
    echo
    printf "%120s          %s\n" "$fname" "`head -1 $fname`" >> $fsummary
  done
done
done
done


