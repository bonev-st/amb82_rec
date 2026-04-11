/*
 * AMB82 Mini Motion-Triggered RTSP Streamer
 *
 * Detects motion via on-board camera pipeline (Channel 3, RGB).
 * On motion: starts RTSP stream (Channel 0, H264 FHD) and notifies
 * the server via MQTT to begin recording.
 * After post-roll: stops RTSP stream, notifies server to stop.
 *
 * Based on official Ameba Arduino SDK examples:
 *   - MotionDetection/CallbackPostProcessing
 *   - StreamRTSP/VideoOnly
 *   - MotionDetection/MaskingMP4Recording
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <StreamIO.h>
#include <VideoStream.h>
#include <RTSP.h>
#include <MotionDetection.h>

#include "config.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "battery_monitor.h"

// ============================================================
// Global Objects
// ============================================================

// Channel 0: High-res H264 for RTSP streaming (started on demand)
VideoSetting configStream(VIDEO_FHD, CAM_FPS, VIDEO_H264, 0);

// Channel 3: Low-res RGB for motion detection (always on)
VideoSetting configMD(VIDEO_VGA, 10, VIDEO_RGB, 0);

RTSP rtsp;
MotionDetection motionDet;

StreamIO videoToRtsp(1, 1);     // Camera Ch0 → RTSP
StreamIO videoToMotion(1, 1);   // Camera Ch3 → MotionDetection

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_TIMEZONE_OFFSET, NTP_UPDATE_INTERVAL);

// Managers
WifiManager wifiMgr;
MqttManager mqttMgr;
BatteryMonitor batteryMon;

WiFiClient mqttWifiClient;

// RTSP URL buffer
char rtspUrl[64] = {0};

// ============================================================
// State Machine
// ============================================================
enum StreamingState {
    STATE_IDLE,         // Waiting for motion (detection channel running)
    STATE_STREAMING,    // RTSP active, motion ongoing
    STATE_POST_ROLL     // Motion ended, RTSP still active for post-roll
};

StreamingState streamState = STATE_IDLE;
unsigned long motionEndTime = 0;
unsigned long streamStartTime = 0;
unsigned long lastStatusTime = 0;

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    LOG("\n========================================");
    LOG(" AMB82 Motion Camera — RTSP Streamer");
    LOG("========================================");

    // 1. WiFi
    wifiMgr.begin();

    // 2. NTP time sync (force initial fetch — update() is a no-op before interval elapses)
    timeClient.begin();
    if (!timeClient.forceUpdate()) {
        LOG("[NTP] forceUpdate() failed — time may be inaccurate");
    }
    LOGF("[NTP] Time: %s\n", timeClient.getFormattedTime().c_str());

    // 3. Battery monitor
    batteryMon.begin();

    // 4. MQTT
    mqttMgr.begin(mqttWifiClient);

    // 5. Configure video channels
    configStream.setBitrate(RTSP_BITRATE);
    Camera.configVideoChannel(RTSP_CHANNEL, configStream);
    Camera.configVideoChannel(DETECT_CHANNEL, configMD);
    Camera.videoInit();

    // 6. Configure RTSP (but don't start yet — wait for motion)
    rtsp.configVideo(configStream);

    // 7. Wire RTSP pipeline (Camera Ch0 → RTSP)
    videoToRtsp.registerInput(Camera.getStream(RTSP_CHANNEL));
    videoToRtsp.registerOutput(rtsp);
    if (videoToRtsp.begin() != 0) {
        LOG("[Pipeline] ERROR: videoToRtsp connection failed");
    }

    // 8. Configure motion detection
    motionDet.configVideo(configMD);
    motionDet.begin();

    // 9. Wire motion pipeline (Camera Ch3 → MotionDetection)
    videoToMotion.registerInput(Camera.getStream(DETECT_CHANNEL));
    videoToMotion.setStackSize();
    videoToMotion.registerOutput(motionDet);
    if (videoToMotion.begin() != 0) {
        LOG("[Pipeline] ERROR: videoToMotion connection failed");
    }

    // 10. Start motion detection channel (always on)
    Camera.channelBegin(DETECT_CHANNEL);

    // Wait for AE + MD background model to stabilize (prevents false triggers at boot).
    // Matches SDK pattern — see tests/test_motion/test_motion.ino.
    LOG("[Setup] Warming up motion detector (8s)...");
    delay(8000);

    // Build RTSP URL for MQTT announcements
    snprintf(rtspUrl, sizeof(rtspUrl), "rtsp://%s:%d",
             WiFi.localIP().get_address(), rtsp.getPort());

    LOG("[Setup] Complete — entering motion detection loop");
    LOGF("[Setup] RTSP URL (when streaming): %s\n", rtspUrl);
    LOG("========================================\n");

    // Publish initial status
    mqttMgr.publishStatus(batteryMon.getPercentage(), batteryMon.getVoltage(), wifiMgr.getRSSI());
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // --- Connectivity maintenance ---
    wifiMgr.ensureConnected();
    mqttMgr.ensureConnected();
    mqttMgr.loop();

    // --- Battery monitoring ---
    batteryMon.update();
    if (batteryMon.hasNewAlert()) {
        mqttMgr.publishBatteryAlert(
            batteryMon.getAlertLevel(),
            batteryMon.getPercentage(),
            batteryMon.getVoltage()
        );
    }

    // --- Periodic status ---
    if (now - lastStatusTime >= MQTT_STATUS_INTERVAL_MS) {
        lastStatusTime = now;
        mqttMgr.publishStatus(batteryMon.getPercentage(), batteryMon.getVoltage(), wifiMgr.getRSSI());
    }

    // --- Motion detection state machine ---
    bool motionDetected = checkMotion();
    handleStreamingState(motionDetected, now);

    delay(100);
}

// ============================================================
// Motion Detection
// ============================================================
bool checkMotion() {
    return motionDet.getResultCount() >= MOTION_DETECT_SENSITIVITY;
}

// ============================================================
// Streaming State Machine
// ============================================================
void handleStreamingState(bool motionDetected, unsigned long now) {
    switch (streamState) {
        case STATE_IDLE:
            if (motionDetected) {
                startStreaming();
                streamState = STATE_STREAMING;
                LOG("[State] IDLE -> STREAMING");
            }
            break;

        case STATE_STREAMING:
            if (!motionDetected) {
                motionEndTime = now;
                streamState = STATE_POST_ROLL;
                LOG("[State] STREAMING -> POST_ROLL (10s countdown)");
            }
            break;

        case STATE_POST_ROLL:
            if (motionDetected) {
                streamState = STATE_STREAMING;
                LOG("[State] POST_ROLL -> STREAMING (motion resumed)");
            } else if (now - motionEndTime >= MOTION_POST_ROLL_MS) {
                stopStreaming();
                streamState = STATE_IDLE;
                LOG("[State] POST_ROLL -> IDLE");
            }
            break;
    }
}

// ============================================================
// RTSP Stream Control
// ============================================================
void startStreaming() {
    streamStartTime = millis();

    // Update RTSP URL in case IP changed after reconnect
    snprintf(rtspUrl, sizeof(rtspUrl), "rtsp://%s:%d",
             WiFi.localIP().get_address(), rtsp.getPort());

    LOGF("[RTSP] Starting stream: %s\n", rtspUrl);

    // Start high-res video channel and RTSP server
    Camera.channelBegin(RTSP_CHANNEL);
    rtsp.begin();

    // Notify server to start recording
    mqttMgr.publishMotionEvent(true, rtspUrl);

    LOG("[RTSP] Stream active");
}

void stopStreaming() {
    unsigned long durationMs = millis() - streamStartTime;
    LOGF("[RTSP] Stopping after %.1f seconds\n", durationMs / 1000.0f);

    // Stop RTSP server and high-res channel (save power)
    rtsp.end();
    Camera.channelEnd(RTSP_CHANNEL);

    // Notify server to stop recording
    mqttMgr.publishMotionEvent(false, NULL);

    LOG("[RTSP] Stream stopped");
}
