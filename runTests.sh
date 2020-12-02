#!/bin/bash
inputDir=$1
outputDir=$2
maxThreads=$3


if (($maxThreads < 1))
then
    echo "Error: invalid thread number"
    exit 1
fi

if [[ ! -d $inputDir ]]
then
    echo "Error: invalid input directory"
    exit 1
fi

if [[ ! -d $outputDir ]]
then
    echo "Error: invalid output directory"
    exit 1
fi

for input in "$inputDir"*
do 
    for ((threads = 1; threads <= maxThreads; threads++))
    do
        echo "InputFile=$input NumThreads=$threads"
        ./tecnicofs $input $outputdir/$input-$threads.txt $threads
    done
done