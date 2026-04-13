# AMB82 Mini Motion-Triggered RTSP Streamer

Motion-triggered RTSP video streamer for the Ameba AMB82 Mini camera board. Detects motion via the on-board camera pipeline, starts an RTSP stream on demand, and notifies a Linux server via MQTT to begin recording. No local SD card storage required.

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
|      (on demand)      |  rtsp://:554  |   kills FFmpeg on stop    |
|                       |               |   saves MP4 clips         |
| MQTT client           |               |                           |
| Battery monitor       |               | Home Assistant            |
+-----------------------+               |   notifications/UI        |
                                        +---------------------------+
```

## Hardware

- **Board:** Ameba AMB82 Mini (Realtek RTL8735B, ARM Cortex-M33)
- **Power:** Battery (LiPo) or USB — supports both modes
- **No SD card needed** — video is stored on the server

## Build & Flash

1. Install Arduino IDE
2. Add Ameba board package URL: `https://github.com/ambiot/ambpro2_arduino`
3. Select board **AMB82 Mini**
4. Edit `config.h` — set WiFi credentials, MQTT broker IP
5. Select build mode (see below)
6. Remove any user-installed PubSubClient from Arduino libraries (use SDK version)
7. Compile and upload via USB

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

- **DEBUG** — bench testing, serial monitor attached, diagnosing motion detection
  sensitivity, verifying MQTT payloads
- **RELEASE** — deployed camera running on battery, no serial monitor, maximum
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
- **Mosquitto** (port 1883) — MQTT broker
- **recorder** — MQTT-triggered FFmpeg recorder, saves clips to `server/clips/`

### How Recording Works

1. Camera detects motion locally (Ch3, low-res RGB)
2. Camera starts RTSP stream (Ch0, 1080p H264)
3. Camera publishes MQTT: `{"motion":true, "rtsp":"rtsp://192.168.1.x:554"}`
4. `recorder.py` receives MQTT, spawns FFmpeg to record from RTSP URL
5. Motion ends, camera publishes `{"motion":false}`
6. `recorder.py` sends SIGTERM to FFmpeg (MP4 finalized cleanly)
7. Clip saved to `server/clips/<device_id>/<date>/<timestamp>.mp4`
8. Clip metadata published back to MQTT for Home Assistant

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
every connect/reconnect — no reboot required after publishing.

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
(motion events, status reports, battery alerts). The compile-time default in
`config.h` (`NTP_TIMEZONE_OFFSET`) is used only until the first retained config
message is received.

**Server-side timezone:** Clip filenames and `start_time`/`end_time` in clip
metadata are generated by `recorder.py` using the server's system clock. Set the
server timezone to match:
```bash
sudo timedatectl set-timezone Europe/Sofia   # adjust to your timezone
```
For Docker deployments, set the `TZ` environment variable in `docker-compose.yml`.

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

**WiFi credentials** — test sketches that need WiFi (`test_wifi`, `test_ntp`, `test_rtsp`, `test_mqtt`) include a shared header `wifi_def.h` rather than hardcoding SSID/password in each sketch. It lives as a user library at:

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

Edit that file once and every test sketch picks up the new credentials. The file is intentionally **not** checked into this repo — keep real credentials out of version control.

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
1. Install from https://mosquitto.org/download/
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

**Point the sketch at your broker** — in `tests/test_mqtt/test_mqtt.ino`:
```c
#define MQTT_BROKER "192.168.x.y"   // Your broker LAN IP
#define MQTT_PORT   1883
#define MQTT_USER   ""              // anonymous
#define MQTT_PASS   ""
```

**Verify the roundtrip from any LAN machine with `mosquitto-clients`:**
```bash
# Terminal A — watch everything the firmware publishes
mosquitto_sub -h 192.168.x.y -t 'amb82_test/#' -v

# Terminal B — inject a command to trigger the sketch's subscribe callback
mosquitto_pub -h 192.168.x.y -t amb82_test/cmd -m 'hello'
```

> `allow_anonymous true` is intended for LAN bench testing only. For the
> production recorder pipeline under `server/`, use a `password_file` and set
> `allow_anonymous false`.
