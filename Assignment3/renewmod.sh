#!/bin/sh

make clean &&\
make &&\
sudo rmmod GPU_Lock.ko &&\
sudo insmod GPU_Lock.ko 

