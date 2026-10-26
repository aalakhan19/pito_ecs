#!/bin/sh
# usage: run.sh [--types update insert delete initialize local shared mixed bank uneven sor buffer] [--libs pico flecs pito pito_atomic] [--entities 100000 ...]
# Run from the build dir
# Default is to run all, at 100000 entities per system
export BENCH_RUN_ID=${BENCH_RUN_ID:-$(date +%Y-%m-%d_%H-%M-%S)}

types=
libs=
entities=
list=

for arg; do
    case $arg in
        --types) list=types ;;
        --libs) list=libs ;;
        --entities) list=entities ;;
        update | insert | delete | initialize | local | shared | mixed | bank | uneven | sor | buffer) [ "$list" = types ] && types="$types $arg" || list=bad ;;
        pico | flecs | pito | pito_atomic) [ "$list" = libs ] && libs="$libs $arg" || list=bad ;;
        *) [ "$list" = entities ] && [ "$arg" -gt 0 ] 2>/dev/null && entities="$entities $arg" || list=bad ;;
    esac

    if [ "$list" = bad ]; then
        echo "usage: $0 [--types update insert delete initialize local shared mixed bank uneven sor buffer] [--libs pico flecs pito pito_atomic] [--entities 100000 ...]" >&2
        exit 1
    fi
done

selected() {
    [ -z "$1" ] || case " $1 " in *" $2 "*) true ;; *) false ;; esac
}

since() {
    s=$(($(date +%s) - $1))
    printf '%dh %02dm %02ds' $((s / 3600)) $((s % 3600 / 60)) $((s % 60))
}

start=$(date +%s)

for bench in update insert delete initialize local shared mixed bank uneven sor buffer; do
    selected "$types" $bench || continue

    for lib in pico flecs pito pito_atomic; do
        selected "$libs" $lib || continue
        for n in ${entities:-100000}; do
            t=$(date +%s)
            ./bench_${bench}_${lib} $n || exit 1
            echo "bench_${bench}_${lib} $n took $(since $t)"
            echo
        done
    done
done

echo "total: $(since $start)"
