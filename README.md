# M5GuruguruAvatar
A cute animated character library that follows your finger (touch) on the M5Stack StopWatch's round AMOLED display.
- 9-direction head tracking

Inspired by @rotejin's tomari-guruguru project.

## Demo Video

https://x.com/nananauno/status/2067257510378274823

## Requirements
- M5Unified

## Tested Devices
- [M5Stack StopWatch](https://shop.m5stack.com/products/m5stack-stopwatch-dev-kit-esp32-s3) (ESP32-S3 + 1.75" round AMOLED touch)

## How to Use

### Preparing Character Images
Image preparation: Please refer to @rotejin's [tomari-guruguru](https://github.com/rotejin/tomari-guruguru) repository for how to create the sprite sheets.

1. Create a sprite sheet.
2. See [here](https://github.com/nananauno/M5-guruguru/tree/main/tools) and open `tools/sprite-splitter.html` in your browser.
3. Upload your sprite sheet and adjust the grid settings.
4. Click **Split Sprites** to generate individual images.
5. Download the sliced images and rename them as `dir0.png` to `dir8.png`.

*Image mapping:*
- `dir0`: Top-Left
- `dir1`: Top
- `dir2`: Top-Right
- `dir3`: Left
- `dir4`: Center (Front) ← Default
- `dir5`: Right
- `dir6`: Bottom-Left
- `dir7`: Bottom
- `dir8`: Bottom-Right

### Flashing to Device
1. Place your character images (`dir0.png` to `dir8.png`) in the `data/` folder of the project.
2. Upload the firmware **and** the LittleFS filesystem (images) to your M5StopWatch.
   - **PlatformIO**: `Upload` and then `Upload Filesystem Image`
   - **Arduino IDE**: Use the [Arduino LittleFS Uploader](https://github.com/earlephilhower/arduino-littlefs-upload) plugin
3. Power on the device and touch the screen — the character will follow your finger!

### Sample code

```cpp
#include <M5Unified.h>
#include <M5GuruguruAvatar.h>

M5GuruguruAvatar avatar;

void setup() {
  M5.begin();
  avatar.init(251, 240);  // imgWidth=251px, imgHeight=240px — adjust to your images
}

void loop() {
  M5.update();
  if (M5.Touch.getCount() > 0) {
    auto p = M5.Touch.getDetail(0);
    if (p.isPressed()) avatar.trackFace(p.x, p.y);
  }
}
```

### platformio.ini

```ini
[env]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
framework = arduino

[env:m5stack-stopwatch]
board = esp32s3box
board_build.partitions = default_16MB.csv
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216
board_build.filesystem = littlefs
board_build.arduino.memory_type = qio_opi
monitor_speed = 115200
build_flags =
    -DESP32S3
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=5
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    M5Unified = https://github.com/m5stack/M5Unified
    https://github.com/nananauno/M5GuruguruAvatar.git
```

### Grid mode (25 directions + blink)

For sensor-driven use (head tracking, gyro, etc.) the library can also run in a
**5×5 grid mode with blink frames**. Instead of 9 directional sprites it loads
50 — two expression sheets of 25 cells each — and exposes APIs to drive the
pose by `(row, col)`, by yaw/pitch, or by touch.

**Image set** (place in `data/`):

- `A_r0c0.png` ... `A_r4c4.png` — eyes open (5×5 grid)
- `D_r0c0.png` ... `D_r4c4.png` — eyes closed (used for blink)

`r` is row (`0` = looking up, `4` = looking down), `c` is column (`0` = left,
`4` = right). The cell sprites can be generated from a 5×5 sheet such as the
one in [tomari-guruguru](https://github.com/rotejin/tomari-guruguru) (its
`slice_character_sheets.py` produces the matching `r*c*.webp` set; convert to
PNG and rename to the pattern above).

```cpp
#include <M5Unified.h>
#include <M5GuruguruAvatar.h>

M5GuruguruAvatar avatar;

void setup() {
  M5.begin();
  avatar.initGrid(251, 240);   // load 50 sprites instead of 9
}

void loop() {
  M5.update();

  // (a) drive by touch on the M5 itself
  if (M5.Touch.getCount() > 0) {
    auto p = M5.Touch.getDetail(0);
    if (p.isPressed()) avatar.trackFaceGrid(p.x, p.y);
  }

  // (b) or feed external yaw/pitch (e.g. AirPods CMHeadphoneMotion via WebSocket)
  // avatar.updateFromYawPitch(yawDeg, pitchDeg, 60.0f);

  // (c) blink — drive from your own timer (180 ms is a comfortable duration)
  // avatar.setBlink(true);   // switch to D-sheet (eyes closed)
  // ...
  // avatar.setBlink(false);  // back to A-sheet (eyes open)
}
```

> Wiring the avatar to an actual sensor source (BLE, WebSocket, IMU, …) is out
> of scope of this library — bring your own input pipeline and just call
> `setPose` / `updateFromYawPitch` / `setBlink` from your sketch.

**Partition note**: 50 PNGs at the default image size easily exceed the LittleFS
slot in `default_16MB.csv`. If `Upload Filesystem Image` complains with
`LFS_ERR_NOSPC`, drop OTA and give LittleFS a bigger slice — for example:

```csv
# partitions_custom.csv (16MB flash, single app, large LittleFS)
nvs,      data, nvs,     0x9000,   0x6000,
app0,     app,  factory, 0x10000,  0x600000,
spiffs,   data, spiffs,  0x610000, 0x9F0000,
```

and point `board_build.partitions = partitions_custom.csv` in `platformio.ini`.

## API

| Method | Description |
|--------|-------------|
| `bool init(int imgWidth, int imgHeight)` | 9-direction touch mode. Mount LittleFS, load `dir0..dir8.png`, and start the internal draw task. Returns `false` if LittleFS fails to mount. |
| `void trackFace(int x, int y)` | (9-dir mode) Update face direction from a touch coordinate. |
| `bool initGrid(int imgWidth, int imgHeight)` | 5×5 grid mode + blink. Load 50 sprites (`A_r*c*.png` + `D_r*c*.png`) and start the draw task. |
| `void setPose(int row, int col)` | (grid mode) Set the face direction directly. `row` 0..4 = up..down, `col` 0..4 = left..right. |
| `void setBlink(bool eyesClosed)` | (grid mode) `true` shows the D-sheet (eyes closed), `false` shows the A-sheet. |
| `void updateFromYawPitch(float yawDeg, float pitchDeg, float maxRange = 60.0f)` | (grid mode) Map a yaw/pitch pair to the 5×5 grid. The `±maxRange/2` range maps onto `0..4`; tune `maxRange` to your sensor's natural span. |
| `void trackFaceGrid(int touchX, int touchY)` | (grid mode) Project a touch coordinate onto the 5×5 grid. |
| `void end()` | Stop the draw task and free all resources. Called automatically by the destructor. |

## License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

Character assets (images) follow their own license. Please respect the original creators.

## Acknowledgments

- Huge thanks to [@rotejin](https://github.com/rotejin) for the wonderful `tomari-guruguru` idea and open resources!
- M5Stack team for the amazing StopWatch hardware.
- [@lovyan03](https://github.com/lovyan03) for the [M5Unified](https://github.com/m5stack/M5Unified) library.