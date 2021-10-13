#!/usr/bin/env bash

make

echo "size, d_r, d_w, p_r, p_w, s_r, s_w"
echo "size, d_r, d_w, p_r, p_w, s_r, s_w" >> log.csv
for((i = 8; i <= 20000000; i = i * 2 )); do
    echo $i
    echo -n $i "," >> log.csv
    ./latency $i >> log.csv
done