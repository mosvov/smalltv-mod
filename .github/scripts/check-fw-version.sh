#!/usr/bin/env bash
# Fail when firmware-affecting files change without bumping FW_VERSION in
# src/config.h. Prevents silently overwriting a GitHub release tag on main.
set -euo pipefail

BASE_REF="${1:?base ref (commit SHA or branch) required}"

if [ "$BASE_REF" = "0000000000000000000000000000000000000000" ]; then
  echo "Initial push; skipping version check."
  exit 0
fi

if ! git cat-file -e "${BASE_REF}^{commit}" 2>/dev/null; then
  echo "Base ref ${BASE_REF} not found; skipping version check."
  exit 0
fi

CHANGED=$(git diff --name-only "${BASE_REF}" HEAD -- src platformio.ini partitions || true)
if [ -z "$CHANGED" ]; then
  echo "No firmware-affecting files changed; version bump not required."
  exit 0
fi

echo "Firmware-affecting changes:"
echo "$CHANGED" | sed 's/^/  /'

OLD_VER=$(git show "${BASE_REF}:src/config.h" | grep '#define FW_VERSION' | head -1 | sed -E 's/.*"([^"]+)".*/\1/')
NEW_VER=$(grep '#define FW_VERSION' src/config.h | head -1 | sed -E 's/.*"([^"]+)".*/\1/')

if [ "$OLD_VER" = "$NEW_VER" ]; then
  echo "::error::Firmware changed but FW_VERSION is still ${NEW_VER}. Bump FW_VERSION in src/config.h."
  exit 1
fi

echo "FW_VERSION bumped: ${OLD_VER} -> ${NEW_VER}"
