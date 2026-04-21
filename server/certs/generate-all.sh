#!/bin/bash
# ============================================================
# generate-all.sh
#
# One-shot: runs the full cert generation pipeline needed for this project:
#   1. Self-signed CA
#   2. Broker server certificate (with SANs)
#   3. Two client certificates (camera + recorder)
#   4. Mosquitto password file with two accounts
#
# After this completes, proceed to install-broker.sh (requires sudo) to
# deploy the certs into /etc/mosquitto/certs/ on the broker.
#
# Usage:
#   ./generate-all.sh
#   ./generate-all.sh /path/to/output-dir
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${1:-$SCRIPT_DIR/out}"

# Regenerating the CA invalidates every existing client cert. If we already
# have one, bail and direct the user at generate-client.sh for incremental
# work. generate-ca.sh itself prompts too, but this guard avoids re-running
# the server cert and password generation on accident.
if [[ -f "$OUT_DIR/ca.crt" ]]; then
    echo "==> CA already exists at $OUT_DIR/ca.crt."
    echo "    Regenerating the CA would invalidate every deployed client cert."
    echo "    To add a new client, run:  ./generate-client.sh <name>"
    echo "    To force a full rebuild, remove $OUT_DIR first."
    exit 0
fi

echo "#################################################"
echo "  1/4  Generating self-signed CA"
echo "#################################################"
"$SCRIPT_DIR/generate-ca.sh" "$OUT_DIR"

echo
echo "#################################################"
echo "  2/4  Generating server certificate"
echo "#################################################"
"$SCRIPT_DIR/generate-server.sh" "$OUT_DIR"

echo
echo "#################################################"
echo "  3/4  Generating client certificates"
echo "#################################################"
"$SCRIPT_DIR/generate-client.sh" amb82_cam_01 "$OUT_DIR"
"$SCRIPT_DIR/generate-client.sh" recorder     "$OUT_DIR"

echo
echo "#################################################"
echo "  4/4  Creating Mosquitto password file"
echo "#################################################"
"$SCRIPT_DIR/generate-passwords.sh" "$OUT_DIR"

echo
echo "#################################################"
echo "  ALL DONE."
echo "#################################################"
echo "Generated files in $OUT_DIR:"
ls -la "$OUT_DIR"
echo
echo "Next steps:"
echo "  1. Install broker certs:   sudo $SCRIPT_DIR/install-broker.sh [$OUT_DIR]"
echo "  2. Copy PEM contents of ca.crt, client_amb82_cam_01.crt, and"
echo "     client_amb82_cam_01.key into ../../mqtt_certs.h, recompile firmware."
echo "  3. Deploy client_recorder.crt + client_recorder.key + ca.crt to"
echo "     ~/amb82_recorder/certs/ on the broker host for the recorder service."
echo
echo "!!!  SECURITY  !!!"
echo "  $OUT_DIR/ca.key is the root of trust for every client cert."
echo "  Move it off this machine (encrypted USB / password manager / HSM)"
echo "  once deployment is complete. Never commit it."
