#!/usr/bin/env bash
# Bring the AMB82 Docker stack down.
set -euo pipefail

cd "$(dirname "$0")/.."

docker compose down --remove-orphans
