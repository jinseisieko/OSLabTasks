#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <watched_directory>"
    exit 1
fi

WATCH_DIR="$1"

mkdir -p "$WATCH_DIR"

echo "Compiling monitor.c and ex1.c..."
gcc -Wall -o monitor monitor.c
gcc -Wall -o ex1 ex1.c

if [ $? -ne 0 ]; then
    echo "Compilation failed."
    exit 1
fi

echo "Starting monitor..."
./monitor "$WATCH_DIR" &
MONITOR_PID=$!

sleep 1

echo "Running ex1 to make specific changes..."
./ex1 "$WATCH_DIR"

echo "Running ex1_test.sh for arbitrary changes..."
./ex1_test.sh "$WATCH_DIR"

echo "Stopping monitor..."
kill -SIGINT $MONITOR_PID
wait $MONITOR_PID

echo "Done."