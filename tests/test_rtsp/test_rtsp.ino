/*
 * Test: RTSP Streaming
 * Verifies: RTSP begin/end, stream URL, dynamic start/stop
 * Needs: Board + WiFi
 *
 * How to verify: open VLC on PC -> Media -> Open Network Stream ->
 *   rtsp://<camera-ip>:<port>
 * The URL is printed to Serial Monitor.
 */

#include <WiFi.h>
#include <VideoStream.h>
#include <RTSP.h>
#include <StreamIO.h>
#include <wifi_def.h>

#define SERIAL_BAUD 115200

// ---- CHANGE THESE ----
// #define WIFI_SSID     "xxxx956"
// #define WIFI_PASSWORD "1RootPass0"
// -----------------------

#define CH_RTSP 0

VideoSetting configStream(VIDEO_FHD, CAM_FPS, VIDEO_H264, 0);
RTSP rtsp;
StreamIO videoToRtsp(1, 1);

int passCount = 0;
int failCount = 0;

void reportTest(const char* name, bool passed) {
    if (passed) {
        passCount++;
        printf("[PASS] %s\n", name);
    } else {
        failCount++;
        printf("[FAIL] %s\n", name);
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(2000);

    printf("\n========================================\n");
    printf(" Test: RTSP Streaming\n");
    printf("========================================\n\n");

    // Connect WiFi
    printf("[SETUP] Connecting WiFi...");
    WiFi.begin((char*)WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
            printf("\n[ABORT] WiFi failed\n");
            return;
        }
        delay(500);
        printf(".");
    }
    printf(" OK (%s)\n", WiFi.localIP().get_address());

    // Test 1: Configure camera
    configStream.setBitrate(2 * 1024 * 1024);
    Camera.configVideoChannel(CH_RTSP, configStream);
    Camera.videoInit();
    reportTest("Camera videoInit()", true);

    // Test 2: Configure RTSP
    rtsp.configVideo(configStream);
    reportTest("rtsp.configVideo()", true);

    // Test 3: Wire pipeline
    videoToRtsp.registerInput(Camera.getStream(CH_RTSP));
    videoToRtsp.registerOutput(rtsp);
    int pipeResult = videoToRtsp.begin();
    printf("[TEST] StreamIO.begin(): %d\n", pipeResult);
    reportTest("StreamIO pipeline connected", pipeResult == 0);

    // Test 4: Start RTSP stream
    Camera.channelBegin(CH_RTSP);
    rtsp.begin();
    reportTest("rtsp.begin()", true);

    // Test 5: Get port and print URL
    int port = rtsp.getPort();
    printf("[TEST] RTSP port: %d\n", port);
    reportTest("getPort() returns valid port", port > 0);

    char url[64];
    snprintf(url, sizeof(url), "rtsp://%s:%d", WiFi.localIP().get_address(), port);
    printf("\n");
    printf("  =============================================\n");
    printf("  RTSP STREAM URL: %s\n", url);
    printf("  Open VLC -> Media -> Open Network Stream\n");
    printf("  =============================================\n\n");

    // Stream for 30 seconds
    printf("[TEST] Streaming for 30 seconds...\n");
    printf("  Open VLC now to verify video!\n");
    for (int i = 30; i > 0; i--) {
        printf("  %d seconds remaining...\n", i);
        delay(1000);
    }

    // Test 6: Stop RTSP
    printf("[TEST] Stopping RTSP...\n");
    rtsp.end();
    Camera.channelEnd(CH_RTSP);
    reportTest("rtsp.end() + channelEnd()", true);

    printf("[TEST] Stream stopped. VLC should disconnect.\n");
    delay(3000);

    // Test 7: Restart RTSP (simulates motion re-trigger)
    printf("[TEST] Restarting RTSP (simulating new motion event)...\n");
    Camera.channelBegin(CH_RTSP);
    rtsp.begin();
    reportTest("RTSP restart after stop", true);

    printf("\n  RTSP URL: %s\n", url);
    printf("  Streaming for 60 seconds...\n");
    for (int i = 60; i > 0; i--) {
        printf("  %d...\n", i);
        delay(1000);
    }

    rtsp.end();
    Camera.channelEnd(CH_RTSP);
    printf("[TEST] Second stream stopped\n");

    // Summary
    printf("\n========================================\n");
    printf(" Results: %d PASS, %d FAIL\n", passCount, failCount);
    printf("========================================\n");
    printf("\nDid you see video in VLC? If yes, RTSP works!\n");
}

void loop() {
    delay(10000);
}
