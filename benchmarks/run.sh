#!/bin/sh
# usage: run.sh [--types update insert delete initialize local] [--libs pico flecs pito pito_atomic]
# Run from the build dir
# Default is to run all
export BENCH_RUN_ID=${BENCH_RUN_ID:-$(date +%Y-%m-%d_%H-%M-%S)}

types=
libs=
list=

for arg; do
    case $arg in
        --types) list=types ;;
        --libs) list=libs ;;
        update | insert | delete | initialize | local) [ "$list" = types ] && types="$types $arg" || list=bad ;;
        pico | flecs | pito | pito_atomic) [ "$list" = libs ] && libs="$libs $arg" || list=bad ;;
        *) list=bad ;;
    esac

    if [ "$list" = bad ]; then
        echo "usage: $0 [--types update insert delete initialize local] [--libs pico flecs pito pito_atomic]" >&2
        exit 1
    fi
done

selected() {
    [ -z "$1" ] || case " $1 " in *" $2 "*) true ;; *) false ;; esac
}

for bench in update insert delete initialize local; do
    selected "$types" $bench || continue

    for lib in pico flecs pito pito_atomic; do
        selected "$libs" $lib || continue
        ./bench_${bench}_${lib} || exit 1
    done
done
