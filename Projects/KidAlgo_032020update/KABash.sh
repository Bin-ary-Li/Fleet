#!/bin/bash

currentdateDir=`date +"%Y-%m-%d_%H-%M-%S"`
output_dir="output_data"
mkdir "$output_dir"
output_date_dir="${output_dir}/${currentdateDir}"
echo output_date_dir
mkdir "$output_date_dir"

data=("R/,/G\;R/,/G,R/,/G\;R/,/G,R/,/G,R/,/G" "/RB,GB/\;/RB,GB/,/RB,GB/\;/RB,GB/,/RB,GB/,/RB,GB/" "G/,/RR\;G/,/RR,G/,/RR\;G/,/RR,G/,/RR,G/,/RR")
name=("sort" "hitch" "double")

param=(8.0 6.0 4.0 0.97 0.95 0.92 0.87 0.85 0.82 0.77 0.75 0.72)

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