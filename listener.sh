#!/bin/bash

# Evan Digles and Anish Boddu
# Accompanying helper script meant to run in the user space since we were getting pissed off with apparmor preventing out every move
# in the kernel driver itself.
# Basically, we NEED this because apparmor won't let drivers do things themselves for most of he commands we want to use.

# Place the config file in a user-accessible directory so we won't have to run the script as root, because
# doing so would cause apparmor and other denials.
CONFIG_FILE="$HOME/.config/key_command_config" # Not an easily visible location because we intend for this to be edited in the GUI.
TMP_FILE="/tmp/key_pressed" # Make sure this is the same as what is set for the location in the driver.

# Ensure the configuration directory exists, if not, well...
mkdir -p "$(dirname "$CONFIG_FILE")"

# Generate default config file if it doesn't exist
if [ ! -f "$CONFIG_FILE" ]; then
    cat <<EOL > "$CONFIG_FILE" # Basically, this is ued to make multi-lines easier. So put the desired default config below.
64353=0:/usr/bin/firefox "https://www.youtube.com/watch?v=qWNQUvIk954"
64354=2000:xfce4-terminal
64353,64354=3000:/usr/bin/mousepad
EOL
    echo "Generated default config at $CONFIG_FILE" # DEBUG: If we needed to generate the file, let us know.
fi

while true; do # Monitor until the helper is killed.
    cat $TMP_FILE # read in from the location of the temp file.
    inotifywait -e close_write "$TMP_FILE" >/dev/null 2>&1 # Extremely convenient, but does require installation outside of the original provided iso scope. Check the readme!
    if [ $? -eq 0 ]; then # Start trying to read things in
        KEYS=$(cat "$TMP_FILE" | tr -d '\n')
        if [ -n "$KEYS" ]; then
            IFS=',' read -r -a PRESS_EVENTS <<< "$KEYS"

            while IFS= read -r LINE; do # If something got read, check the configuration for what keys to check, how long they need to be held, and what command to do if requirement are met. Cuts are to keep the config file to the format. 
                # If you want to change how the config file looks, edit the next section while exercising some caution.
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


                if $MATCH; then # If there is a match, check the hold time.
                    for EVENT in "${PRESS_EVENTS[@]}"; do
                        IFS=':' read -r KEY TIME <<< "$EVENT"
                        if (( TIME < HOLD_TIME )); then
                            MATCH=false
                            break
                        fi
                    done
                fi

                if $MATCH; then # If the key combination is correct, and the hold time is met, do the command and then don't let the script do anything else for 2 seconds to prevent spamming of held type commands.
                    echo "Executing: $COMMAND"
                    bash -c "$COMMAND" &
                    sleep 2 # God I wish we had a better implementation than needing to sleep between commands.
                    break
                fi
                
            done < "$CONFIG_FILE"
        fi
    fi
done
