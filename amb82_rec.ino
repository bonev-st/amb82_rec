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
#include <time.h>
// rtc_write() from rtc_api.h is the SDK-linkable symbol (in liboutsrc.a)
// that writes the hardware RTC. mbedTLS reads from this RTC to check cert
// notBefore/notAfter during TLS handshake. Note: set_time() declared in
// rtc_time.h is NOT compiled into the platform libs — use rtc_write().
extern "C" void rtc_init(void);
extern "C" void rtc_write(time_t t);

#include "config.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "battery_monitor.h"

#if WDT_ENABLED
#include <WDT.h>
#endif

// ============================================================
// Global Objects
// ============================================================

// Channel 0: High-res H264 for RTSP streaming (started on demand)
VideoSetting configStream(VIDEO_FHD, CAM_FPS, VIDEO_H264, 0);

// Channel 3: Low-res RGB for motion detection (always on)
VideoSetting configMD(VIDEO_VGA, DETECT_FPS, VIDEO_RGB, 0);

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

#if MQTT_USE_TLS
WiFiSSLClient mqttWifiClient;
#else
WiFiClient mqttWifiClient;
#endif

#if WDT_ENABLED
WDT wdt(0);  // SDK bug: WDT() declared but not defined; use WDT(int) constructor
#endif

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

#ifndef BUILD_RELEASE
    Serial.begin(SERIAL_BAUD);
    delay(2000);  // Let serial monitor attach
#endif
    LOG("\n========================================");
    LOGF(" %s v%s [%s]\n", DEVICE_NAME, FIRMWARE_VERSION, FIRMWARE_BUILD);
    LOG("========================================");

    // ----------------------------------------------------------
    // Phase A — WiFi first
    // RTSP server binds to a network socket; rtsp.begin() must NOT be
    // called before WiFi is up or the sketch hangs.
    // Matches SDK MotionDetection/LoopPostProcessing.ino ordering.
    // ----------------------------------------------------------
    wifiMgr.begin();

    timeClient.begin();
    for (int attempt = 1; attempt <= 5; attempt++) {
        if (timeClient.forceUpdate()) {
            LOGF("[NTP] Synced: %s (attempt %d)\n",
                 timeClient.getFormattedTime().c_str(), attempt);
            break;
        }
        LOGF("[NTP] Sync failed (attempt %d/5)\n", attempt);
        if (attempt < 5) delay(2000);
    }

    // Write the NTP epoch to the hardware RTC. mbedTLS reads from the RTC
    // (via time()) to check cert notBefore/notAfter during TLS handshake.
    // Without this, every cert looks "not yet valid" → -0x2700 handshake fail.
    unsigned long epoch = timeClient.getEpochTime();
    if (epoch > 1704067200UL) {  // sanity: after 2024-01-01
        rtc_init();
        rtc_write((time_t)epoch);
        LOGF("[RTC] System time set to epoch %lu\n", epoch);
    } else {
        LOG("[RTC] WARNING: NTP epoch invalid, RTC not set — TLS will fail");
    }

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

#if MQTT_USE_TLS
    mqttWifiClient.setRootCA((unsigned char*)mqtt_ca_cert);
    mqttWifiClient.setClientCertificate((unsigned char*)mqtt_client_cert,
                                        (unsigned char*)mqtt_client_key);
    LOG("[MQTT] TLS + mTLS enabled (port 8883)");
#else
    LOG("[MQTT] Plain connection (port 1883)");
#endif
    mqttMgr.begin(mqttWifiClient, timeClient);

    // ----------------------------------------------------------
    // Phase D — Watchdog (release builds only)
    // ----------------------------------------------------------
#if WDT_ENABLED
    wdt.init(WDT_TIMEOUT_MS);
    wdt.start();
    LOG("[WDT] Watchdog started (30s timeout)");
#endif

    LOG("[Setup] Complete — entering motion detection loop");
    LOG("========================================\n");

    mqttMgr.publishStatus(batteryMon.getPercentage(), batteryMon.getVoltage(), wifiMgr.getRSSI());
    lastStatusTime = millis();
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // Motion detection state machine — always first, never blocked.
    bool motionDetected = checkMotion();
    handleStreamingState(motionDetected, now);

    // Battery monitoring — update() has its own 60s throttle
    batteryMon.update();
    if (batteryMon.hasNewAlert()) {
        mqttMgr.publishBatteryAlert(
            batteryMon.getAlertLevel(),
            batteryMon.getPercentage(),
            batteryMon.getVoltage()
        );
    }

    // Periodic status publish (every MQTT_STATUS_INTERVAL_MS = 1 hour)
    if (now - lastStatusTime >= MQTT_STATUS_INTERVAL_MS) {
        lastStatusTime = now;
        mqttMgr.publishStatus(
            batteryMon.getPercentage(),
            batteryMon.getVoltage(),
            wifiMgr.getRSSI()
        );
    }

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
        timeClient.update();    // re-sync NTP (self-throttled to once per hour)
        mqttMgr.checkConfig();  // poll for timezone/config updates
    }

#if WDT_ENABLED
    wdt.refresh();
#endif

    // Adaptive delay: save power when idle, stay responsive during streaming
    delay(streamState == STATE_IDLE ? LOOP_DELAY_IDLE_MS : LOOP_DELAY_ACTIVE_MS);
}

// ============================================================
// Motion Detection
// ============================================================
bool checkMotion() {
    uint16_t count = motionDet.getResultCount();
#ifndef BUILD_RELEASE
    // Verbose MD logging: every count change + every 500ms heartbeat.
    // Debug only — serial I/O and printf formatting waste power.
    static uint16_t lastCount = 0xFFFF;
    static unsigned long lastPrint = 0;
    unsigned long now = millis();
    if (count != lastCount || now - lastPrint >= 500) {
        LOGF("[MD] regions=%u%s\n", count, count ? "" : " .");
        lastCount = count;
        lastPrint = now;
    }
#endif
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
    mqttMgr.publishStatus(batteryMon.getPercentage(), batteryMon.getVoltage(), wifiMgr.getRSSI());
    lastStatusTime = millis();  // Reset hourly timer so we don't double-publish
    digitalWrite(REC_LED_PIN, LOW);
}
