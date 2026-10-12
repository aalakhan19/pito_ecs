#!/bin/sh
# usage: [DURATION=30] profile_all.sh [threads...]
# Profiles every benchmark (variant 0, no work) for pito and pito_atomic into one profiles/<host>_<date>/ folder.
# Threads default to 1 and the benchmarks' largest count: the largest power of two <= hardware threads, max 32.
cd "$(dirname "$0")"
export PROFILE_RUN_ID=${PROFILE_RUN_ID:-$(date +%Y-%m-%d_%H-%M-%S)}

threads=$*
if [ -z "$threads" ]; then
    max=1
    while [ $((max * 2)) -le "$(getconf _NPROCESSORS_ONLN)" ] && [ $max -lt 32 ]; do max=$((max * 2)); done
    threads="1 $max"
fi

for bench in update insert delete initialize local shared mixed; do
    for lib in pito pito_atomic; do
        for t in $threads; do
            echo "$bench $lib $t threads"
            sh profile.sh $bench $lib 0 $t 0 > /dev/null || exit 1
        done
    done
done
