#!/bin/bash
FIFO="/tmp/trigger_action"
URL="https://www.youtube.com/watch?v=dQw4w9WgXcQ"

# Create the FIFO if it doesn't exist
[ ! -p $FIFO ] && mkfifo $FIFO

while true; do
    if read line < $FIFO; then
        if [ "$line" = "open_firefox" ]; then
            firefox "$URL" &
        fi
    fi
done
