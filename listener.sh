#!/bin/bash

# Place the config file in a user-accessible directory
CONFIG_FILE="$HOME/.config/key_command_config"
TMP_FILE="/tmp/key_pressed"

# Ensure the configuration directory exists
mkdir -p "$(dirname "$CONFIG_FILE")"

# Generate default config file if it doesn't exist
if [ ! -f "$CONFIG_FILE" ]; then
    cat <<EOL > "$CONFIG_FILE"
64353="/usr/bin/firefox \"https://www.youtube.com/watch?v=qWNQUvIk954\""
64354="xfce4-terminal"
EOL
    echo "Generated default config at $CONFIG_FILE"
fi

while true; do
    # Wait for a key press to be written
    inotifywait -e close_write "$TMP_FILE" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        # Read the key from the temporary file
        KEY=$(cat "$TMP_FILE" | tr -d '\n')

        if [ -n "$KEY" ]; then
            # Read the config file and look for a matching key
            while IFS= read -r LINE; do
                CONFIG_KEY=$(echo "$LINE" | cut -d '=' -f 1)
                COMMAND=$(echo "$LINE" | cut -d '=' -f 2- | tr -d '"')

                if [ "$KEY" = "$CONFIG_KEY" ]; then
                    echo "Executing: $COMMAND"
                    bash -c "$COMMAND" &
                    break
                fi
            done < "$CONFIG_FILE"
        else
            echo "No key found in $TMP_FILE."
        fi
    fi
done
