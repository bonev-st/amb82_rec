#!/usr/bin/env bash
# Tail logs. With no args: follow all services. With args: follow those
# services only. Ctrl-C to exit.
#
# Examples:
#   ./scripts/logs.sh              # follow everything
#   ./scripts/logs.sh recorder     # follow just the recorder
#   ./scripts/logs.sh mosquitto recorder
set -euo pipefail

cd "$(dirname "$0")/.."

docker compose logs -f "$@"
