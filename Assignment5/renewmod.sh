#!/bin/sh

gcc caller.c &&\
gcc mapRand.c -o mapRand &&\
gcc mapSeq.c -o mapSeq &&\

make clean &&\
make &&\
sudo rmmod pflog.ko &&\
sudo insmod pflog.ko 

