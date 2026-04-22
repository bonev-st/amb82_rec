#!/usr/bin/env bash
# Bring the AMB82 Docker stack up. Idempotent.
set -euo pipefail

cd "$(dirname "$0")/.."

# Ensure host-side bind-mount targets exist and are owned by the
# invoking user. Without this, Docker creates them as root on first
# start which then breaks the mosquitto container (running as uid 1000).
mkdir -p mosquitto/data mosquitto/log

docker compose up -d --remove-orphans
docker compose ps
