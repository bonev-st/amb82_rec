#!/usr/bin/env bash
# Show compose state + a short tail of each container's logs.
set -euo pipefail

cd "$(dirname "$0")/.."

docker compose ps
echo

for svc in $(docker compose ps --services 2>/dev/null); do
    echo "=== $svc (last 5 log lines) ==="
    docker compose logs --no-color --tail=5 "$svc" 2>&1 || true
    echo
done
