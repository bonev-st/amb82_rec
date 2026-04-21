#!/bin/bash
# ============================================================
# install-broker.sh
#
# Installs the CA, server cert/key, and password file into
# /etc/mosquitto/certs/ and writes the Mosquitto config to
# /etc/mosquitto/conf.d/secure.conf. Restarts Mosquitto.
#
# Must be run on the broker host. Requires sudo (will prompt).
#
# The resulting broker config has TWO listeners:
#   1883 -- anonymous (backward-compatible plain listener)
#   8883 -- TLS + mTLS + password auth
#
# Usage:
#   ./install-broker.sh
#   ./install-broker.sh /path/to/certs-dir
#
# The input directory must contain:
#   ca.crt, server.crt, server.key, passwd
# (i.e. the output of generate-all.sh)
# ============================================================
set -euo pipefail

# Resolve source dir from the script's own location -- NOT $HOME -- because
# sudo rewrites $HOME to /root and ~ then points to the wrong place.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="${1:-$SCRIPT_DIR/out}"

for f in ca.crt server.crt server.key passwd; do
    if [[ ! -f "$SRC_DIR/$f" ]]; then
        echo "ERROR: $SRC_DIR/$f not found. Run generate-all.sh first."
        exit 1
    fi
done

echo "==> Installing certs to /etc/mosquitto/certs/ ..."
sudo mkdir -p /etc/mosquitto/certs
sudo cp "$SRC_DIR/ca.crt"     /etc/mosquitto/certs/
sudo cp "$SRC_DIR/server.crt" /etc/mosquitto/certs/
sudo cp "$SRC_DIR/server.key" /etc/mosquitto/certs/
sudo cp "$SRC_DIR/passwd"     /etc/mosquitto/certs/
sudo chown -R mosquitto:mosquitto /etc/mosquitto/certs
sudo chmod 600 /etc/mosquitto/certs/server.key /etc/mosquitto/certs/passwd

echo "==> Writing /etc/mosquitto/conf.d/secure.conf ..."
sudo tee /etc/mosquitto/conf.d/secure.conf >/dev/null <<'EOF'
# ============================================================
# Dual-listener config:
#   :1883 -- plain, anonymous (backward compat)
#   :8883 -- TLS + mTLS + password auth
# ============================================================
per_listener_settings true

# Plain listener -- bound to localhost only. Do NOT expose anonymous MQTT
# to the LAN; any client would be able to subscribe to camera topics
# (RTSP URLs, battery, motion) and publish fake motion events.
listener 1883 127.0.0.1
allow_anonymous true

# TLS listener
listener 8883 0.0.0.0
allow_anonymous false
password_file /etc/mosquitto/certs/passwd
cafile   /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile  /etc/mosquitto/certs/server.key
require_certificate     true
use_identity_as_username false
EOF

echo "==> Restarting Mosquitto ..."
sudo systemctl restart mosquitto
sleep 1
sudo systemctl status mosquitto --no-pager | head -10

echo
echo "==> Verifying both listeners are up:"
ss -tln | grep -E '(:1883|:8883)' || true

echo
echo "==> Done. Sanity checks you can run from this host:"
echo "  # anonymous on 1883 (should succeed):"
echo "  mosquitto_pub -h 127.0.0.1 -p 1883 -t test -m hello"
echo
echo "  # mTLS + auth on 8883 (should succeed with valid creds):"
echo "  mosquitto_pub -h 127.0.0.1 -p 8883 \\"
echo "    --cafile $SRC_DIR/ca.crt \\"
echo "    --cert   $SRC_DIR/client_amb82_cam_01.crt \\"
echo "    --key    $SRC_DIR/client_amb82_cam_01.key \\"
echo "    -u amb82_cam_01 -P '<password>' \\"
echo "    -t test -m hello"
