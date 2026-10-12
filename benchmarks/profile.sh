#!/bin/sh
# usage: [DURATION=30] [PROFILE_RUN_ID=...] profile.sh <bench> <pito|pito_atomic> <variant> <threads> <work> [entities]
# Output in profiles/<host>_<run id>/ (run id defaults to the date, like the CSVs; set it to collect several
# profiles in one folder): <name>.svg (timed work), <name>.reversed.svg (same, by leaf), <name>.full.svg (whole
# process), the collapsed stacks they're drawn from, and info.txt. Binary and raw samples live in a temp dir, deleted on exit.
# macOS: sample + brew flamegraph. Linux: perf, perl, addr2line, FlameGraph in $FLAMEGRAPH_DIR (default ~/FlameGraph),
# kernel.perf_event_paranoid <= 1 (or sudo); libc6-dbg names libc's internals.
set -e
cd "$(dirname "$0")"
[ $# -ge 5 ] || { sed -n 2p "$0" >&2; exit 1; }

BENCH=$1 LIB=$2 VARIANT=$3 THREADS=$4 WORK=$5 ENTITIES=${6:-100000}
DURATION=${DURATION:-30}
NAME="${BENCH}_${LIB}_v${VARIANT}_t${THREADS}_w${WORK}"
DIR=profiles/$(hostname -s)_${PROFILE_RUN_ID:-$(date +%Y-%m-%d_%H-%M-%S)}
OUT=$DIR/$NAME
SCRATCH=$(cd "$(mktemp -d)" && pwd -P)
trap 'rm -rf "$SCRATCH"' EXIT
TMP=$SCRATCH/$NAME
mkdir -p "$DIR"

CFLAGS="-std=gnu11 -O3 -g -DNDEBUG -fno-omit-frame-pointer -pthread -I.."
case $LIB in
    pito)        ;;
    pito_atomic) CFLAGS="$CFLAGS -DPITO_ECS_OWNED_DELETE_ATOMIC=1" ;;
    *)           sed -n 2p "$0" >&2; exit 1 ;;
esac
if [ "$(uname)" = Darwin ]; then
    FG=$(brew --prefix flamegraph)/bin
    CPU=$(sysctl -n machdep.cpu.brand_string)
else
    FG=${FLAMEGRAPH_DIR:-$HOME/FlameGraph}
    CPU=$(sed -n 's/^model name[^:]*: //p' /proc/cpuinfo | head -1)
fi
# Line numbers in the graphs refer to the header at this commit.
[ -f "$DIR/info.txt" ] || cat > "$DIR/info.txt" << EOF
commit: $(git rev-parse --short HEAD)$(git diff --quiet HEAD -- .. || echo ' (dirty)')
host: $(hostname -s), $CPU, $(getconf _NPROCESSORS_ONLN) hardware threads, $(uname -sr)
compiler: $(cc --version | head -1)
flags: $CFLAGS (pito_atomic adds -DPITO_ECS_OWNED_DELETE_ATOMIC=1)
duration: ${DURATION}s per profile
EOF

cc $CFLAGS -DBENCH_FLAGS="\"$CFLAGS\"" -o "$TMP" "owned_$BENCH.c" -lm
# Profiling mode: repeats this one configuration until the time is up, no CSV.
"$TMP" "$ENTITIES" "$VARIANT" "$THREADS" "$WORK" $((DURATION + 1)) &
PID=$!
if [ "$(uname)" = Darwin ]; then
    sample "$PID" "$DURATION" -file "$TMP.sample.txt" > /dev/null 2>&1
else
    # DWARF unwinding: libc has no frame pointers, so a sample in pthread_mutex_lock would lose its
    # caller. 2048 bytes of stack keep the same stacks as the default 8192 at a third of the size.
    perf record -q -F 997 --call-graph dwarf,2048 -o "$TMP.perf.data" -p "$PID" -- sleep "$DURATION"
fi
wait "$PID"

# Sums identical stacks, drops empty ones: the files are committed, unmerged they reach 14 MB.
merge()
{
    awk '$NF > 0 {n = $NF; sub(/ [0-9]+$/, ""); s[$0] += n} END {for (k in s) print k, s[k]}' | sort
}

# Every frame becomes fn@file:line: at -O3 the library is inlined into the system, so the line is what
# tells e.g. ecs_get's lock apart from the system's own code.
if [ "$(uname)" = Darwin ]; then
    sed -E 's/^([^0-9]*[0-9]+ )([^ ]+)(  \(in .*  )([^ ]+:[0-9]+)$/\1\2@\4\3\4/' "$TMP.sample.txt" \
        | awk -f "$FG/stackcollapse-sample.awk" | sed -E 's/[^;` ]+`//g' > "$TMP.raw.txt"
else
    perf script --no-inline -F comm,tid,ip,sym,dso -i "$TMP.perf.data" > "$TMP.perf.txt" 2> /dev/null
    perl profile_lines.pl "$TMP" "$TMP.perf.txt" | "$FG/stackcollapse-perf.pl" \
        | sed -E 's/_\[i\]|@@GLIBC_[0-9.]+//g' > "$TMP.raw.txt"
fi
# Thread name dropped: every run starts new workers, which would otherwise be thousands of slivers.
sed 's/^[^;]*;//' "$TMP.raw.txt" | merge > "$OUT.raw.txt"

# Timed work only: stacks cut to start at run_systems/finish, so main thread and workers merge.
sed -E 's/^.*;((run_systems|finish)[@;])/\1/' "$OUT.raw.txt" | grep -E '^(run_systems|finish)[@;]' \
    | merge > "$OUT.collapsed.txt"

OPTS="--width 1800 --minwidth 0.1 --hash --countname samples"
"$FG/flamegraph.pl" $OPTS --title "$NAME" "$OUT.collapsed.txt" > "$OUT.svg"
"$FG/flamegraph.pl" $OPTS --reverse --inverted --title "$NAME (reversed)" "$OUT.collapsed.txt" > "$OUT.reversed.svg"
"$FG/flamegraph.pl" $OPTS --title "$NAME (full)" "$OUT.raw.txt" > "$OUT.full.svg"
echo "$OUT.svg $OUT.reversed.svg $OUT.full.svg"

# Top self time, same as the top row of the reversed graph.
awk '{n = $NF; sub(/ [0-9]+$/, ""); k = split($0, f, ";"); self[f[k]] += n; total += n}
     END {for (s in self) printf "%5.1f%%  %s\n", 100 * self[s] / total, s}' "$OUT.collapsed.txt" | sort -rn | head -10
