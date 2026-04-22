#!/usr/bin/env bash
# Full recycle: down + up. Useful after editing docker-compose.yml,
# mosquitto.conf, or the cert files.
set -euo pipefail

cd "$(dirname "$0")/.."

docker compose down --remove-orphans
mkdir -p mosquitto/data mosquitto/log
docker compose up -d --remove-orphans
docker compose ps
