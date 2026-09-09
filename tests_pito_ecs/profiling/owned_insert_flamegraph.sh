#!/bin/sh
set -e

cd "$(dirname "$0")"

MODE="${1:-v1}"

case "$MODE" in
    v1)
        DEFINE=
        TITLE="owned_insert (entity_lock), no work, 4 threads"
        SUFFIX=
        ;;
    v2)
        DEFINE=-DPITO_ECS_OWNED_DELETE_ATOMIC=1
        TITLE="owned_insert (atomic), no work, 4 threads"
        SUFFIX=_v2
        ;;
    *)
        echo "usage: $0 [v1|v2]" >&2
        exit 1
        ;;
esac

BIN="owned_insert_fg$SUFFIX"

clang -std=c11 -O2 -g -fno-omit-frame-pointer -Wall -Wextra -pthread $DEFINE \
    -o "$BIN" owned_insert_fg.c -lm

sample "$BIN" 12 -wait -file "sample_insert$SUFFIX.txt" &
SAMPLE_PID=$!

"./$BIN" &

wait "$SAMPLE_PID"

awk -f "$(brew --prefix flamegraph)/bin/stackcollapse-sample.awk" "sample_insert$SUFFIX.txt" > "collapsed_insert$SUFFIX.txt"
flamegraph.pl --title "$TITLE" "collapsed_insert$SUFFIX.txt" > "flamegraph_insert$SUFFIX.svg"

echo "wrote flamegraph_insert$SUFFIX.svg"
