# SMARTMINI — README

![SMARTMINI Overview](/images/smartwatch.png)

---

## Project Overview

**SMARTMINI** is a modular smartwatch prototype built on an **STM32F4** MCU. The watch includes a Home/Clock screen, Light & Door Control, Health Tracker, Voice-Control Automation, and Wireless Communication. The UI is implemented using **TouchGFX** for smooth embedded graphics. Firmware integrates sensors (IMU, heart-rate), ADC, RTC, DMA, and UART/I²C peripherals.

This README explains, step-by-step, how each screen was created using TouchGFX and how it connects to the underlying STM32 firmware.

---

## Requirements

**Hardware**

* STM32F4 (STM32F412G-DISCO used in prototype)
* 240×240 TFT display (SPI or parallel interface)
* MPU6050 (IMU) or equivalent
* Heart-rate sensor (ADC-based PPG or MAX30102)
* BLE/UART module for wireless features
* Li-ion battery, regulator, and charger

**Software / Tools**

* STM32CubeIDE
* TouchGFX Designer (TouchGFX Designer + TouchGFX HAL integration)
* STM32CubeMX (optional for peripheral config)
* ST-Link for flashing & debugging

---

## Project Structure (recommended)

```
SMARTMINI/
├─ touchgfx/            # TouchGFX Designer project (views, presenters, assets)
├─ STM32Cube/           # CubeMX generated code and HAL drivers
├─ App/                 # Application layer (apps manager, power manager)
├─ Drivers/             # Sensor drivers (I2C, ADC), UART comms
├─ Docs/                # This README, diagrams, test logs
└─ Resources/           # Images, fonts, audio assets
```

---

## TouchGFX Setup (quick)

1. Create a new TouchGFX project for the target STM32 board in TouchGFX Designer.
2. Add screens in Designer: Home, LightControl, Health, VoiceControl, Wireless.
3. Import bitmaps and fonts into the TouchGFX assets. Keep images compressed and use appropriate color depth to save RAM.
4. Generate code from TouchGFX Designer and open the generated project in STM32CubeIDE.
5. Implement presenters and connect to HAL drivers (RTC, I2C, ADC, UART).

---

## How each screen was built (step-by-step)

### Part 1 — Home / Clock (TouchGFX)

**Goal:** Show a real-time analog/digital clock and quick access to apps.

**Steps in TouchGFX Designer:**

1. Create a new `HomeScreen` view.
2. Add a background image (bitmap) to the screen. Use `Image` widget.
3. Add a `TextArea` (or `TypedText`) to display the digital time. Create an ID like `T_TEXT_TIME` in the text resources.
4. (Optional) Add an analog clock widget (group of bitmaps for hour/minute/second hands) and set rotation anchors.
5. Add navigation buttons or swipe gestures to open the app menu.
6. Set a periodic UI tick (TouchGFX tick or timer event) to refresh the time every second.

**Presenter / Firmware integration:**

* In `HomeView::setupScreen()` start a tick timer. On each tick call `presenter->requestUpdateTime()`.
* `HomePresenter::requestUpdateTime()` reads RTC using HAL and formats the time string, then calls `view.updateTime(formattedTime)`.

**Example code (presenter -> view):**

```cpp
// in HomeView
void HomeView::setupScreen() {
    tick.start(); // TouchGFX timer configured to 1 second
}

void HomeView::handleTickEvent() {
    presenter->requestUpdateTime();
}

// in HomePresenter
void HomePresenter::requestUpdateTime() {
    RTC_TimeTypeDef sTime;
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
    view.updateTime(buf);
}
```

**Notes / Tips:**

* Keep text updates minimal: only update the `TextArea` when time changes.
* Avoid heavy logic on UI thread; use presenter to fetch data.

---

### Part 2 — Light & Door Control

**Goal:** Provide on-screen control for lights and door lock with immediate visual feedback.

**Steps in TouchGFX Designer:**

1. Create a `LightControlView` and import button bitmaps (ON/OFF states) or use `Button` widgets.
2. For each control, create `Button` and an `Image` or `Icon` to show current state.
3. Assign callback actions in Designer (or in the generated view) to trigger `onLightToggle()` or `onDoorToggle()`.
4. Provide a `Back` button to return to Home.

**Presenter / Firmware integration:**

* The view callback calls a presenter method which queues or directly performs a HAL call to toggle GPIO or to send a wireless command (via UART/BLE) to a relay module.

**Example (View callback):**

```cpp
void LightControlView::onLampButtonPressed(const AbstractButton& b) {
    presenter->toggleLamp(1); // logical lamp id
}

// Presenter calls HAL
void LightControlPresenter::toggleLamp(uint8_t id) {
    // Option 1: Local relay pin toggle
    HAL_GPIO_TogglePin(LAMP_GPIO_Port, LAMP_Pin);
    // Option 2: Send command via UART to remote module
    // sendCommand("LIGHT:1:TOGGLE");
    // Update view
    view.updateLampState(id, newState);
}
```

**Notes:**

* Debounce hardware buttons with a short software debounce if physical buttons are used.
* Use images for state change to keep UI responsive and clear.

---

### Part 3 — Health Tracker (Heart Rate & Steps)

**Goal:** Display heart-rate (BPM), steps, and basic workout stats. Provide a Start/Stop workout control.

**TouchGFX Steps:**

1. Create `HealthView` with labels for BPM, Steps, Calories and a Start/Stop `Button`.
2. Optionally, add a small chart or progress bar to show recent heart-rate trend. Use `Container` + small plotting routine (or a prebuilt Chart widget if available).
3. Add a `Start Workout` button that triggers data collection in firmware.

**Firmware integration:**

* **Heart Rate (ADC-based):** Use ADC with DMA to collect PPG samples into a buffer. In the ADC conversion-complete callback, run a lightweight peak-detection routine and update BPM periodically.
* **Steps (IMU IMU):** Read accelerometer via I²C and compute magnitude; apply low-pass filter and peak detection to count steps.

**Example flow:**

```c
// Start workout
presenter->startWorkout();
// Firmware starts ADC DMA, and enables IMU interrupts or sampling.

// Periodic update (every 1s or 2s)
view.updateBpm(currentBpm);
view.updateSteps(stepCount);
```

**UI Updates:**

* Update text labels only when values change significantly to avoid constant redraw.
* For the chart, append new sample to a circular buffer and redraw only the chart container.

**Notes:**

* Use adaptive thresholds for peak detection to reduce false positives.
* Validate heart-rate with a reference device during tests.

---

### Part 4 — Voice-Control Automation

**Goal:** Provide a screen to show voice recognition status and last command; accept voice-triggered actions for smart-home.

**TouchGFX Steps:**

1. Create a `VoiceControlView` with a microphone icon, status label, and recent commands list.
2. Add an animated waveform or pulsing circle when voice capture is active (simple animation using TouchGFX animations).
3. Provide a `Start Listening` button which toggles recording mode and signals firmware to enable voice module.

**Firmware integration:**

* Voice recognition usually runs on an external module (e.g., dedicated ASR board) or on a connected smartphone. Use UART/BLE to send/receive commands.
* When firmware receives `VOICE_CMD:LIGHT_ON`, the presenter calls the appropriate toggle function (same as Light Control). Update the view with the recognized text.

**Example:**

```cpp
void VoiceView::onStartListeningPressed(const AbstractButton& b) {
    presenter->startVoiceRecognition();
}

// Presenter
void VoicePresenter::onVoiceCommandReceived(const char* cmd) {
    view.showRecognizedCommand(cmd);
    // map to action
    if (strcmp(cmd, "lights on") == 0) toggleLamp(1);
}
```

**Notes:**

* Keep UI feedback immediate (show "Listening..." and micro animations).
* Show last recognized command and confidence (if available).

---

### Part 5 — Wireless Communication (Smart-Home / Remote Control)

**Goal:** Show connectivity status and provide a way to send/receive remote commands.

**TouchGFX Steps:**

1. Create a `WirelessView` showing icons for BLE/Wi-Fi/UART signal and connection status text.
2. Add buttons to "Connect", "Disconnect", and to send simple pre-defined commands.

**Firmware integration:**

* Implement UART (or HCI for BLE) handlers. Use interrupt-driven UART or DMA for transfers.
* Provide a small command protocol: `CMD:LIGHT:1:ON` and an ACK response. Presenters parse responses and update the view.

**Example:**

```c
void WirelessPresenter::sendLightCommand(uint8_t id, bool on) {
    char buf[32];
    snprintf(buf, sizeof(buf), "CMD:LIGHT:%d:%s\r\n", id, on?"ON":"OFF");
    HAL_UART_Transmit_IT(&huart2, (uint8_t*)buf, strlen(buf));
}
```

**Notes:**

* Display connection state clearly and handle retries/timeouts.
* Consider using tiny command ACKs to ensure reliability.

---

## Integration patterns (View <-> Presenter <-> HAL)

1. **View (TouchGFX UI)**: Handles layout and touch events. Keeps updates lightweight.
2. **Presenter**: Glue layer. Receives UI events, requests data from firmware or issues commands, and calls view update functions.
3. **HAL / Drivers**: Actual hardware interaction (I2C, ADC, RTC, UART). Use flags, RTOS queues, or event callbacks to pass data back to presenter.

**Good practice:** Never call blocking HAL functions directly from the UI thread; call them from the presenter which can schedule work on lower-priority contexts.

---

## Performance & Memory Optimizations

* Use **DMA** for ADC and display transfers to keep CPU free.
* Use **partial invalidation** in TouchGFX to redraw only changing areas.
* Keep large bitmaps in external flash if SRAM is limited; stream via DMA when required.
* Avoid heap allocations during runtime; use static buffers and check linker map for memory usage.
* Reduce font sizes / color depth where possible to save RAM.

---

## Testing & Validation

* Unit-test individual drivers (I2C read/write, ADC sampling).
* Use a logic analyzer/oscilloscope to verify I2C/SPI timings and DMA transfers.
* Validate step counter and BPM against reference devices (phone or medical devices).
* Perform UI stress tests: rapid touch actions, screen rotations (if any), memory leak checks.

---

## Build & Flash (quick steps)

1. Generate TouchGFX code and open project in STM32CubeIDE.
2. Configure CubeMX for peripherals (I2C, ADC, UART, DMA, RTC).
3. Implement Presenter hooks to call HAL functions.
4. Build the project.
5. Flash using ST-Link and test.

**Optional:** Implement a DFU bootloader for OTA updates over BLE.

---

## Troubleshooting Checklist

* **Blank display:** Check display init sequence and SPI/parallel pins.
* **Slow UI / lag:** Ensure DMA is used and reduce framebuffer updates.
* **Wrong sensor values:** Check I2C addresses, pull-ups, and sensor initialization registers.
* **UART issues:** Verify baud rate, parity, and wiring.

---

## Future improvements

* Add BLE LE smartphone app for data sync and OTA.
* Add secure boot and encrypted OTA.
* Implement Kalman filter for improved sensor fusion.
* Tiny ML on-device for activity classification (run/walk/sit).

---

## License & Contact

This README is authored by the project maintainer (Kritika Sinha). For questions or help, contact `kritika.bengaluru@gmail.com` or view the GitHub repo linked on the resume.

---

*End of README — open the TouchGFX project, follow the steps above screen-by-screen, and reach out if you want sample code for any specific presenter or driver.*

 

