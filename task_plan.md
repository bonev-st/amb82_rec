# Task Plan: Bench Deployment of amb82_rec

## Goal
Stand up the test infrastructure on `test-system` (Debian aarch64), build and
flash `amb82_rec.ino` to the AMB82 Mini from this Windows machine via Arduino
IDE, and verify the end-to-end motion → RTSP → MQTT → ffmpeg recording loop.

USB-powered bench test. No battery, no Home Assistant, no Docker stack — the
existing native Mosquitto on `test-system` is reused.

## Current Phase
Phase 7 — Complete

## Configuration (from user, 2026-04-11)
| Item | Value |
|------|-------|
| MQTT broker | Native Mosquitto on `test-system` (`sbbu01.local:1883`, anonymous) |
| Recorder host | `test-system` directly (system Python venv + apt ffmpeg) |
| Home Assistant | Skipped |
| Wi-Fi SSID | *(provided out-of-band; substitute in `config.h`)* |
| Wi-Fi password | *(provided out-of-band; substitute in `config.h`)* |
| Clip dir | `~/amb82_clips/` on `test-system` |
| Device ID | `amb82_cam_01` (default) |
| Battery monitor | Left as-is, USB-powered, ADC reading ignored |
| Flash toolchain | Arduino IDE on Windows (Ameba SDK 4.1.0 already installed) |

## Phases

### Phase 1 — Pre-flight & decisions
- [x] Inspect firmware, modules, server stack
- [x] Resolve open questions with user
- [x] Confirm `test-system` reachable (`ssh test-system uptime`) — up 1 day 6h
- [x] Confirm board enumerates over USB on Windows — flashed twice in Phase 4
- **Status:** complete

### Phase 2 — Test-system infrastructure
- [x] Verify Mosquitto is running and listening on `0.0.0.0:1883`
- [x] Verify anonymous publish/subscribe works from the host itself
- [x] Install runtime deps: `ffmpeg` (already present), `python3.13-venv` (user installed)
- [x] Create `~/amb82_clips/` directory
- [x] Copy `server/recorder/recorder.py` to `~/amb82_recorder/recorder.py`
- [x] Create venv `~/amb82_recorder/.venv`, install `paho-mqtt==2.1.0`
- [x] Create systemd **user** unit `~/.config/systemd/user/amb82-recorder.service`,
      `loginctl enable-linger`, enable + start
- [x] Confirm `Connected to MQTT broker` + `Subscribed to camera/+/motion` in journal
- **Status:** complete

### Phase 3 — Firmware configuration
- [x] `git rm --cached config.h` + add `config.h` to `.gitignore` (now untracked)
- [x] Edit `config.h`: WIFI_SSID/PASSWORD set, MQTT_BROKER="sbbu01.local",
      MQTT_USER/PASSWORD="" (anonymous), defaults left for everything else
- [x] Sanity-check: board's Wi-Fi network reaches broker at `192.168.2.143` (mDNS failed, IPv4 used instead)
- **Status:** complete

### Phase 4 — Build & flash from Arduino IDE
- [x] Compiled and flashed via Arduino IDE (twice — first with `sbbu01.local`,
      then with `192.168.2.143` after mDNS lookup failed on lwIP)
- **Status:** complete

### Phase 5 — Bring-up verification
- [x] Serial: WiFi (192.168.2.186, RSSI -38 dBm), NTP 11:36:58, MQTT Connected,
      8s warm-up, "Setup Complete"
- [x] `mosquitto_sub` on test-system shows `camera/amb82_cam_01/availability online`
      (retained) — and an offline/online flap from the keepalive timeout during
      `video_init`, which is cosmetic
- [x] `ffprobe rtsp://192.168.2.186:554` — confirmed via 108 recorded clips, all valid H264 1080p
- **Status:** complete

### Phase 6 — End-to-end recording test
- [x] Wave hand in front of the camera — 108 clips recorded across 2026-04-11 and 2026-04-12
- [x] Serial: `IDLE -> STREAMING`, MQTT publishes `motion=true` with rtsp URL — confirmed in recorder journal
- [x] Recorder log shows `Starting recording` with correct path (e.g. `/home/arduino/amb82_clips/amb82_cam_01/2026-04-12/amb82_cam_01_13-48-35.mp4`)
- [x] After ~10 s of stillness: `motion=false` published, ffmpeg terminates, clip metadata published on `camera/amb82_cam_01/clip` — confirmed in journal
- [x] Inspect the resulting `.mp4` — ffprobe: H264 High profile, 1920x1080, 30fps, yuv420p, valid container
- **Status:** complete

### Phase 7 — Teardown / handoff notes
- [x] Document any deltas vs. plan in `findings.md` — mDNS→IPv4 fallback and MQTT keepalive flap documented in Errors table above
- [x] Decide whether the recorder unit should stay running or be stopped — kept running (26+ hours, actively recording)
- **Status:** complete

## Key Questions (resolved)
1. Native Mosquitto vs Docker stack? → **Native**
2. Recorder in Docker or native? → **Native venv**
3. Home Assistant? → **Skip**
4. Battery wiring? → **Skip, USB-powered**
5. Flash from Windows or arduino-cli? → **Arduino IDE on Windows**

## Risks & Watch-outs
- `config.h` is git-tracked — secrets will leak if committed. Untrack first.
- The bundled `server/mosquitto/mosquitto.conf` requires a password file and
  is **not** what's running on `test-system`. We are deliberately bypassing
  that file for this bench test.
- `MQTT_BROKER "sbbu01.local"` relies on mDNS — if the AMB82's lwIP stack
  doesn't resolve `.local`, fall back to the LAN IPv4 of `test-system`.
- The user-installed PubSubClient at `~/Documents/Arduino/libraries/PubSubClient`
  has `MQTT_MAX_PACKET_SIZE=256`, which can truncate the status JSON. The
  `mqtt_manager.h` include `"src/PubSubClient.h"` is intentional — do not
  "fix" it.
- The detection-channel warm-up `delay(8000)` in `setup()` is required;
  without it the MD background model produces false positives at boot.

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| `MQTT rc=-2` at boot | `sbbu01.local` via mDNS | Swapped `MQTT_BROKER` to `192.168.2.143` (IPv4 literal). Fixed. |
| 90 s MQTT flap (offline/online cycle) | Observed with `mosquitto_sub -F '%I %t %p'` — gap is 89–91 s, matches `1.5 × MQTT_KEEPALIVE` (60 s default) | Board's PubSubClient not emitting PINGREQ inside the keepalive window. Not blocking Phase 6 (~85 s of every 90 s the connection is live). **TODO after Phase 6:** call `_client.setKeepAlive(30)` in `MqttManager::begin`, or force a dummy publish every 20 s as a heartbeat. |
