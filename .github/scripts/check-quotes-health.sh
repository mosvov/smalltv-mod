#!/usr/bin/env bash
# Verify the data branch exists, is recent, and contains every configured quote.
set -euo pipefail

MAX_AGE_MINUTES="${MAX_AGE_MINUTES:-30}"
REPO="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY required}"

if ! git ls-remote --heads origin data 2>/dev/null | grep -q 'refs/heads/data'; then
  echo "::warning::data branch does not exist yet."
  if [ "${AUTO_BOOTSTRAP_QUOTES:-}" = "true" ]; then
    echo "Triggering quotes workflow to create the data branch..."
    gh workflow run quotes.yml --repo "$REPO"
    echo "Bootstrap triggered; health check will pass after quotes completes."
    exit 0
  fi
  echo "::error::data branch does not exist. Run the quotes workflow (Actions → quotes → Run workflow)."
  exit 1
fi

git fetch --depth=1 origin data

COMMIT_TS=$(git show -s --format=%ct origin/data)
NOW_TS=$(date +%s)
AGE_MIN=$(( (NOW_TS - COMMIT_TS) / 60 ))

echo "data branch tip is ${AGE_MIN} minute(s) old (limit: ${MAX_AGE_MINUTES})"
if [ "$AGE_MIN" -gt "$MAX_AGE_MINUTES" ]; then
  echo "::error::data branch is stale (${AGE_MIN}m). Check the quotes workflow schedule."
  exit 1
fi

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT
git --work-tree="$WORKDIR" checkout origin/data -- quotes

KEYS=$(node -e "
  const cfg = require('./quotes-config.json');
  cfg.symbols.forEach(s => console.log(s.key));
")

MISSING=()
while IFS= read -r key; do
  [ -z "$key" ] && continue
  if [ ! -f "$WORKDIR/quotes/${key}.json" ]; then
    MISSING+=("$key")
  fi
done <<< "$KEYS"

if [ ${#MISSING[@]} -gt 0 ]; then
  echo "::error::Missing quote files on data branch: ${MISSING[*]}"
  exit 1
fi

KEY_COUNT=$(echo "$KEYS" | grep -c . || true)
echo "data branch healthy: ${KEY_COUNT} quote file(s), ${AGE_MIN}m old"
