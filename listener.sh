#!/bin/bash

# Place the config file in a user-accessible directory
CONFIG_FILE="$HOME/.config/key_command_config"
TMP_FILE="/tmp/key_pressed"

# Ensure the configuration directory exists
mkdir -p "$(dirname "$CONFIG_FILE")"

# Generate default config file if it doesn't exist
if [ ! -f "$CONFIG_FILE" ]; then
    cat <<EOL > "$CONFIG_FILE"
64353=0:/usr/bin/firefox "https://www.youtube.com/watch?v=qWNQUvIk954"
64354=2000:xfce4-terminal
64353,64354=3000:/usr/bin/mousepad
EOL
    echo "Generated default config at $CONFIG_FILE"
fi

while true; do
    cat $TMP_FILE
    inotifywait -e close_write "$TMP_FILE" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        KEYS=$(cat "$TMP_FILE" | tr -d '\n')
        if [ -n "$KEYS" ]; then
            IFS=',' read -r -a PRESS_EVENTS <<< "$KEYS"

            while IFS= read -r LINE; do
                CONFIG_KEYS=$(echo "$LINE" | cut -d '=' -f 1)
                HOLD_TIME=$(echo "$LINE" | cut -d ':' -f 1 | cut -d '=' -f 2)
                COMMAND=$(echo "$LINE" | cut -d ':' -f 2-)

                MATCH=true
                for CONFIG_KEY in $(echo "$CONFIG_KEYS" | tr ',' ' '); do
                    if ! echo "${PRESS_EVENTS[@]}" | grep -q "$CONFIG_KEY"; then
                        MATCH=false
                        break
                    fi
                done

                if $MATCH; then
                    for EVENT in "${PRESS_EVENTS[@]}"; do
                        IFS=':' read -r KEY TIME <<< "$EVENT"
                        if (( TIME < HOLD_TIME )); then
                            MATCH=false
                            break
                        fi
                    done
                fi

                if $MATCH; then
                    echo "Executing: $COMMAND"
                    bash -c "$COMMAND" &
                    break
                fi
            done < "$CONFIG_FILE"
        fi
    fi
done
