#!/bin/bash
# Script to stop the database server
echo "----------------------------------------"
echo "Stopping the database server..."
server_process_name="servidor"
pkill -f "$server_process_name"
if [ $? -eq 0 ]; then
    echo "Database server stopped successfully."
else
    echo "Failed to stop the database server or it was not running."
fi  