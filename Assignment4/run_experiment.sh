#!/bin/bash

echo -e "Running experiments..."

sleep 1
sudo chrt -r 1 ./spinner uwrr 2 >> log &
sudo chrt -r 1 ./spinner uwrr 4 >> log &
sudo chrt -r 1 ./spinner uwrr 6 >> log &
sudo chrt -r 1 ./spinner uwrr 8 >> log &
sudo chrt -r 1 ./spinner uwrr 10 >> log &
sudo chrt -r 1 ./spinner uwrr 12 >> log &
sudo chrt -r 1 ./spinner uwrr 14 >> log &
sudo chrt -r 1 ./spinner uwrr 16 >> log &

sleep 1
echo -e "Release all spinners"
./signal event_signal 99

wait %1 %2 %3 %4 %5 %6 %7 %8
echo "Result logged"

