#!/bin/sh
# Install cron: push usage to SmallTV every 30 min, Mon–Fri 9:00–17:59.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
SCRIPT="$DIR/push-once.sh"
TARGET="${1:-http://192.168.5.11}"
MARKER="# smalltv ai-usage-daemon"
CRON_LINE="*/30 9-17 * * 1-5 AI_USAGE_TARGET=$TARGET $SCRIPT >> $DIR/cron.log 2>&1 $MARKER"

chmod +x "$SCRIPT"
if [ ! -x "$DIR/.venv/bin/python" ]; then
  echo "Missing venv. Run: python3 -m venv .venv && source .venv/bin/activate && pip install -r requirements.txt"
  exit 1
fi

TMP="$(mktemp)"
crontab -l 2>/dev/null | grep -v "$MARKER" | grep -v "push-once.sh" >"$TMP" || true
echo "$CRON_LINE" >>"$TMP"
crontab "$TMP"
rm -f "$TMP"

echo "Installed cron (Mon–Fri, every 30 min, 9:00–17:59):"
echo "  $CRON_LINE"
echo ""
echo "Logs: $DIR/cron.log"
echo "Remove: crontab -l | grep -v '$MARKER' | crontab -"
