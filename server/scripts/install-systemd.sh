#!/usr/bin/env bash
# Installs /etc/systemd/system/amb82-stack.service so the Docker stack
# starts automatically at boot.
#
# The unit runs `docker compose up -d` as the INVOKING user in this
# server/ directory. That user must be in the `docker` group.
#
# Run this script with `sudo` available (it uses sudo itself for the
# privileged steps; the rest runs as you).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
UNIT_NAME="amb82-stack.service"
UNIT_DST="/etc/systemd/system/$UNIT_NAME"
RUN_USER="$(id -un)"
RUN_GROUP="$(id -gn)"

# Sanity: user must be a member of the docker group, otherwise the unit
# will fail to talk to /var/run/docker.sock at boot.
if ! id -nG "$RUN_USER" | tr ' ' '\n' | grep -qx docker; then
    echo "ERROR: user '$RUN_USER' is not in the 'docker' group." >&2
    echo "       Add with: sudo usermod -aG docker $RUN_USER   (then log out / back in)" >&2
    exit 1
fi

DOCKER_BIN="$(command -v docker)"
if [[ -z "$DOCKER_BIN" ]]; then
    echo "ERROR: docker not found in PATH." >&2
    exit 1
fi

echo "==> Writing $UNIT_DST"
sudo tee "$UNIT_DST" >/dev/null <<EOF
[Unit]
Description=AMB82 Docker stack (mosquitto + recorder + HomeAssistant)
Requires=docker.service
After=docker.service network-online.target
Wants=network-online.target

[Service]
Type=oneshot
RemainAfterExit=true
User=$RUN_USER
Group=$RUN_GROUP
WorkingDirectory=$SERVER_DIR
# Ensure bind-mount targets exist; harmless if they already do.
ExecStartPre=/bin/mkdir -p $SERVER_DIR/mosquitto/data $SERVER_DIR/mosquitto/log
ExecStart=$DOCKER_BIN compose up -d --remove-orphans
ExecStop=$DOCKER_BIN compose down --remove-orphans
TimeoutStartSec=180
TimeoutStopSec=60

[Install]
WantedBy=multi-user.target
EOF

sudo chmod 0644 "$UNIT_DST"
sudo systemctl daemon-reload
sudo systemctl enable "$UNIT_NAME"

echo
echo "==> Enabled $UNIT_NAME. To start now:"
echo "    sudo systemctl start $UNIT_NAME"
echo
echo "==> Status:"
sudo systemctl status --no-pager "$UNIT_NAME" || true
