#!/usr/bin/env bash
set -euo pipefail

# Build
BUILD_DIR="${BUILD_DIR:-build}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake ..
cmake --build . -j

# Run server + 3 clients quickly
PORT=8080
NUM_CLIENTS=3
MSG_PER_CLIENT=5

# Start server
./server "$PORT" "$NUM_CLIENTS" &
SERVER_PID=$!
sleep 0.2

# Start clients
./client 127.0.0.1 "$PORT" "$MSG_PER_CLIENT" client1.log &
./client 127.0.0.1 "$PORT" "$MSG_PER_CLIENT" client2.log &
./client 127.0.0.1 "$PORT" "$MSG_PER_CLIENT" client3.log &

wait $SERVER_PID || true

echo "Done. Logs:"
ls -l client*.log
echo "Sample (client1.log):"
head -n 5 client1.log || true
