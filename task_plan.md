# Task Plan: Bench Deployment of amb82_rec

## Goal
Stand up the test infrastructure on `test-system` (Debian aarch64), build and
flash `amb82_rec.ino` to the AMB82 Mini from this Windows machine via Arduino
IDE, and verify the end-to-end motion → RTSP → MQTT → ffmpeg recording loop.

USB-powered bench test. No battery, no Home Assistant, no Docker stack — the
existing native Mosquitto on `test-system` is reused.

## Current Phase
Phase 6 — End-to-end recording test (in progress)

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
- [ ] Confirm `test-system` reachable (`ssh test-system uptime`)
- [ ] Confirm board enumerates over USB on Windows
- **Status:** in progress

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
- [ ] Sanity-check: board's Wi-Fi network must reach `sbbu01.local` (verified at flash time)
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
- [ ] `ffprobe rtsp://192.168.2.186:554` — deferred to Phase 6 (channel only
      starts when motion fires)
- **Status:** complete (with caveats logged)

### Phase 6 — End-to-end recording test
- [ ] Wave hand in front of the camera
- [ ] Serial: `IDLE -> STREAMING`, MQTT publishes `motion=true` with rtsp URL
- [ ] Recorder log shows `Starting recording … -> /home/<user>/amb82_clips/amb82_cam_01/<date>/<time>.mp4`
- [ ] After ~10 s of stillness: `STREAMING -> POST_ROLL -> IDLE`,
      `motion=false` published, ffmpeg terminates, clip metadata published on
      `camera/amb82_cam_01/clip`
- [ ] Inspect the resulting `.mp4` (`ffprobe`, plays in VLC)
- **Status:** pending

### Phase 7 — Teardown / handoff notes
- [ ] Document any deltas vs. plan in `findings.md`
- [ ] Decide whether the recorder unit should stay running or be stopped
- **Status:** pending

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
