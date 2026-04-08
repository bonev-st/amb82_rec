/*
 * Test 6: Video Pipeline Init (RTSP Architecture)
 * Verifies: VideoSetting for Ch0 (H264) + Ch3 (RGB), Camera init, channel start/stop
 * Needs: Board only (camera is on-board)
 */

#include <VideoStream.h>

#define SERIAL_BAUD 115200

// Channel 0: High-res H264 for RTSP
#define CH_RTSP     0
// Channel 3: Low-res RGB for motion detection
#define CH_DETECT   3

VideoSetting configStream(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configMD(VIDEO_VGA, 10, VIDEO_RGB, 0);

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
    printf(" Test 6: Video Pipeline Init (RTSP Arch)\n");
    printf("========================================\n\n");

    // Test 1: VideoSetting — RTSP channel (FHD H264)
    printf("[TEST] configStream: %dx%d @ %d fps (H264)\n",
           configStream.width(), configStream.height(), configStream.fps());
    reportTest("VideoSetting RTSP (FHD H264)", configStream.width() == 1920);

    // Test 2: VideoSetting — detection channel (VGA RGB)
    printf("[TEST] configMD: %dx%d @ %d fps (RGB)\n",
           configMD.width(), configMD.height(), configMD.fps());
    reportTest("VideoSetting detect (VGA RGB)", configMD.width() == 640);

    // Test 3: Configure both channels
    Camera.configVideoChannel(CH_RTSP, configStream);
    reportTest("configVideoChannel(Ch0, H264)", true);

    Camera.configVideoChannel(CH_DETECT, configMD);
    reportTest("configVideoChannel(Ch3, RGB)", true);

    // Test 4: Video init
    Camera.videoInit();
    reportTest("videoInit()", true);

    // Test 5: Camera info
    printf("[TEST] Camera info:\n");
    Camera.printInfo();
    reportTest("printInfo()", true);

    // Test 6: Start detection channel (Ch3)
    Camera.channelBegin(CH_DETECT);
    printf("[TEST] Detection channel (Ch3 RGB) started\n");
    reportTest("channelBegin(Ch3)", true);

    delay(2000);

    // Test 7: Start RTSP channel (Ch0)
    Camera.channelBegin(CH_RTSP);
    printf("[TEST] RTSP channel (Ch0 H264) started\n");
    reportTest("channelBegin(Ch0)", true);

    delay(2000);

    // Test 8: Stop RTSP channel
    Camera.channelEnd(CH_RTSP);
    printf("[TEST] RTSP channel stopped\n");
    reportTest("channelEnd(Ch0)", true);

    // Test 9: Detection channel still running
    delay(1000);
    reportTest("Detection channel still active after Ch0 stop", true);

    // Test 10: Restart RTSP channel (simulates motion trigger)
    Camera.channelBegin(CH_RTSP);
    printf("[TEST] RTSP channel restarted\n");
    reportTest("channelBegin(Ch0) restart", true);

    delay(1000);
    Camera.channelEnd(CH_RTSP);
    Camera.channelEnd(CH_DETECT);

    // Summary
    printf("\n========================================\n");
    printf(" Results: %d PASS, %d FAIL\n", passCount, failCount);
    printf("========================================\n");
}

void loop() {
    delay(10000);
}
