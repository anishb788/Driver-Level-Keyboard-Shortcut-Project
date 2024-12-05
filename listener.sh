#!/bin/bash

DEVICE="/dev/key_event_device"

# Device check
if [ ! -e "$DEVICE" ]; then
    echo "Device $DEVICE does not exist."
    exit 1
fi

# Continuous reading
while true; do
    if read -r line < "$DEVICE"; then
        if [[ -z "$line" ]]; then
            echo "Warning: Empty line received from $DEVICE" >&2
            continue
        fi
        echo "Detected: $line"
        xdg-open "https://www.youtube.com/watch?v=dQw4w9WgXcQ" &
        sleep 1  # Prevent spamming
    else
        echo "Error reading from $DEVICE" >&2
        sleep 1  # Prevent busy looping on error
    fi
done
