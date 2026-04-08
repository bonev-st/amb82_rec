You are a senior embedded/IoT engineer. Design and implement an Arduino IDE project for the **Ameba AMB82 Mini** camera board.
Goal
----

Create a production-oriented firmware prototype that:

1. Connects to a server over **Wi-Fi**

2. Detects **motion**

3. Records **video only when motion is detected**

4. Continues recording for **10 seconds after motion ends**

5. Uploads the recorded video clip to a server

6. Minimizes **battery power consumption** as much as possible

7. Provides a mechanism for:
   
   * **low battery notifications**
   
   * **new video notifications**

Very important constraints
--------------------------

* Platform: **Ameba AMB82 Mini**

* Toolchain: **Arduino IDE**

* Power source: **battery**

* Favor the **lowest practical power consumption**

* Prefer a **robust, simple, maintainable architecture**

* Avoid overengineering

* Reuse official Arduino SDK examples and APIs where possible

* The solution must be realistic for this board and its available Arduino libraries

First task: feasibility and architecture study
----------------------------------------------

Before writing code, analyze the best architecture and explain tradeoffs.

### Research the board capabilities and official examples

Investigate which official Arduino examples/APIs are most suitable for:

* motion detection

* MP4 recording

* Wi-Fi connection

* file upload

* power saving / sleep / standby / deep sleep

### Important design note

Since this is a battery-powered camera, do not assume that continuous streaming is acceptable.  
Prefer an **event-based clip upload architecture** instead of continuous RTSP streaming to the server, unless you can justify otherwise.
Server-side research and recommendation
---------------------------------------

Research and compare the most suitable server architecture for storing uploaded motion clips and generating notifications.

Compare at least these options:

1. **Home Assistant + MQTT + object storage**

2. **NAS/shared folder upload**

3. **Cloud object storage / cloud webhook approach**

4. **NVR-style server**

Then choose the best option for this use case.
Preferred recommendation
------------------------

Unless your research shows a clearly better option, prefer this architecture:

* **Home Assistant** for automation and notifications

* **MQTT** for device status, battery level, and motion event metadata

* **MinIO** (or another S3-compatible local object storage) for storing uploaded video clips

* n8n

* NodeRed

* Home Assistant automation for:
  
  * low battery alerts
  
  * new video alerts
  
  * optional dashboard / clip links

Explain why this is the best tradeoff in terms of:

* simplicity

* local control

* low ongoing cost

* maintainability

* extensibility

Firmware functional requirements
--------------------------------

Implement or outline firmware that does the following:

### 1. Boot and initialization

* initialize camera

* initialize Wi-Fi

* initialize time if needed for timestamped filenames

* initialize battery measurement

* initialize motion detection pipeline

* initialize upload client

* initialize power-saving policy

### 2. Motion detection behavior

* detect motion using the most suitable official mechanism

* start recording when motion is detected

* continue recording while motion remains active

* when motion stops, keep recording for **10 more seconds**

* if motion resumes during the post-roll interval, continue recording seamlessly

* finalize the clip only after motion has truly ended and the extra 10 seconds have elapsed

### 3. Clip handling

* generate safe timestamp-based filenames

* save clip locally first if needed

* upload clip reliably to the server

* include metadata if practical:
  
  * timestamp
  
  * clip duration
  
  * battery percentage or voltage
  
  * Wi-Fi RSSI
  
  * device name / camera ID

### 4. Notifications

Implement a clean notification strategy:

#### Low battery

* periodically measure battery voltage

* estimate battery state using configurable thresholds

* send a low-battery notification only when crossing threshold, not continuously spamming

#### New video

* notify the server when a new clip is available

* include clip path / object name / timestamp / duration

* if direct device notification is not ideal, let the server generate the end-user notification

### 5. Reliability

* handle Wi-Fi reconnect

* handle temporary server outage

* queue or retry uploads if upload fails

* do not lose clips easily

* avoid corrupting recordings on reconnect/retry scenarios

* add clear serial logs for debugging

Power optimization requirements
-------------------------------

This part is critical.

You must explicitly analyze how to reduce power consumption on AMB82 Mini.

Consider:

* whether camera-based motion detection requires the board to stay active

* whether deep sleep is compatible with continuous motion watching

* whether standby / light sleep / retention mode is more appropriate

* whether motion detection should run in a lower frame rate or lower resolution mode

* whether clip recording should switch to a different encode/quality profile

* how often battery should be measured

* how often status should be reported

Provide:

1. a **realistic power strategy**

2. a **best practical compromise**

3. a **short explanation of what cannot be optimized further due to the requirement for motion-triggered video**

Implementation deliverables
---------------------------

Produce the following:

### A. System design summary

A concise architecture description with:

* device responsibilities

* server responsibilities

* protocol choices

* power strategy

### B. Recommended server deployment

Provide a practical deployment recommendation, preferably:

* Home Assistant

* MQTT broker

* MinIO or equivalent S3-compatible storage

* example automation flow for notifications

### C. Arduino firmware

Generate a complete Arduino project skeleton or full implementation with:

* main `.ino` file

* helper classes/modules if useful

* configuration section for:
  
  * Wi-Fi SSID/password
  
  * server address/port
  
  * MQTT settings if used
  
  * upload endpoint or S3-compatible endpoint
  
  * battery thresholds
  
  * motion parameters
  
  * recording parameters

### D. Configuration placeholders

Use clearly marked placeholders for:

* Wi-Fi credentials

* API keys / tokens

* MQTT credentials

* webhook URLs

* server upload credentials

### E. Explanation

Explain:

* which parts are fully implemented

* which parts are board/API-dependent and may need adjustment after compilation

* which official AMB82 examples your solution is based on

Coding style requirements
-------------------------

* Write clean, modular, readable code

* Use descriptive names

* Avoid unnecessary abstractions

* Add comments only where useful

* Keep the code realistic for Arduino IDE and embedded constraints

* Prefer compile-time configuration constants where possible

Acceptance criteria
-------------------

The solution should satisfy these conditions:

* motion starts recording

* recording continues for 10 seconds after motion ends

* clip is uploaded to the server

* low battery can trigger an alert

* new clip can trigger an alert

* Wi-Fi reconnect is handled

* approach is reasonably power-optimized for battery operation

Output format
-------------

Respond in this exact structure:

1. **Feasibility assessment**

2. **Best server architecture recommendation**

3. **Power optimization strategy**

4. **Firmware architecture**

5. **Arduino code**

6. **Server-side integration outline**

7. **Setup and test procedure**

8. **Known limitations / assumptions**

Think carefully and prefer a practical implementation over a theoretically perfect one.  
Use the official AMB82 Mini Arduino capabilities as the foundation, not a generic ESP32-style solution.
