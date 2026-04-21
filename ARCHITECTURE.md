# System Architecture -- AMB82 Motion Camera

## 1. Feasibility Assessment

The AMB82 Mini (RTL8735B) is well-suited for this project:

- **Motion detection**: Built-in `MotionDetection` class in the SDK compares RGB frames from the JXF37 sensor -- no external PIR needed
- **RTSP streaming**: The SDK's `RTSP` class exposes an H.264 stream the server can pull on demand -- no on-device MP4 finalisation needed
- **Dual video channels**: The SDK supports multiple simultaneous video channels, enabling low-res RGB detection (Ch3) alongside the high-res H.264 RTSP channel (Ch0)
- **WiFi + MQTT**: `WiFiClient` / `WiFiSSLClient` with the SDK-patched `PubSubClient` handle event signalling and optional TLS/mTLS
- **Limitation**: Camera-based motion detection requires the video pipeline to stay active -- deep sleep is not compatible with continuous motion watching

## 2. Server Architecture

**Stack: Mosquitto + recorder.py (FFmpeg) + Home Assistant**

| Component            | Role                                                                                                                           |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| **Mosquitto (MQTT)** | Device status, battery level, motion events, clip metadata                                                                     |
| **recorder.py**      | Subscribes to MQTT motion events and spawns FFmpeg to pull the device's RTSP stream; writes MP4 clips to the server filesystem |
| **Home Assistant**   | Automation hub -- notifications, dashboards, clip links                                                                        |
| **n8n / Node-RED**   | Optional workflow automation (e.g., video post-processing)                                                                     |

**Why this is the best tradeoff:**

- **Simplicity** -- all components run as Docker containers on a single machine
- **Local control** -- no cloud dependency, no subscriptions, privacy-preserving
- **Low cost** -- free/open-source software on any old PC or Raspberry Pi 4+
- **Maintainability** -- standard tools with large communities
- **Extensibility** -- easy to add more cameras, object detection, or integrations

**Rejected alternatives:**

- On-device MP4 recording to SD + upload to object storage -- dynamically starting/stopping the H.264 encoder for per-event clips produced decodable-but-static frames in testing; leaving the encoder up and recording server-side is reliable and removes an on-device storage/wear component
- NAS/shared folder -- too basic, no automation or notifications
- Cloud storage -- ongoing cost, latency, privacy concerns
- NVR (Frigate/ZoneMinder) -- designed for always-on server-side motion on full RTSP; we run motion on the device so the network only carries the stream during real events

## 3. Power Optimization Strategy

### Realistic Power Strategy

| Mode                | Camera                                                        | MCU    | WiFi      | Notes                                                                   |
| ------------------- | ------------------------------------------------------------- | ------ | --------- | ----------------------------------------------------------------------- |
| **Idle (watching)** | Ch3 RGB 640x480 @ 5-10 fps; Ch0 H.264 1080p encoder always on | Active | Connected | RTSP packets only leave the device when the server is actively pulling  |
| **Motion active**   | Same channel configuration                                    | Active | Active    | Server pulls RTSP; MQTT keeps the broker informed through state changes |

### Best Practical Compromise

- Detection channel (Ch3): 640x480 RGB @ 5-10 fps -- minimal processing, feeds `MotionDetection` only
- Streaming channel (Ch0): 1920x1080 H.264 @ 30 fps -- encoder stays up; the RTSP server only sends packets while a client is connected
- Main-loop idle delay and battery/config poll intervals are stretched in RELEASE builds to save cycles
- Battery measured every 60-120 s; status reported every hour
- Watchdog in RELEASE reboots on hangs so long-running deployments self-heal

### What Cannot Be Optimized Further

The fundamental requirement for camera-based motion-triggered video means:

1. The camera sensor must remain powered and capturing frames for motion detection
2. The MCU must remain active to process motion detection results
3. The Ch0 H.264 encoder must stay up -- dynamic restart produced static/stuck frames during testing
4. WiFi must stay connected for timely MQTT notifications and server-pull RTSP
5. **Deep sleep is not an option** while watching for motion -- the entire video pipeline resets on wake

A PIR sensor could enable deep sleep between events, but at the cost of losing video-based motion detection quality and requiring additional hardware.

## 4. Firmware Architecture

```
+--------------------------------------------------------------+
|                     AMB82 Mini Firmware                      |
+--------------------------------------------------------------+
|                                                              |
|  +-----------+   +---------------+   +---------------------+ |
|  | Camera    |-->|MotionDetection|-->| State Machine       | |
|  | Ch3 (RGB) |   | (low-res)     |   | IDLE ->             | |
|  +-----------+   +---------------+   | MOTION_ACTIVE ->    | |
|                                      | POST_ROLL -> IDLE   | |
|                                      +----------+----------+ |
|                                                 |            |
|                                                 v            |
|  +-----------+   +---------------+   +---------------------+ |
|  | Camera    |-->| RTSP Server   |<--+ motion events via   | |
|  | Ch0 (H264)|   | (always on;   |   | MQTT tell server    | |
|  +-----------+   |  server pulls)|   | when to pull        | |
|                  +-------+-------+   +---------------------+ |
|                          |                                   |
|                          | rtsp://<ip>:554  (pulled by       |
|                          v                   recorder.py on  |
|                                              the LAN)        |
|                                                              |
|  +----------+    +---------------+   +------------------+    |
|  | Battery  |--->| MqttManager   |-->| Mosquitto (MQTT) |    |
|  | Monitor  |    | (status,      |   +------------------+    |
|  +----------+    |  motion,      |                           |
|                  |  battery)     |                           |
|                  +---------------+                           |
|                                                              |
|  +----------+    +---------------+   +----------------+      |
|  | WiFi     |    | NTP / RTC     |   | LedManager     |      |
|  | Manager  |    | (timestamps,  |   | green + blue   |      |
|  |          |    |  mbedTLS time)|   | indicators     |      |
|  +----------+    +---------------+   +----------------+      |
+--------------------------------------------------------------+
```

### Motion/Record State Machine

```
IDLE --(motion detected)--> MOTION_ACTIVE
                               |
                    (motion stops)
                               v
                          POST_ROLL --(post-roll elapsed)--> IDLE
                               |
                    (motion resumes)
                               |
                               v
                          MOTION_ACTIVE
```

Each transition publishes an MQTT motion event carrying the RTSP URL. The server starts FFmpeg on a MOTION_ACTIVE event and terminates it when POST_ROLL ends, producing the clip on the server filesystem.

## 5. File Structure

| File                     | Purpose                                                                         |
| ------------------------ | ------------------------------------------------------------------------------- |
| `amb82_rec.ino`          | Main firmware -- setup, loop, state machine, RTSP + motion glue                 |
| `config.h`               | Build-mode and non-secret configuration constants                               |
| `secrets.h` (gitignored) | WiFi + MQTT credentials; copied from `secrets.h.example`                        |
| `mqtt_certs.h`           | CA + client cert + client key PEMs for MQTT TLS (Level 2)                       |
| `wifi_manager.h/.cpp`    | WiFi connect, non-blocking reconnect, RSSI                                      |
| `mqtt_manager.h/.cpp`    | MQTT publish (status, motion, battery); disposable-client retained-config reads |
| `battery_monitor.h/.cpp` | ADC reading, percentage estimation, state-transition alerts                     |
| `led_manager.h/.cpp`     | Non-blocking LED driver for the green (record) and blue (health) indicators     |

## 6. Known Limitations & Assumptions

1. **Continuous H.264 encoder**: Ch0 stays encoding even when the server is not pulling; this is a deliberate power tradeoff because dynamic encoder restart produced static/stuck frames in testing
2. **Motion detection sensitivity**: `getResultCount()` thresholds and detection FPS may need tuning for your environment
3. **ADC calibration**: Battery voltage divider ratio and ADC reference voltage need calibration for your specific circuit
4. **No audio**: Current configuration is video-only; adding audio requires the SDK's `SingleVideoWithAudio` mode and microphone wiring
5. **Single camera**: Designed for one camera per device; multi-camera would need separate device IDs
6. **mbedTLS 2.28 hostname SAN**: When MQTT TLS is enabled, `MQTT_BROKER` must be an IPv4 literal -- the platform's mbedTLS matches DNS SAN entries against SNI strings but does not match IP SAN against an IP literal SNI. The firmware logs a fatal error at boot if a hostname is detected in TLS mode.

## 7. Setup & Test Procedure

### Firmware

1. Install Arduino IDE and add Ameba board package URL: `https://github.com/ambiot/ambpro2_arduino`
2. Select board "AMB82 Mini" in Arduino IDE
3. Copy `secrets.h.example` -> `secrets.h` and fill in WiFi SSID/password and MQTT broker IP (IPv4 literal when TLS is on), user, and password
4. If MQTT TLS is enabled (`MQTT_USE_TLS 1`, the default), copy `mqtt_certs.h.example` -> `mqtt_certs.h` and paste in the CA + client cert + client key PEMs produced by `server/certs/generate-all.sh`
5. Wire the battery voltage divider to A0 (or change `BATTERY_ADC_PIN`)
6. Select DEBUG or RELEASE build mode in `config.h`
7. Compile and upload via USB
8. In DEBUG, open Serial Monitor at 115200 baud to verify boot sequence; in RELEASE the on-board LEDs report WiFi/NTP/MQTT health directly (see README's LED indicator tables)

### Server

1. Install Docker and Docker Compose on your server
2. Run `docker compose up -d` from the `server/` directory -- this starts Mosquitto and `recorder.py`
3. For MQTT TLS: generate certs with `server/certs/generate-all.sh` and deploy them with `server/certs/install-broker.sh`; feed the generated passwords into the recorder's environment variables and into the device's `secrets.h`
4. In Home Assistant, include `server/ha_sensors.yaml` and `server/ha_automations.yaml` from `configuration.yaml`

### Verification Checklist

- [ ] LEDs settle: blue off (WiFi + NTP + MQTT all OK), green off (no motion)
- [ ] In DEBUG, Serial shows "Connected" for WiFi, MQTT; RTSP URL is printed
- [ ] Open the RTSP URL in VLC -- a live video feed is visible regardless of motion state
- [ ] Wave a hand in front of the camera -> green goes solid on once the motion event is notified
- [ ] Stop moving -> green stays on through the post-roll, then turns off; `server/clips/<device_id>/<date>/<timestamp>.mp4` appears on the server
- [ ] MQTT topics show motion/status/battery events; Home Assistant reflects the same state
