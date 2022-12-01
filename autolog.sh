#!/bin/bash

exec 1> >(logger -s -t $(basename "$0")) 2>&1

echo "Logging test llama"
echo -e "Logging test pygmy\n"
