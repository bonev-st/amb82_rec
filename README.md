# AMB82 Mini Motion-Triggered RTSP Streamer

Motion-triggered video recorder for the Ameba AMB82 Mini camera board. The camera runs an RTSP stream (Ch0, FHD H264) continuously; when on-board motion detection (Ch3, RGB) fires, the firmware publishes an MQTT event with the RTSP URL so a Linux server can pull and record the clip. No local SD card storage required.

## Architecture

```
AMB82 Mini                              Linux Server
+-----------------------+               +---------------------------+
| Ch3 (VGA,RGB,10fps)   |               |                           |
|   -> MotionDetection  |--MQTT-------->| Mosquitto (MQTT broker)   |
|      (always on)      |  start/stop   |                           |
|                       |               | recorder.py               |
| Ch0 (FHD,H264,30fps)  |               |   subscribes to MQTT      |
|   -> RTSP server <----+--RTSP pull----+   spawns FFmpeg on start  |
|      (always on)      |  rtsp://:554  |   kills FFmpeg on stop    |
|                       |               |   saves MP4 clips         |
| MQTT client           |               |                           |
| Battery monitor       |               | Home Assistant            |
+-----------------------+               |   notifications/UI        |
                                        +---------------------------+
```

## Hardware

- **Board:** Ameba AMB82 Mini (Realtek RTL8735B, ARM Cortex-M33)
- **Power:** Battery (LiPo) or USB -- supports both modes
- **No SD card needed** -- video is stored on the server

### LED indicators

Two on-board LEDs report live status without needing a serial monitor.

**Green** (`LED_G`, PE_6) -- motion / record state:

| State | Meaning |
|---|---|
| Off | No motion; server is not recording. |
| Blink 0.5 s / 0.5 s | Motion detected by the algorithm, but the server has not yet been notified (MQTT publish pending/retrying). |
| Solid on | Motion notified -- server is (or was) recording. Stays on during post-roll. |

**Blue** (`LED_BUILTIN`, PF_9) -- system health. Priority ladder, highest first:

| State | Meaning |
|---|---|
| Solid on | WiFi not associated. |
| Blink 0.5 s / 0.5 s | WiFi OK, but NTP has not produced a valid epoch yet (TLS will fail until it does). |
| Blink 0.5 s on / 3.0 s off | WiFi + NTP OK, but the MQTT broker is not connected. |
| Off | All OK. |

## Build & Flash

1. Install Arduino IDE
2. Add Ameba board package URL: `https://github.com/ambiot/ambpro2_arduino`
3. Select board **AMB82 Mini**
4. Copy `secrets.h.example` -> `secrets.h` and fill in WiFi SSID/password and
   MQTT broker IP / user / password. `secrets.h` is gitignored. When
   `MQTT_USE_TLS=1` (the default in `config.h`), `MQTT_BROKER` **must** be an
   IPv4 literal -- mbedTLS 2.28 on this platform cannot verify hostname SANs,
   and the firmware logs a fatal error at boot if it detects a hostname.
5. If using TLS (default), copy `mqtt_certs.h.example` -> `mqtt_certs.h` and
   paste in the CA + client cert + client key PEMs produced by
   `server/certs/generate-all.sh`. See "MQTT Security" below for details.
6. Adjust `config.h` as needed (device ID, build mode, thresholds,
   `MQTT_USE_TLS`). WiFi/MQTT credentials are **not** in this file.
7. Select build mode (see below)
8. Remove any user-installed PubSubClient from Arduino libraries (use SDK version)
9. Compile and upload via USB

## Build Modes (DEBUG / RELEASE)

The firmware supports two build modes controlled by a single `#define` in
`config.h`. **Exactly one** must be active:

```c
// Uncomment ONE of these lines in config.h:
// #define BUILD_RELEASE    // Production
#define BUILD_DEBUG         // Development (default)
```

| Feature | DEBUG | RELEASE |
|---------|-------|---------|
| Serial output | Full logging at 115200 baud | Disabled (UART off) |
| Boot delay | 2s (for serial monitor) | None |
| MD region logging | Every 500ms | Disabled |
| Detection FPS | 10 fps | 5 fps (power saving) |
| Main loop delay (idle) | 100ms | 500ms (power saving) |
| Main loop delay (streaming) | 100ms | 100ms |
| Battery check interval | 60s | 120s |
| Config poll interval | 1 min | 5 min |
| Watchdog timer | Disabled | Enabled (30s timeout) |
| MQTT status payload | Includes `firmware` + `build` fields | Same |

### Switching modes

1. Open `config.h`
2. Comment out `#define BUILD_DEBUG` and uncomment `#define BUILD_RELEASE` (or vice versa)
3. Recompile and flash

### When to use each mode

- **DEBUG** -- bench testing, serial monitor attached, diagnosing motion detection
  sensitivity, verifying MQTT payloads
- **RELEASE** -- deployed camera running on battery, no serial monitor, maximum
  battery life, watchdog auto-recovery from hangs

## Server Setup

### Requirements

- Linux machine (Raspberry Pi 4+, VM, or any Linux box)
- Docker and Docker Compose
- 2+ GB RAM, storage for clips

### Quick Start

```bash
cd server
docker compose up -d
```

This starts:
- **Mosquitto** (port 1883) -- MQTT broker
- **recorder** -- MQTT-triggered FFmpeg recorder, saves clips to `server/clips/`

### How Recording Works

The camera's Ch0 H264 encoder and RTSP server run continuously -- dynamically
toggling them per motion event produced stuck/static-image clips in testing,
so they stay alive. The motion event is purely an MQTT notification that
tells the server when to start/stop the ffmpeg pull.

1. Camera detects motion locally (Ch3, low-res RGB).
2. Camera publishes MQTT: `{"motion":true, "rtsp":"rtsp://192.168.1.x:554"}`.
3. `recorder.py` receives MQTT, spawns FFmpeg to pull the already-running
   RTSP stream.
4. Motion ends (plus post-roll), camera publishes `{"motion":false}`.
5. `recorder.py` sends SIGTERM to FFmpeg so the MP4 is finalized cleanly.
6. Clip saved to `server/clips/<device_id>/<date>/<timestamp>.mp4`.
7. Clip metadata published back to MQTT for Home Assistant.

A background reaper in `recorder.py` also finalizes any clip whose FFmpeg
process exited on its own (hit `-t MAX_DURATION`, lost RTSP, crashed) so
capped recordings are not lost if the firmware's stop event never arrives.

### Clips Directory Structure

```
server/clips/
  amb82_cam_01/
    2026-04-06/
      amb82_cam_01_14-30-22.mp4
      amb82_cam_01_15-12-08.mp4
```

## MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `camera/<id>/motion` | Camera -> Server | `{"motion":true/false, "rtsp":"rtsp://..."}` |
| `camera/<id>/status` | Camera -> Server | `{"firmware":"1.0.0", "build":"RELEASE", "timestamp":..., "battery_pct":85, "rssi":-45, "uptime":3600}` |
| `camera/<id>/battery` | Camera -> Server | `{"alert":"LOW", "battery_pct":18, "timestamp":1776025299}` |
| `camera/<id>/clip` | Server -> HA | `{"file":"/clips/...", "size_bytes":...}` |
| `camera/<id>/availability` | Camera -> Server | `"online"` / `"offline"` (LWT) |
| `camera/<id>/config` | Server -> Camera | `{"timezone_offset":7200}` (retained) |

## Timezone Configuration

The device defaults to UTC but the timezone can be changed at runtime via a
**retained** MQTT message on the config topic. The device reads this message on
every connect/reconnect -- no reboot required after publishing.

**Set timezone** (example: UTC+2, i.e. 7200 seconds):

```bash
# Use printf + stdin piping to avoid shell escaping issues (especially over SSH)
printf '{"timezone_offset":7200}' | mosquitto_pub -h 192.168.2.143 -t camera/amb82_cam_01/config -r -l
```

**Reset to UTC:**

```bash
printf '{"timezone_offset":0}' | mosquitto_pub -h 192.168.2.143 -t camera/amb82_cam_01/config -r -l
```

**Via SSH to the test system:**

```bash
printf '{"timezone_offset":7200}' | ssh test-system 'mosquitto_pub -h 127.0.0.1 -t camera/amb82_cam_01/config -r -l'
```

**Common offsets:**

| Timezone | Offset (seconds) |
|----------|----------------:|
| UTC      | 0 |
| UTC+1 (CET) | 3600 |
| UTC+2 (EET/CEST) | 7200 |
| UTC+3 (EEST/MSK) | 10800 |
| UTC-5 (EST) | -18000 |
| UTC-8 (PST) | -28800 |

**How it works:** The device reads the retained config message using a disposable
MQTT connection at boot and once per minute during idle. A separate
`WiFiClient`/`PubSubClient` pair is used so the main MQTT publish client is never
affected. The parsed `timezone_offset` is applied to the NTPClient via
`setTimeOffset()`, adjusting `getEpochTime()` and all timestamps in MQTT payloads
(motion events, status reports, battery alerts). The firmware boots in UTC
(offset 0) and stays in UTC until the first retained config message is received.

**Server-side timezone:** Clip filenames and `start_time`/`end_time` in clip
metadata are generated by `recorder.py` using the server's system clock. Set the
server timezone to match:

```bash
sudo timedatectl set-timezone Europe/Sofia   # adjust to your timezone
```

For Docker deployments, set the `TZ` environment variable in `docker-compose.yml`.

## MQTT Security

The system supports three security levels, controlled by `secrets.h` +
`config.h` on the device and environment variables on the recorder. All levels
are backward-compatible -- the broker runs both listeners simultaneously.

| Level | Device Config | Port | Encryption | Authentication |
|-------|--------------|------|------------|----------------|
| 0 -- Anonymous | `MQTT_USER=""` (secrets.h), `MQTT_USE_TLS 0` (config.h) | 1883 | None | None |
| 1 -- Password | `MQTT_USER="x"` (secrets.h), `MQTT_USE_TLS 0` (config.h) | 1883 | None | Username/password |
| 2 -- mTLS + Password | `MQTT_USER="x"` (secrets.h), `MQTT_USE_TLS 1` (config.h) | 8883 | TLS | Client cert + password |

### Switching security levels on the device

`MQTT_USER` and `MQTT_PASSWORD` live in `secrets.h` (gitignored). `MQTT_USE_TLS`
lives in `config.h`.

In `secrets.h`:

```c
// Level 0 (anonymous):
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""

// Level 1 or 2 (authenticated):
#define MQTT_USER       "amb82_cam_01"
#define MQTT_PASSWORD   "your-password"
```

In `config.h`:

```c
#define MQTT_USE_TLS    0   // Level 0 or 1 -- plain on port 1883
// or
#define MQTT_USE_TLS    1   // Level 2 -- TLS + mTLS on port 8883
```

For Level 2, also populate `mqtt_certs.h` (see above). Recompile and flash
after changing.

### Broker setup (Mosquitto on Linux)

> **Shortcut:** `server/certs/generate-all.sh` automates Steps 1-2 (CA, server
> cert, per-client certs, Mosquitto password file) and
> `server/certs/install-broker.sh` automates Steps 3-4 (deploy to
> `/etc/mosquitto/`, write `secure.conf`, restart the broker). See
> `server/certs/README.md`. The manual steps below are the equivalent
> walk-through if you prefer to do it by hand or want to understand what the
> scripts do.

#### Step 1 -- Generate certificates (self-signed CA, 10-year validity)

```bash
mkdir -p ~/mqtt_certs && cd ~/mqtt_certs

# CA key + self-signed cert WITH proper CA:TRUE extension.
# (Note: plain `-extensions v3_ca` does NOT set CA:TRUE without a config file --
# mbedTLS on the ESP/AMB82 rejects CAs without basicConstraints=CA:TRUE.)
openssl genrsa -out ca.key 2048
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
  -subj "/CN=AMB82 MQTT CA" \
  -extensions v3_ca -config <(cat /etc/ssl/openssl.cnf - << 'EOF'
[v3_ca]
basicConstraints = critical, CA:TRUE
keyUsage = critical, keyCertSign, cRLSign
subjectKeyIdentifier = hash
EOF
)

# Server cert with Subject Alternative Names so clients can connect by
# hostname OR IP (mbedTLS on AMB82 validates hostname against SAN).
cat > server_ext.cnf << 'EOF'
subjectAltName = DNS:sbbu01.local, DNS:localhost, IP:192.168.2.143, IP:127.0.0.1
extendedKeyUsage = serverAuth
EOF

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -subj "/CN=sbbu01.local"
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server.crt -days 3650 -extfile server_ext.cnf

# Client certificate for camera
openssl genrsa -out client_camera.key 2048
openssl req -new -key client_camera.key -out client_camera.csr \
  -subj "/CN=amb82_cam_01"
openssl x509 -req -in client_camera.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out client_camera.crt -days 3650

# Client certificate for recorder
openssl genrsa -out client_recorder.key 2048
openssl req -new -key client_recorder.key -out client_recorder.csr \
  -subj "/CN=recorder"
openssl x509 -req -in client_recorder.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out client_recorder.crt -days 3650
```

#### Step 2 -- Create MQTT password file

```bash
touch ~/mqtt_certs/passwd
mosquitto_passwd -b ~/mqtt_certs/passwd amb82_cam_01 "your-camera-password"
mosquitto_passwd -b ~/mqtt_certs/passwd recorder "your-recorder-password"
chmod 600 ~/mqtt_certs/passwd
```

#### Step 3 -- Install into Mosquitto

```bash
sudo mkdir -p /etc/mosquitto/certs
sudo cp ~/mqtt_certs/{ca.crt,server.crt,server.key,passwd} /etc/mosquitto/certs/
sudo chown -R mosquitto:mosquitto /etc/mosquitto/certs
sudo chmod 600 /etc/mosquitto/certs/server.key /etc/mosquitto/certs/passwd
```

#### Step 4 -- Configure Mosquitto

Replace `/etc/mosquitto/conf.d/local.conf` with:

```
per_listener_settings true

# Plain listener -- backward-compatible anonymous access
listener 1883 0.0.0.0
allow_anonymous true

# TLS listener -- encrypted + mTLS + password auth
listener 8883 0.0.0.0
allow_anonymous false
password_file /etc/mosquitto/certs/passwd
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate true
use_identity_as_username false
```

```bash
sudo systemctl restart mosquitto
```

#### Step 5 -- Verify

```bash
# Anonymous on 1883 (should work)
mosquitto_pub -h 127.0.0.1 -p 1883 -t test -m "hello"

# TLS + mTLS + auth on 8883
mosquitto_pub -h 127.0.0.1 -p 8883 \
  --cafile ~/mqtt_certs/ca.crt \
  --cert ~/mqtt_certs/client_camera.crt \
  --key ~/mqtt_certs/client_camera.key \
  -u amb82_cam_01 -P "your-camera-password" \
  -t test -m "hello secure"
```

### Updating device certificates

The CA and client certificates are embedded in `mqtt_certs.h` as PEM strings.
To update them (e.g., after regenerating certs), copy the PEM content into
the `mqtt_ca_cert`, `mqtt_client_cert`, and `mqtt_client_key` constants,
recompile, and flash.

### Updating recorder certificates

Recorder certs live at `~/amb82_recorder/certs/` on the server. The systemd
service environment variables point to them. After replacing cert files,
restart the recorder:

```bash
systemctl --user restart amb82-recorder
```

## Firmware Modules

| File | Purpose |
|------|---------|
| `amb82_rec.ino` | Main: state machine (IDLE/STREAMING/POST_ROLL), RTSP control |
| `config.h` | All configuration constants |
| `wifi_manager.h/cpp` | WiFi connect/reconnect |
| `mqtt_manager.h/cpp` | MQTT publish (motion events with RTSP URL, status, battery) |
| `battery_monitor.h/cpp` | ADC-based battery voltage/percentage with alert edge detection |

## Test Sketches

Standalone test sketches in `tests/`, one per module. Open in Arduino IDE, set WiFi credentials where needed, flash, and watch Serial Monitor at 115200 baud.

**WiFi credentials** -- test sketches that need WiFi (`test_wifi`, `test_ntp`, `test_rtsp`, `test_mqtt`) include a shared header `wifi_def.h` rather than hardcoding SSID/password in each sketch. It lives as a user library at:

```
C:\Work\Arduino\libraries\wifi_def\wifi_def.h
```

with contents:

```c
#ifndef WIFI_DEF_H
#define WIFI_DEF_H

#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

#endif
```

Edit that file once and every test sketch picks up the new credentials. The file is intentionally **not** checked into this repo -- keep real credentials out of version control.

| # | Sketch | What it tests | Needs |
|---|--------|---------------|-------|
| 1 | `test_serial` | Serial, printf, LOG/LOGF macros, LED | Board only |
| 2 | `test_battery` | ADC read, voltage calc, % mapping | Board only |
| 3 | `test_wifi` | Connect, IP, RSSI, disconnect/reconnect | WiFi network |
| 4 | `test_ntp` | Time sync, epoch, formatted date/time | WiFi + internet |
| 5 | `test_camera` | Ch0 H264 + Ch3 RGB, channel start/stop | Board only |
| 6 | `test_motion` | MotionDetection on Ch3 RGB, getResultCount() | Board only (set correct Sensor Selection in IDE) |
| 7 | `test_rtsp` | RTSP start/stop, verify with VLC | WiFi network |
| 8 | `test_mqtt` | Connect, publish motion event with RTSP URL | MQTT broker |

### MQTT broker for `test_mqtt`

The `test_mqtt` sketch needs a reachable MQTT broker. For bench testing you can
point it at the full `server/` Docker stack, at `test.mosquitto.org:1883`, or at
a bare Mosquitto install on any LAN machine.

**Bare Mosquitto on Debian/Ubuntu (ARM64 or x86_64):**

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients

sudo tee /etc/mosquitto/conf.d/local.conf >/dev/null <<'EOF'
listener 1883 0.0.0.0
allow_anonymous true
EOF

sudo systemctl enable --now mosquitto
sudo systemctl restart mosquitto

# Verify listener is on the LAN interface (not just localhost)
ss -tln | grep 1883
```

If `ufw` is active, also open the port:

```bash
sudo ufw allow 1883/tcp
```

**Bare Mosquitto on Windows 11:**
1. Install from <https://mosquitto.org/download/>
2. Edit `C:\Program Files\mosquitto\mosquitto.conf` (as Administrator) and append:

   ```
   listener 1883 0.0.0.0
   allow_anonymous true
   ```

3. Start the service and open the firewall (Admin PowerShell):

   ```powershell
   net start mosquitto
   New-NetFirewallRule -DisplayName "Mosquitto MQTT" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow
   ```

**Point the sketch at your broker** -- in `tests/test_mqtt/test_mqtt.ino`:

```c
#define MQTT_BROKER "192.168.x.y"   // Your broker LAN IP
#define MQTT_PORT   1883
#define MQTT_USER   ""              // anonymous
#define MQTT_PASS   ""
```

**Verify the roundtrip from any LAN machine with `mosquitto-clients`:**

```bash
# Terminal A -- watch everything the firmware publishes
mosquitto_sub -h 192.168.x.y -t 'amb82_test/#' -v

# Terminal B -- inject a command to trigger the sketch's subscribe callback
mosquitto_pub -h 192.168.x.y -t amb82_test/cmd -m 'hello'
```

> `allow_anonymous true` is intended for LAN bench testing only. For the
> production recorder pipeline under `server/`, use a `password_file` and set
> `allow_anonymous false`.
