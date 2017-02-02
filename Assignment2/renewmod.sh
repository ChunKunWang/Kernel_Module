#!/bin/sh

make clean &&\
make &&\
sudo rmmod mysync.ko &&\
sudo insmod mysync.ko 

