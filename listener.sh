#!/bin/bash

FILE="/tmp/trigger_action"

while true; do
    sleep 1
    inotifywait -e close_write "$FILE" && \
    cat "$FILE" | grep -q "open_firefox" && \
    #/usr/bin/firefox "https://www.youtube.com/watch?v=dQw4w9WgXcQ"
    echo "Yeah, it's working."
done
