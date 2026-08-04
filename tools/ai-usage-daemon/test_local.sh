#!/bin/sh
set -e
cd "$(dirname "$0")"
python3 daemon.py --once | head -40
echo "---"
python3 daemon.py --serve &
PID=$!
sleep 2
curl -sS "http://127.0.0.1:8787/" | head -20
kill "$PID" 2>/dev/null || true
