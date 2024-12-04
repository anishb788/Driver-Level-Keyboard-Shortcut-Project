#!/bin/bash

DEVICE="/dev/key_event_device"

# Continuously read from the character device
while true; do
    if read -r line < "$DEVICE"; then
        echo "Detected: $line"
        firefox "https://www.youtube.com/watch?v=dQw4w9WgXcQ" &
    fi
done
