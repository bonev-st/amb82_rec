# AMB82 Mini Motion-Triggered RTSP Streamer

Motion-triggered RTSP video streamer for the Ameba AMB82 Mini camera board. Detects motion via the on-board camera pipeline, starts an RTSP stream on demand, and notifies a Linux server via MQTT to begin recording. No local SD card storage required.

## Architecture

```
AMB82 Mini                              Linux Server
+-----------------------+               +---------------------------+
| Ch3 (VGA,RGB,10fps)  |               |                           |
|   -> MotionDetection  |--MQTT-------->| Mosquitto (MQTT broker)   |
|      (always on)      |  start/stop  |                           |
|                       |               | recorder.py               |
| Ch0 (FHD,H264,30fps) |               |   subscribes to MQTT      |
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
5. Remove any user-installed PubSubClient from Arduino libraries (use SDK version)
6. Compile and upload via USB

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
| `camera/<id>/status` | Camera -> Server | `{"battery_pct":85, "rssi":-45, "uptime":3600}` |
| `camera/<id>/battery` | Camera -> Server | `{"alert":"LOW", "battery_pct":18}` |
| `camera/<id>/clip` | Server -> HA | `{"file":"/clips/...", "size_bytes":...}` |
| `camera/<id>/availability` | Camera -> Server | `"online"` / `"offline"` (LWT) |

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

| # | Sketch | What it tests | Needs |
|---|--------|---------------|-------|
| 1 | `test_serial` | Serial, printf, LOG/LOGF macros, LED | Board only |
| 2 | `test_battery` | ADC read, voltage calc, % mapping | Board only |
| 3 | `test_wifi` | Connect, IP, RSSI, disconnect/reconnect | WiFi network |
| 4 | `test_ntp` | Time sync, epoch, formatted date/time | WiFi + internet |
| 5 | `test_camera` | Ch0 H264 + Ch3 RGB, channel start/stop | Board only |
| 6 | `test_motion` | MotionDetection on Ch3 RGB, getResult() | Board only |
| 7 | `test_rtsp` | RTSP start/stop, verify with VLC | WiFi network |
| 8 | `test_mqtt` | Connect, publish motion event with RTSP URL | MQTT broker |
