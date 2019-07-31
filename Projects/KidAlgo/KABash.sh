#!/bin/bash

currentdateDir=`date +"%Y-%m-%d_%H-%M-%S"`
output_dir="output_data"
mkdir "$output_dir"
output_date_dir="${output_dir}/${currentdateDir}"
echo output_date_dir
mkdir "$output_date_dir"

data=("R/,/G\;R/,/G,R/,/G\;R/,/G,R/,/G,R/,/G" "/RB,GB/\;/RB,GB/,/RB,GB/\;/RB,GB/,/RB,GB/,/RB,GB/" "G/,/RR\;G/,/RR,G/,/RR\;G/,/RR,G/,/RR,G/,/RR")
name=("sort" "hitch" "double")

param=(2.0 1.0 0.9 0.8 0.7 0.6 0.5 0.4 0.3)

cnt=0

for onedata in "${data[@]}"
do
    for oneparam in "${param[@]}"
    do
        currentdate=`date +"%Y-%m-%d_%H-%M-%S"`
        output="$output_date_dir/${name[$cnt]}_param${oneparam}_${currentdate}.txt"
        touch "$output"
        command="./main --alphabet=BRG --data=$onedata --param=$oneparam --threads=12 --top=10000 --time=20m"
        echo -e $command >> $output
       ./main --alphabet=BRG --data=$onedata --param=$oneparam --threads=12 --top=10000 --time=20m >> $output

    done
    cnt=$((cnt+1))
done