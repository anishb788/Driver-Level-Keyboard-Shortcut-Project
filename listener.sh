#!/bin/bash

FILE="/tmp/trigger_action"
USER="$USER"  # Replace with the actual username

while true; do
    sleep 1
    inotifywait -e close_write "$FILE" && \
    cat "$FILE" | grep -q "open_firefox" && \
    sudo -u "$USER" /usr/bin/firefox "https://www.youtube.com/watch?v=dQw4w9WgXcQ"
    echo "Yeah, it's working."
done
