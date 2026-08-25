#!/bin/sh
set -e

cd "$(dirname "$0")"

clang -std=c11 -O2 -g -fno-omit-frame-pointer -Wall -Wextra -pthread -o owned_delete_fg owned_delete_fg.c -lm

sample owned_delete_fg 12 -wait -file sample.txt &
SAMPLE_PID=$!

./owned_delete_fg &

wait "$SAMPLE_PID"

awk -f "$(brew --prefix flamegraph)/bin/stackcollapse-sample.awk" sample.txt > collapsed.txt
flamegraph.pl --title "owned_delete (entity_lock), no work, 4 threads" collapsed.txt > flamegraph.svg

echo "wrote flamegraph.svg"
