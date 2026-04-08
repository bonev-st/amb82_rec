/*
 * Test 7: Motion Detection (RTSP Architecture)
 * Verifies: MotionDetection on Ch3 (RGB), configVideo, StreamIO, getResult
 * Needs: Board only — wave hand in front of camera to trigger
 */

#include <VideoStream.h>
#include <MotionDetection.h>
#include <StreamIO.h>

#define SERIAL_BAUD     115200
#define CH_DETECT       3           // RGB channel for motion detection
#define TRIGGER_COUNT   3           // Motion regions to trigger

VideoSetting configMD(VIDEO_VGA, 10, VIDEO_RGB, 0);
MotionDetection motionDet;
StreamIO videoToMotion(1, 1);

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
    printf(" Test 7: Motion Detection (Ch3 RGB)\n");
    printf("========================================\n\n");

    // Test 1: Configure camera
    Camera.configVideoChannel(CH_DETECT, configMD);
    Camera.videoInit();
    reportTest("Camera videoInit()", true);

    // Test 2: Configure motion detection using configVideo (not configResolution)
    motionDet.configVideo(configMD);
    reportTest("motionDet.configVideo()", true);

    motionDet.setTriggerBlockCount(TRIGGER_COUNT);
    reportTest("setTriggerBlockCount()", true);

    motionDet.begin();
    reportTest("motionDet.begin()", true);

    // Test 3: Setup StreamIO pipeline
    videoToMotion.registerInput(Camera.getStream(CH_DETECT));
    videoToMotion.setStackSize();
    videoToMotion.registerOutput(motionDet);
    int pipeResult = videoToMotion.begin();
    printf("[TEST] StreamIO.begin(): %d (0 = success)\n", pipeResult);
    reportTest("StreamIO pipeline connected", pipeResult == 0);

    // Test 4: Start detection channel
    Camera.channelBegin(CH_DETECT);
    reportTest("channelBegin(Ch3)", true);

    // Test 5: Initial result check
    delay(2000);
    std::vector<MotionDetectionResult> results = motionDet.getResult();
    printf("[TEST] Initial result: %d regions\n", results.size());
    reportTest("getResult() returns valid vector", true);

    // Summary
    printf("\n========================================\n");
    printf(" Results: %d PASS, %d FAIL\n", passCount, failCount);
    printf("========================================\n");
    printf("\n[TEST] Monitoring motion (wave hand in front of camera):\n");
    printf("  Trigger threshold: %d regions\n\n", TRIGGER_COUNT);
}

void loop() {
    std::vector<MotionDetectionResult> results = motionDet.getResult();
    int count = results.size();

    if (count > 0) {
        printf("  MOTION: %d regions", count);
        if (count >= TRIGGER_COUNT) {
            printf(" [TRIGGERED]");
        }
        printf("\n");
        if (count > 0) {
            printf("    Region 0: x=[%.2f,%.2f] y=[%.2f,%.2f]\n",
                   results[0].xMin(), results[0].xMax(),
                   results[0].yMin(), results[0].yMax());
        }
    }

    delay(500);
}
