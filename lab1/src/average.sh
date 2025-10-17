#!/bin/bash

count=$#
sum=0

for num in "$@"; do
    sum=$((sum+num))
done

average=$((sum/count))

echo "Число аргументов: $count"
echo "Среднее значение: $average"