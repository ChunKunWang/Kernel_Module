#!/bin/sh

gcc caller.c -o signal &&\
gcc spinner.c -lcrypt -o spinner &&\
make clean &&\
make &&\
sudo rmmod uwrr.ko &&\
sudo rmmod ../Assignment2/mysync.ko &&\
sudo insmod uwrr.ko &&\
sudo insmod ../Assignment2/mysync.ko 

rm log

