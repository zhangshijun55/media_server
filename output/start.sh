#!/bin/bash

# Enter the directory where the script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

# Check if media_server executable exists
if [ ! -f "./media_server" ]; then
    echo "Error: media_server executable not found in $DIR"
    exit 1
fi

# Ensure it is executable
chmod +x ./media_server

# Set necessary limits (optional)
ulimit -c unlimited

# Start media_server in the background
# Output is redirected to /dev/null because the application likely handles its own logging (based on 'log' directory existence)
nohup ./media_server > /dev/null 2>&1 &

PID=$!
echo "Media server started with PID $PID"
