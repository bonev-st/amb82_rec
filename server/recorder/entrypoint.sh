#!/bin/sh
# Verify /clips is writable by the recorder user before starting the main
# process. Bind-mounted volumes created by docker-compose on a fresh host
# can end up root-owned, which silently breaks the first recording.
set -e

if ! ( : > /clips/.write_test ) 2>/dev/null; then
    uid=$(id -u)
    echo "ERROR: /clips is not writable by uid $uid." >&2
    echo "The recorder container runs as uid 1000. If your host mount" >&2
    echo "./clips was created by docker as root, fix it:" >&2
    echo "  sudo chown 1000:1000 \"\$(pwd)/clips\"" >&2
    echo "or switch the compose volume to a named volume." >&2
    exit 1
fi
rm -f /clips/.write_test

exec "$@"
