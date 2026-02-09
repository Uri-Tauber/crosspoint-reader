# Hardware Assumptions and Hardcoded Values

This document identifies places in the CrossPoint firmware that make assumptions about the hardware of the device (ESP32-C3, 16MB flash, 4.2" e-ink display 800*480).

## 1. Display Resolution and Aspect Ratio (480x800)
The firmware is optimized for a 4.2-inch 480x800 display. Assumptions are found in:
*   **`lib/Xtc/Xtc/XtcTypes.h`**: Hardcoded constants `DISPLAY_WIDTH = 480` and `DISPLAY_HEIGHT = 800`.
*   **`src/CrossPointSettings.h`**: The `ORIENTATION` enum (Portrait/Landscape) explicitly defines coordinates as 480x800 or 800x480.
*   **`lib/GfxRenderer/GfxRenderer.cpp`**: `rotateCoordinates()` contains hardcoded rotation logic specifically for a 480x800 panel. Methods like `getScreenWidth()` and `getScreenHeight()` return these hardcoded panel dimensions.
*   **`lib/JpegToBmpConverter/JpegToBmpConverter.cpp`**: Hardcodes `TARGET_MAX_WIDTH = 480` and `TARGET_MAX_HEIGHT = 800` for generating book cover thumbnails.
*   **`src/activities/network/CrossPointWebServerActivity.cpp`**: Uses hardcoded `480` in the formula to center QR codes: `(480 - 6 * 33) / 2`.
*   **`src/activities/network/CalibreConnectActivity.cpp`**: Uses hardcoded `480` to center progress bars: `(480 - barWidth) / 2`.
*   **`src/activities/reader/XtcReaderActivity.cpp`**: Explicitly handles pre-rendered page bitmaps that are assumed to be 480x800 pixels.

## 2. GPIO Pin Configuration (ESP32-C3 specific)
Hardware pins are hardcoded in the HAL layer:
*   **`lib/hal/HalGPIO.h`**:
    *   **E-ink SPI**: `EPD_SCLK 8`, `EPD_MOSI 10`, `EPD_CS 21`, `EPD_DC 4`, `EPD_RST 5`, `EPD_BUSY 6`.
    *   **Shared SPI MISO**: `SPI_MISO 7`.
    *   **Battery Monitoring**: `BAT_GPIO0 0`.
    *   **USB Detection**: `UART0_RXD 20` (Assumes High level on UART RX indicates USB power).
*   **`lib/hal/HalGPIO.cpp`**: Uses `InputManager::POWER_BUTTON_PIN` specifically for deep sleep wakeup triggers.

## 3. Memory and Flash Architecture
Optimizations for the ESP32-C3's memory constraints and 16MB flash:
*   **`platformio.ini`**: Hardcodes `board_upload.flash_size = 16MB` and `board_build.flash_size = 16MB`.
*   **`partitions.csv`**: Defines a fixed partition table utilizing 16MB, with large 0x640000 (6.25MB) OTA app slots.
*   **`lib/GfxRenderer/GfxRenderer.h`**: Implements framebuffer chunking (`BW_BUFFER_CHUNK_SIZE = 8000`) to mitigate heap fragmentation issues common on the ESP32-C3.
*   **`src/main.cpp`**: `verifyPowerButtonDuration()` includes a 1000ms delay/calibration loop specifically to account for slow GPIO state reporting on ESP32-C3 wake-up.

## 4. Grayscale and Rendering Assumptions
*   **`lib/GfxRenderer/GfxRenderer.h`**: The `Color` enum and dithering logic (`fillRectDither`) are mapped to a 4x4 Bayer matrix to simulate 16 gray levels on 1-bit hardware.
*   **`lib/hal/HalDisplay.h` & `lib/hal/HalDisplay.cpp`**: Grayscale methods (`copyGrayscaleLsbBuffers`, `copyGrayscaleMsbBuffers`) implement a 2-bit (4-level) native grayscale pipeline.
*   **`src/activities/reader/XtcReaderActivity.cpp`**: Hardcoded logic for 2-bit grayscale distribution and rendering passes.

## 5. UI Layout and Metrics
Many UI elements use hardcoded pixel offsets:
*   **`src/components/themes/lyra/LyraTheme.h`**: `LyraMetrics` contains absolute values like `homeCoverHeight = 226`, `headerHeight = 84`, and `homeTopPadding = 56`.
*   **`src/components/themes/BaseTheme.cpp`**:
    *   `drawButtonHints()` hardcodes button X-positions: `{25, 130, 245, 350}`.
    *   `drawSideButtonHints()` hardcodes `topButtonY = 345`.
    *   `drawPopup()` hardcodes a fixed vertical position `y = 60`.
