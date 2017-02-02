#!/bin/sh

gcc -fopenmp caller.c -lcrypt &&\
gcc -o command command.c &&\

make clean &&\
make &&\
sudo rmmod amos_perf.ko &&\
sudo insmod amos_perf.ko 

