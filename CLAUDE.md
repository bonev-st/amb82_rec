# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Motion-triggered RTSP streamer for the **Ameba AMB82 Mini** camera board. The firmware detects motion via the on-board camera pipeline, starts an RTSP stream on demand, and notifies a Linux server via MQTT to begin FFmpeg recording. No local SD card storage.

## Target Platform

- **Board:** Ameba AMB82 Mini (Realtek RTL8735B, ARM Cortex-M33)
- **Toolchain:** Arduino IDE with Ameba Arduino SDK
- **Language:** C/C++ (Arduino `.ino` + helper modules)
- **Power source:** Battery — power optimization is a first-class concern

## Architecture

### Firmware (this repo)
- Motion detection via Channel 3 (VGA, RGB, 10fps) — always on, low power
- RTSP streaming via Channel 0 (FHD, H264, 30fps) — started/stopped on motion
- MQTT for signaling (motion start/stop with RTSP URL, status, battery alerts)
- Wi-Fi reconnect for reliability

### Server Stack (`server/` directory)
- **Mosquitto** — MQTT broker for event signaling
- **recorder.py** — Python script: subscribes to MQTT, spawns FFmpeg to record RTSP
- **Home Assistant** — automation, dashboards, notifications

## Key Design Decisions

- **Motion-triggered RTSP**, not continuous streaming (battery-friendly)
- No local SD card storage — server records clips via FFmpeg
- Camera detects motion locally (Ch3 RGB), starts RTSP (Ch0 H264) on demand
- MQTT carries motion events with RTSP URL so server knows when/where to record
- Low-battery alerts fire on threshold crossing only (no spam)
- Supports both battery and USB-powered operation

## Build & Flash

```bash
# Open in Arduino IDE, select board "AMB82 Mini" from Realtek Ameba boards package
# Required board package URL: https://github.com/ambiot/ambpro2_arduino
# Compile and upload via Arduino IDE (USB connection)
```

## Coding Conventions

- Clean, modular C/C++ appropriate for embedded constraints
- Compile-time configuration constants (Wi-Fi credentials, MQTT settings, thresholds, etc.) with clearly marked placeholders
- Descriptive names; comments only where non-obvious
- Reuse official Ameba Arduino SDK examples and APIs wherever possible
- No unnecessary abstractions — keep it practical for Arduino IDE

## AMB82 Mini SDK API Reference

SDK path: `C:\Users\bonev\AppData\Local\Arduino15\packages\realtek\hardware\AmebaPro2\4.1.0\`

### Video / Camera

**Header:** `libraries/Multimedia/src/VideoStream.h`

- `extern Video Camera;` — global camera instance (class is `Video`, NOT `Camera` as type name)
- Do NOT declare `Camera cam;` — use `Camera` directly

```cpp
// VideoSetting constructors
VideoSetting(uint8_t preset = 0);
VideoSetting(uint8_t resolution, uint8_t fps, uint8_t encoder, uint8_t snapshot);
VideoSetting(uint16_t w, uint16_t h, uint8_t fps, uint8_t encoder, uint8_t snapshot);
// Note: 5th param is `snapshot` (0 or 1), NOT channel number

// Video (Camera) methods
void configVideoChannel(int ch, VideoSetting& config);
void videoInit(void);
void channelBegin(int ch = 0);
void channelEnd(int ch = 0);
MMFModule getStream(int ch = 0);
```

**Constants:**
```
VIDEO_H264=1, VIDEO_HEVC=0, VIDEO_JPEG=2, VIDEO_RGB=4, VIDEO_NV12=3
VIDEO_FHD=6 (1920x1080), VIDEO_HD=5 (1280x720), VIDEO_VGA=3 (640x480)
V1_CHANNEL=0, V2_CHANNEL=1, V3_CHANNEL=2
CAM_FPS=30
```

**Important:** Motion detection requires **Channel 3 with VIDEO_RGB** format. Do not use H264 for motion detection. Channel 3 uses index `3` (not `V3_CHANNEL` which is `2`). You must also configure Channel 0 (H264) for sensor initialization — see Motion Detection section for details.

### RTSP Streaming

**Header:** `libraries/Multimedia/src/RTSP.h`

```cpp
RTSP(void);
void configVideo(VideoSetting& config);
void configAudio(AudioSetting& config, Audio_Codec_T codec);
void begin(void);   // Start RTSP server (can be called dynamically)
void end(void);     // Stop RTSP server (can be called dynamically)
int getPort(void);  // Get assigned RTSP port (typically 554)
void printInfo(void);
void printInfo(char* ip);  // Prints rtsp://<ip>:<port>
```

RTSP extends `MMFModule` — use with StreamIO like any other output.
`begin()`/`end()` map to `RTSPSetStreaming(ctx, 1/0)` — safe to call repeatedly.

### Audio (optional, for RTSP with audio)

**Header:** `libraries/Multimedia/src/AudioStream.h`

```cpp
// Audio codec enum
CODEC_AAC = AV_CODEC_ID_MP4A_LATM
CODEC_G711_PCMU = AV_CODEC_ID_PCMU
CODEC_G711_PCMA = AV_CODEC_ID_PCMA

// AudioSetting
AudioSetting(uint8_t preset = 0);
// Presets: 0=8kHz Mono AMIC, 1=16kHz Mono AMIC, 2=8kHz Mono DMIC, 3=16kHz Mono DMIC

// Audio class
Audio audio;
audio.configAudio(configA);
audio.begin();

// AAC encoder
AAC aac;
aac.configAudio(configA);
aac.begin();

// RTSP with audio: use StreamIO(2, 1) for video+audio → RTSP
rtsp.configVideo(configV);
rtsp.configAudio(configA, CODEC_AAC);
```

### Motion Detection

**Header:** `libraries/Multimedia/src/MotionDetection.h`

```cpp
MotionDetection(uint8_t row = 32, uint8_t col = 32);

void configResolution(uint8_t row = 32, uint8_t col = 32);  // NOT channel!
void configVideo(VideoSetting &config);
void begin(void);
void end(void);
void setTriggerBlockCount(uint16_t count);
void setDetectionMask(char *mask);
void setResultCallback(void (*md_callback)(std::vector<MotionDetectionResult>));
uint16_t getResultCount(void);
MotionDetectionResult getResult(uint16_t index);
std::vector<MotionDetectionResult> getResult(void);  // returns MotionDetectionResult, NOT vector<float>
```

**MotionDetectionResult** — has `xMin()`, `xMax()`, `yMin()`, `yMax()` (all return float)

**Important gotchas:**
- Motion detection uses **Channel 3** (not V3_CHANNEL=2). Channel 3 is a special NN/MD channel with index 3.
- Channel 3 (RGB) alone cannot initialize the camera sensor. **You must also configure Channel 0** (H264) via `Camera.configVideoChannel(0, configV)` before `Camera.videoInit()`. Channel 0 does NOT need to be started (`channelBegin`) — just configured. Starting it without a consumer causes `CH 0 MMF ENC Queue full` warnings.
- **Do NOT call `setTriggerBlockCount()` before `begin()`** — it enables `CMD_EIP_SET_MD_OUTPUT` which changes the MD module's internal output mode and causes stale/stuck results.
- Use **`getResultCount()`** to check for motion, not `getResult().size()`. This matches the SDK examples.
- Allow **5–8 seconds warm-up** after `channelBegin()` for auto-exposure (AE) to stabilize and the MD background model to build. Without this, you get constant false detections.

### MP4Recording

**Header:** `libraries/Multimedia/src/MP4Recording.h`

```cpp
void configVideo(VideoSetting& config);
void configAudio(AudioSetting& config, Audio_Codec_T codec);
void begin(void);
void end(void);
void setRecordingFileName(const char* filename);
void setRecordingFileName(String filename);
void setRecordingDuration(uint32_t secs);
void setRecordingFileCount(uint32_t count);
void setLoopRecording(int enable);
uint8_t getRecordingState(void);
String getRecordingFileName(void);
```

### StreamIO

**Header:** `libraries/Multimedia/src/StreamIO.h`

```cpp
StreamIO(uint8_t numInput, uint8_t numOutput);
int begin(void);
void end(void);
void registerInput(const MMFModule &module);
void registerInput1(const MMFModule &module);  // for multi-input
void registerInput2(const MMFModule &module);
void registerOutput(const MMFModule &module);
void registerOutput1(const MMFModule &module);  // for multi-output
void registerOutput2(const MMFModule &module);
```

### AmebaFatFS / File

**Headers:** `libraries/FileSystem/src/AmebaFatFS.h`, `AmebaFatFSFile.h`

```cpp
// AmebaFatFS
bool begin(void);
void end(void);
File open(const char *path);         // returns File object
File open(const char *path, int fileType);
bool exists(const char *path);
bool remove(const char *path);
bool rename(const char *pathFrom, const char *pathTo);
bool mkdir(const char *path);
char *getRootPath(void);
long long int get_free_space(void);

// File (extends Stream)
void close(void);
size_t write(const uint8_t *buf, size_t size);
int read(void *buf, size_t size);
int available(void);
bool seek(uint32_t pos);
uint32_t size(void);
bool isOpen(void);
const char *name(void);
```

### WiFi

**Header:** `libraries/WiFi/src/WiFi.h`

```cpp
extern WiFiClass WiFi;

int begin(char* ssid, const char* passphrase);
int disconnect(void);
uint8_t status();        // WL_CONNECTED etc.
IPAddress localIP(uint8_t interface = 0);
int32_t RSSI();
char* SSID();
```

**IPAddress** (`cores/ambpro2/IPAddress.h`):
- Use `get_address()` to get string — NO `toString()` method

**WiFiClient** (`libraries/WiFi/src/WiFiClient.h`):
```cpp
int connect(const char *host, uint16_t port);
size_t write(const uint8_t *buf, size_t size);
int available(void);
int read(void);
void stop(void);
uint8_t connected(void);
// Inherits print(), println() from Print
```

### MQTT (PubSubClient)

**Use SDK version:** `libraries/MQTTClient/src/PubSubClient.h` (has Ameba patches, MQTT_MAX_PACKET_SIZE=512)
**User library also installed:** `C:\Users\bonev\Documents\Arduino\libraries\PubSubClient` (standard, 256 byte packets)

```cpp
PubSubClient& setClient(Client& client);
PubSubClient& setServer(const char* domain, uint16_t port);

boolean connect(const char* id, const char* user, const char* pass,
                const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
void disconnect();
boolean publish(const char* topic, const char* payload);
boolean publish(const char* topic, const char* payload, boolean retained);
boolean subscribe(const char* topic);
boolean loop();
boolean connected();
int state();
```

### NTP

**Header:** `libraries/NTPClient/src/NTPClient.h`

```cpp
NTPClient(UDP& udp, const char* poolServerName, long timeOffset, unsigned long updateInterval);
void begin();
bool update();
bool forceUpdate();
unsigned long getEpochTime() const;
String getFormattedTime() const;    // "hh:mm:ss"
String getFormattedDate() const;    // "dd:mm:yyyy"
int getYear() const;
int getMonth() const;
int getMonthDay() const;
```

### Serial / Debug Printing

**Header:** `cores/ambpro2/LOGUARTClass.h`

- `extern LOGUARTClass Serial;`
- `Serial.print()` / `Serial.println()` — standard Print methods
- **NO `Serial.printf()`** — `LOGUARTClass` does not have `printf`
- Global `printf()` is available — mapped to `dbg_printf()` via macro in `cores/ambpro2/Arduino.h`
- Global `sprintf()` is available — mapped to `dbg_sprintf()`

### ADC / Analog

**Headers:** `cores/ambpro2/wiring_analog.h`, `variants/ameba_amb82-mini/variant.h`

```cpp
uint32_t analogRead(uint32_t ulPin);
void analogReadResolution(int res);  // default 10-bit (0-1023)
```

**AMB82 Mini ADC pins:** A0 (PF_0), A1 (PF_1), A2 (PF_2), A4 (PA_0), A5 (PA_1), A6 (PA_2), A7 (PA_3)
**Note:** No A3 pin available.
**Built-in LED:** `LED_BUILTIN` (PF_9, blue), **Button:** `PUSH_BTN` (PF_10)

### Linux Server (for tests)
## Access to remote Linux test machine
System: Debian 13 (aarch64)
Use the SSH alias configured locally on the developer machine:

```bash
ssh test-system
```
#### MQTT Broker (Mosquitto)
Installed and running on the Debian host for `tests/test_mqtt/test_mqtt.ino` and
any other firmware test that needs a broker.

- Service: `mosquitto.service` (active, enabled at boot)
- Package: `mosquitto` + `mosquitto-clients` (v2.0.21)
- Listener: `0.0.0.0:1883`, anonymous allowed (LAN-only test setup)
- LAN-exposing config: `/etc/mosquitto/conf.d/local.conf`
  ```
  listener 1883 0.0.0.0
  allow_anonymous true
  ```
- Main config: `/etc/mosquitto/mosquitto.conf` (distro defaults, includes `conf.d/`)
- Firewall: none active (no ufw, no iptables rules)

**Point firmware tests at it:** set `MQTT_BROKER "sbbu01.local"` and leave
`MQTT_USER`/`MQTT_PASS` empty. Example (`tests/test_mqtt/test_mqtt.ino`):
```c
#define MQTT_BROKER "sbbu01.local"
#define MQTT_PORT   1883
#define MQTT_USER   ""
#define MQTT_PASS   ""
```

**Watch traffic from the host** (useful for verifying firmware publishes):
```bash
ssh test-system "mosquitto_sub -h 127.0.0.1 -t '#' -v"
# Or filter to the test topics:
ssh test-system "mosquitto_sub -h 127.0.0.1 -t 'amb82_test/#' -v"
```

**Inject test messages from the host:**
```bash
ssh test-system "mosquitto_pub -h 127.0.0.1 -t amb82_test/cmd -m hello"
```

**Service ops:**
```bash
sudo systemctl {status,restart,stop,start} mosquitto
sudo journalctl -u mosquitto -f
```

Note: `allow_anonymous true` is a LAN test convenience. For the production
recorder pipeline (`server/` stack), switch to a `password_file` and set
`allow_anonymous false`.

#### Recorder (recorder.py)
Runs on the Debian host as a user-level systemd service.

- Service: `amb82-recorder.service` (user service under `arduino` user)
- Install path: `/home/arduino/amb82_recorder/`
- Python venv: `/home/arduino/amb82_recorder/.venv/`
- Clips path: `/home/arduino/amb82_clips/`

**Service ops:**
```bash
systemctl --user status amb82-recorder
systemctl --user restart amb82-recorder
systemctl --user stop amb82-recorder
journalctl --user -u amb82-recorder -f
```

**Timezone:** Clip filenames and timestamps use the server's system timezone.
Set it with `sudo timedatectl set-timezone Europe/Sofia` (or as needed).
