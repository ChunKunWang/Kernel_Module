#!/bin/sh
rm output

counter=0
input=$1
#value=$(awk -F" " '{ print $2}' $input > temp)
file="./output"
while IFS= read -r line
do
  if [ "$counter" -eq 0 ]; then
    counter=1
  else
    echo "$line" >> output
    counter=0
  fi
done < "$1"

awk -F" " '{print $1 " " $2 " " $3 " " $4 " " $5 " " $8 " " $9 " " $10 " " $11 " " $12 " " $13 " " $14 " " $15 " " $16 " " $17 " " $18}' ./output >> output_2



