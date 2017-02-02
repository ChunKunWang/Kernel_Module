#!/bin/bash
echo "Launching 4 processes running 30 seconds"
echo "Simulation..." >>log
for i in `seq 1 4`;
do
    chrt -f $i ./GPU_Sim  >>log &
done
wait %1 %2 %3 %4
echo "Result logged -- run me again"
