#!/bin/bash

# Stop media_server
pkill -x media_server

if [ $? -eq 0 ]; then
    echo "Media server stopped."
else
    echo "Media server was not running."
fi
