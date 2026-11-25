#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <number_of_subscribers>"
    exit 1
fi

n=$1
if [ "$n" -le 0 ] || [ "$n" -ge 4 ]; then
    echo "Error: n must be between 1 and 3"
    exit 1
fi

# Compile programs
echo "Compiling..."
gcc -o publisher publisher.c
gcc -o subscriber subscriber.c

# Clean up old files and ensure the directory exists
rm -rf /tmp/ex1
mkdir -p /tmp/ex1

# Function to open a terminal emulator
open_term() {
    local title=$1
    local cmd=$2
    
    if command -v kitty &> /dev/null; then
        kitty --title "$title" bash -c "$cmd; echo 'Process finished. Press Enter to close.'; read" &
    elif command -v konsole &> /dev/null; then
        konsole --title "$title" -e bash -c "$cmd; echo 'Process finished. Press Enter to close.'; read" &
    elif command -v gnome-terminal &> /dev/null; then
        gnome-terminal --title="$title" -- bash -c "$cmd; echo 'Process finished. Press Enter to close.'; read" &
    elif command -v xterm &> /dev/null; then
        xterm -title "$title" -hold -e "$cmd" &
    else
        echo "No supported terminal emulator found."
        exit 1
    fi
}

# 1. Run publisher in a new terminal window
echo "Launching Publisher..."
open_term "Publisher" "./publisher $n"

# 2. Wait for the publisher to actually create the named pipes (FIFOs)
echo "Waiting for publisher to create pipes..."
for (( i=1; i<=n; i++ ))
do
    # -p checks if the file exists AND is a named pipe (FIFO)
    while [ ! -p "/tmp/ex1/s$i" ]; do
        sleep 0.1
    done
done
echo "Pipes created successfully. Launching subscribers..."

# 3. Run subscribers in new terminal windows
for (( i=1; i<=n; i++ ))
do
    open_term "Subscriber $i" "./subscriber $i"
done

echo "Started 1 publisher and $n subscribers in separate terminal windows."