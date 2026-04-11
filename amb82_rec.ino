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
    // LED pin first, matching test_motion.ino setup order
    pinMode(REC_LED_PIN, OUTPUT);
    digitalWrite(REC_LED_PIN, LOW);

    Serial.begin(SERIAL_BAUD);
    delay(2000);
    LOG("\n========================================");
    LOG(" AMB82 Motion Camera — RTSP Streamer");
    LOG("========================================");

    // ----------------------------------------------------------
    // Phase A — WiFi first
    // RTSP server binds to a network socket; rtsp.begin() must NOT be
    // called before WiFi is up or the sketch hangs.
    // Matches SDK MotionDetection/LoopPostProcessing.ino ordering.
    // ----------------------------------------------------------
    wifiMgr.begin();

    timeClient.begin();
    if (!timeClient.forceUpdate()) {
        LOG("[NTP] forceUpdate() failed — time may be inaccurate");
    }
    LOGF("[NTP] Time: %s\n", timeClient.getFormattedTime().c_str());

    // ----------------------------------------------------------
    // Phase B — Camera + RTSP + Motion Detection
    // BOTH Ch0 (H264) and Ch3 (RGB) are configured and started once at
    // boot and run continuously. Dynamically toggling Ch0/RTSP between
    // motion events produced RTSP clips that decoded as a static image
    // (the re-started Ch0 encoder emitted one cached sample). Keeping
    // Ch0 + RTSP + Ch0→RTSP pipeline alive permanently matches the SDK
    // example and gives a clean H264 stream on every motion event.
    // ----------------------------------------------------------
    configStream.setBitrate(RTSP_BITRATE);
    Camera.configVideoChannel(RTSP_CHANNEL, configStream);
    Camera.configVideoChannel(DETECT_CHANNEL, configMD);
    Camera.videoInit();

    // RTSP server + Ch0 → RTSP pipeline (WiFi is already up).
    rtsp.configVideo(configStream);
    rtsp.begin();
    videoToRtsp.registerInput(Camera.getStream(RTSP_CHANNEL));
    videoToRtsp.registerOutput(rtsp);
    if (videoToRtsp.begin() != 0) {
        LOG("[Pipeline] ERROR: videoToRtsp connection failed");
    }
    Camera.channelBegin(RTSP_CHANNEL);

    // Motion detection pipeline on Ch3.
    motionDet.configVideo(configMD);
    motionDet.begin();
    videoToMotion.registerInput(Camera.getStream(DETECT_CHANNEL));
    videoToMotion.setStackSize();
    videoToMotion.registerOutput(motionDet);
    if (videoToMotion.begin() != 0) {
        LOG("[Pipeline] ERROR: videoToMotion connection failed");
    }
    Camera.channelBegin(DETECT_CHANNEL);

    LOG("[Setup] Warming up motion detector (8s)...");
    delay(8000);
    LOGF("[Setup] Initial MD count: %u\n", motionDet.getResultCount());

    // ----------------------------------------------------------
    // Phase C — Battery + MQTT (after camera/RTSP are live)
    // ----------------------------------------------------------
    batteryMon.begin();
    mqttMgr.begin(mqttWifiClient);

    LOG("[Setup] Complete — entering motion detection loop");
    LOG("========================================\n");

    mqttMgr.publishStatus(batteryMon.getPercentage(), batteryMon.getVoltage(), wifiMgr.getRSSI());
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // Motion detection state machine — always first, never blocked.
    bool motionDetected = checkMotion();
    handleStreamingState(motionDetected, now);

    // IMPORTANT: mqttMgr.loop() and mqttMgr.ensureConnected() are
    // DELIBERATELY absent from the main loop. On the Ameba-patched
    // PubSubClient / WiFiClient stack these calls block the main task
    // indefinitely (~45 s at a time) which freezes motion detection.
    // Instead:
    //   - MqttManager::begin() calls setKeepAlive(0), so the broker never
    //     disconnects us for keepalive timeout.
    //   - publishMotionEvent() has its own force-reconnect path
    //     (mqtt_manager.cpp) and is the only code that touches MQTT state
    //     from the loop. Motion start/stop events reconnect on demand.
    //
    // WiFi maintenance is safe here because WifiManager::ensureConnected()
    // is a non-blocking state machine (wifi_manager.cpp Edit 3).
    if (streamState == STATE_IDLE) {
        wifiMgr.ensureConnected();
    }

    (void)now;
    delay(100);
}

// ============================================================
// Motion Detection
// ============================================================
bool checkMotion() {
    uint16_t count = motionDet.getResultCount();
    static uint16_t lastCount = 0xFFFF;
    static unsigned long lastPrint = 0;
    unsigned long now = millis();
    // Log on every count change, and at least every 500 ms regardless,
    // so the serial monitor shows continuous proof the MD pipeline is
    // alive — matches tests/test_motion/test_motion.ino's UX.
    if (count != lastCount || now - lastPrint >= 500) {
        LOGF("[MD] regions=%u%s\n", count, count ? "" : " .");
        lastCount = count;
        lastPrint = now;
    }
    return count >= MOTION_DETECT_SENSITIVITY;
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
// Ch0, RTSP server, and Ch0→RTSP pipeline run continuously from boot
// (see setup()). These functions are pure state-machine transitions —
// they publish the motion event, drive the LED, and track duration.
// The RTSP stream is *always* available on :554.
void startStreaming() {
    streamStartTime = millis();

    // Build RTSP URL (IP may have changed after reconnect).
    snprintf(rtspUrl, sizeof(rtspUrl), "rtsp://%s:%d",
             WiFi.localIP().get_address(), rtsp.getPort());
    LOGF("[RTSP] Motion-start: %s\n", rtspUrl);

    mqttMgr.publishMotionEvent(true, rtspUrl);
    digitalWrite(REC_LED_PIN, HIGH);
}

void stopStreaming() {
    unsigned long durationMs = millis() - streamStartTime;
    LOGF("[RTSP] Motion-stop after %.1f seconds\n", durationMs / 1000.0f);

    mqttMgr.publishMotionEvent(false, NULL);
    digitalWrite(REC_LED_PIN, LOW);
}
