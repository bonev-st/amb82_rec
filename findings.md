# Findings & Decisions

## 2026-04-11 — Bench deployment analysis (amb82_rec.ino)

### Runtime dependencies inferred from `amb82_rec.ino`
- **WiFi** (`WiFi.h`, `WiFiUdp.h`) → board needs SSID/password reaching the test LAN
- **NTP** (`NTPClient.h`) → outbound UDP to `pool.ntp.org`; `forceUpdate()` is
  called once in `setup()` because `update()` is a no-op before the interval
- **Camera pipeline** → Channel 0 (FHD H264 @ 30fps, 2 Mbps) configured but
  *not started* until motion fires; Channel 3 (VGA RGB @ 10fps) always-on for
  motion detection. Both channels must be `configVideoChannel`'d before
  `Camera.videoInit()`, otherwise the sensor won't initialise.
- **RTSP** server bound on `:554`, started/stopped via `rtsp.begin()`/`end()`
  (the SDK maps these to `RTSPSetStreaming(1/0)` so toggling is cheap).
- **MotionDetection** on channel index `3` (NOT `V3_CHANNEL`=2), 8 s warm-up
  needed for AE + background model. `getResultCount() >= 3` is the trigger.
- **MQTT** via `PubSubClient` — uses the SDK-patched copy under `src/` (512 B
  max packet) instead of the user-installed stock copy (256 B). Topics:
  `camera/<id>/status|battery|motion|availability`. LWT is configured.
- **Battery monitor** reads `A0`; harmless when USB-powered (will report
  garbage voltage but no functional impact).

### Server-side dependencies (`server/recorder/recorder.py`)
- Python 3, `paho-mqtt==2.1.0`
- `ffmpeg` on `$PATH` (used with `-rtsp_transport tcp -c copy` — passthrough,
  no re-encode, very low CPU)
- Subscribes to `camera/+/motion`; payload schema is `{"motion": bool, "rtsp": "..."}`
- Writes clips to `$CLIPS_DIR/<device_id>/<YYYY-MM-DD>/<HH-MM-SS>.mp4`
- Publishes `camera/<id>/clip` metadata when ffmpeg exits

### Test-system already has
- Mosquitto 2.0.21, listener `0.0.0.0:1883`, anonymous, no firewall
  (per CLAUDE.md). This satisfies the broker requirement directly — the
  bundled `server/docker-compose.yml` is **not** used in this bench test.

### Decisions for this bench test
| Decision | Rationale |
|----------|-----------|
| Skip Docker stack | Existing native broker works; fewer moving parts |
| Run recorder.py natively in a venv | Same reason; matches "test-system directly" choice |
| Anonymous MQTT | Matches existing broker config; LAN-only |
| `MQTT_BROKER = sbbu01.local` | Resolves via mDNS; fall back to IPv4 if needed |
| Untrack `config.h` before editing | It currently contains placeholders, but real creds must not be committed |
| Leave battery code enabled | Code path is harmless on USB; disabling is extra churn |

## Requirements
- Platform: Ameba AMB82 Mini (RTL8735B, ARM Cortex-M33)
- Toolchain: Arduino IDE with Ameba Arduino SDK (ambpro2_arduino)
- Battery-powered motion-triggered video recorder
- Motion detection → start recording → 10s post-roll → finalize clip
- Upload clip to server (MinIO S3-compatible)
- MQTT for status/metadata (battery, motion events, clip info)
- Low battery + new video notifications (edge-triggered, no spam)
- WiFi reconnect, upload retry, no clip loss
- Power optimization is critical

## Research Findings

### AMB82 Mini SDK APIs

#### Motion Detection
- Class: `MotionDetection` via `#include "MotionDetection.h"`
- Two modes: `CallbackPostProcessing` (callback on motion) and `LoopPostProcessing` (poll in loop)
- Works by comparing RGB information between video frames from JX-F37P sensor
- Detection mask array: 1 = region enabled, 0 = disabled
- Official examples: MotionDetection/LoopPostProcessing, MotionDetection/CallbackPostProcessing, MotionDetection/MaskingMP4Recording

#### MP4 Recording
- Class: `RecordMP4` (or `MP4Recording`)
- Modes: AudioOnly, VideoOnly, SingleVideoWithAudio, DoubleVideoWithAudio
- Records from onboard camera sensor (JXF37) + audio codec
- Saves MP4 files to SD card
- Supports H.264 and H.265 video encoding
- Official example: RecordMP4/SingleVideoWithAudio

#### Video Pipeline
- `VideoSetting` class configures resolution, FPS, codec, channel
- Example: `VideoSetting configV1(VIDEO_FHD, CAM_FPS, VIDEO_H264, 0);`
- `Camera` class manages video streams
- `StreamIO` connects pipeline stages (camera → encoder → recorder/motion detection)
- Multiple concurrent streams supported (dual channel)
- JXF37 sensor: up to 1920×1080
- Formats: VIDEO_FHD, VIDEO_HD, VIDEO_VGA; Codecs: VIDEO_H264, VIDEO_H265, VIDEO_JPEG

#### WiFi
- `WiFiClient` class with connect/read/write/stop methods
- Security: OPEN, WPA, WEP, WPA2-EAP
- `setRecvTimeout()`, blocking/non-blocking modes
- Example: WiFi/ConnectToWiFi, WiFi/SimpleHttpRequest

#### HTTP Upload
- Built on WiFiClient — construct HTTP POST/PUT manually
- Official example: RecordMP4/HTTP_Post_MP4_Httpbin
- Can upload MP4 files from SD card to server via HTTP POST
- For MinIO: use HTTP PUT with S3v4 signing or presigned URLs

#### MQTT
- Built-in `AmebaMQTTClient` library
- TLS support
- Publish/subscribe functionality
- Example: AmebaMQTTClient/MQTT_TLS
- Alternative: PubSubClient (third-party, widely used)

#### Power Saving / Deep Sleep
- Three modes: Sleep, Snooze, Deep Sleep
- Deep Sleep wake sources: AON Timer, AON GPIO, RTC
- Deep Sleep = ultra-low power but full reboot on wake
- **Critical finding:** Deep sleep is NOT compatible with continuous camera-based motion detection — the camera pipeline must be running to detect motion
- Best approach: keep board active with lowest practical camera settings for detection

#### SD Card / File System
- `File` class with open/read/write/seek/close
- `AmebaFatFS` for SD card access
- MP4 files saved directly to SD card by RecordMP4
- Example: AmebaFileSystem

#### NTP Time Sync
- `NTPClient` library
- Constructor: `NTPClient(WiFiUDP, server, timeOffset, updateInterval)`
- Retrieves UTC time, applies timezone offset
- Example: NTPClient/Advanced

#### ADC / Battery Reading
- ADC available on GPIO pins
- Power supply: 3.3V–5V
- Use voltage divider circuit to read battery voltage
- `analogRead(pin)` standard Arduino API expected

### Server Architecture Comparison

| Option | Pros | Cons | Verdict |
|--------|------|------|---------|
| Home Assistant + MQTT + MinIO | Local, extensible, low cost, rich automation | More setup | **Best fit** |
| NAS/shared folder | Simple | No automation, limited notifications | Too basic |
| Cloud storage | Managed | Ongoing cost, privacy, latency | Overkill |
| NVR (Frigate/ZoneMinder) | Designed for cameras | Expects RTSP streams, not clips | Wrong paradigm |

### Power Strategy Analysis
- Camera-based motion detection requires active board + camera sensor → no deep sleep while watching
- Best compromise: low-res, low-FPS detection channel (e.g., 640×480 @ 5fps) + high-res recording channel (1080p @ 15-20fps)
- Battery measurement: every 60 seconds (low overhead)
- Status reporting: every 5 minutes via MQTT
- **Cannot optimize further:** the fundamental requirement for motion-triggered video means the camera and MCU must stay powered

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Dual video channel (low-res detect + high-res record) | Minimize power while idle, maximize quality during events |
| SD card intermediate storage | Decouple recording from upload; survive network outages |
| HTTP PUT to MinIO (presigned URL or basic auth) | Simple S3-compatible upload; avoid complex AWS signing |
| MQTT for metadata only (not video) | Lightweight; video goes direct to object storage |
| Edge-triggered battery alerts | Notify once per threshold crossing, not every measurement |
| NTP sync on boot only | Minimize network traffic; RTC drift acceptable for clip naming |
| 10s post-roll timer with reset on new motion | Per requirements; seamless recording extension |

## Resources
- AMB82 Mini SDK repo: https://github.com/Ameba-AIoT/ameba-arduino-pro2
- API docs: https://ameba-doc-arduino-sdk.readthedocs-hosted.com/en/latest/ameba_pro2/amb82-mini/
- Forum: https://forum.amebaiot.com/
- Key examples: MotionDetection/MaskingMP4Recording, RecordMP4/SingleVideoWithAudio, RecordMP4/HTTP_Post_MP4_Httpbin
