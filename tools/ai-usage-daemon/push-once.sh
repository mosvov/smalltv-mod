#!/bin/sh
# Fetch usage once and push to SmallTV. Used by cron during work hours.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="${AI_USAGE_TARGET:-http://192.168.5.11}"
cd "$DIR"
exec "$DIR/.venv/bin/python" daemon.py --push-once --push-to "$TARGET"
