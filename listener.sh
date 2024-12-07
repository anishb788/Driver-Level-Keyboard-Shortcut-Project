#!/bin/bash

FILE="/tmp/trigger_action"
USER="$USER"  # Replace with the actual username

while true; do
    inotifywait -e close_write "$FILE" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        COMMAND=$(cat "$FILE")
        if [ -n "$COMMAND" ]; then
            echo "Executing: $COMMAND"
            sudo -u "$USER" bash -c "$COMMAND" &
            echo "Command executed successfully."
        else
            echo "No command found in $FILE."
        fi
    fi
done
