#include "M5GuruguruAvatar.h"
#include <math.h>

// ── original 9-dir mode ──────────────────────────────────────────
bool M5GuruguruAvatar::init(int imgWidth, int imgHeight) {
  _imgWidth   = imgWidth;
  _imgHeight  = imgHeight;
  _currentDir = AVATAR_CENTER_DIR;
  _mode       = Mode::Touch9;

  if (!LittleFS.begin(true)) {
    Serial.println("[Avatar] LittleFS mount failed");
    return false;
  }

  for (int i = 0; i < AVATAR_NUM_DIR; i++) {
    _canvas[i] = new M5Canvas(&M5.Display);
    _canvas[i]->setColorDepth(16);
    _canvas[i]->createSprite(_imgWidth, _imgHeight);

    String path = String("/dir") + i + ".png";
    if (_canvas[i]->drawPngFile(LittleFS, path.c_str())) {
      Serial.printf("[Avatar] Loaded: %s\n", path.c_str());
    } else {
      Serial.printf("[Avatar] Missing: %s  (placeholder)\n", path.c_str());
      _canvas[i]->fillSprite(TFT_BLACK);
      _canvas[i]->setTextColor(TFT_WHITE, TFT_BLACK);
      _canvas[i]->drawCenterString(String("Dir ") + i, _imgWidth / 2, _imgHeight / 2);
    }
  }

  _running = true;
  xTaskCreatePinnedToCore(drawTask9, "AvatarDraw9", 4096, this, 1, &_taskHandle, 1);
  return true;
}

void M5GuruguruAvatar::trackFace(int touchX, int touchY) {
  _currentDir = getDirection(touchX, touchY);
}

// ── new grid25 mode (sensor-bus driven, with blink) ──────────────
bool M5GuruguruAvatar::initGrid(int imgWidth, int imgHeight) {
  _imgWidth     = imgWidth;
  _imgHeight    = imgHeight;
  _currentRow   = AVATAR_GRID_CENTER_ROW;
  _currentCol   = AVATAR_GRID_CENTER_COL;
  _blink        = false;
  _mode         = Mode::Grid25;

  if (!LittleFS.begin(true)) {
    Serial.println("[Avatar] LittleFS mount failed");
    return false;
  }

  Serial.printf("[Avatar] initGrid: loading 50 sprites (PSRAM free=%d)\n", ESP.getFreePsram());

  int loaded = 0;
  for (int r = 0; r < AVATAR_GRID_ROWS; r++) {
    for (int c = 0; c < AVATAR_GRID_COLS; c++) {
      int idx = r * AVATAR_GRID_COLS + c;

      // A シート (目開け)
      _canvasA[idx] = new M5Canvas(&M5.Display);
      _canvasA[idx]->setColorDepth(16);
      _canvasA[idx]->createSprite(_imgWidth, _imgHeight);
      String pathA = String("/A_r") + r + "c" + c + ".png";
      if (_canvasA[idx]->drawPngFile(LittleFS, pathA.c_str())) {
        loaded++;
      } else {
        Serial.printf("[Avatar] Missing: %s\n", pathA.c_str());
        _canvasA[idx]->fillSprite(TFT_BLACK);
      }

      // D シート (目閉じ)
      _canvasD[idx] = new M5Canvas(&M5.Display);
      _canvasD[idx]->setColorDepth(16);
      _canvasD[idx]->createSprite(_imgWidth, _imgHeight);
      String pathD = String("/D_r") + r + "c" + c + ".png";
      if (_canvasD[idx]->drawPngFile(LittleFS, pathD.c_str())) {
        loaded++;
      } else {
        Serial.printf("[Avatar] Missing: %s\n", pathD.c_str());
        _canvasD[idx]->fillSprite(TFT_BLACK);
      }
    }
  }
  Serial.printf("[Avatar] initGrid done: %d/50 loaded, PSRAM free=%d\n", loaded, ESP.getFreePsram());

  _running = true;
  xTaskCreatePinnedToCore(drawTaskGrid, "AvatarGrid", 4096, this, 1, &_taskHandle, 1);
  return true;
}

void M5GuruguruAvatar::setPose(int row, int col) {
  if (row < 0) row = 0; else if (row >= AVATAR_GRID_ROWS) row = AVATAR_GRID_ROWS - 1;
  if (col < 0) col = 0; else if (col >= AVATAR_GRID_COLS) col = AVATAR_GRID_COLS - 1;
  _currentRow = row;
  _currentCol = col;
}

void M5GuruguruAvatar::setBlink(bool eyesClosed) {
  _blink = eyesClosed;
}

void M5GuruguruAvatar::trackFaceGrid(int touchX, int touchY) {
  int w = M5.Display.width();
  int h = M5.Display.height();
  if (w <= 0 || h <= 0) return;
  int col = (touchX * AVATAR_GRID_COLS) / w;
  int row = (touchY * AVATAR_GRID_ROWS) / h;
  setPose(row, col);
}

void M5GuruguruAvatar::updateFromYawPitch(float yawDeg, float pitchDeg, float maxRange) {
  // -maxRange/2 ..+maxRange/2 を 0..4 にスケール (中央 maxRange/2 = col 2)
  float halfRange = maxRange * 0.5f;
  float yawNorm = (yawDeg + halfRange) / maxRange;   // 0..1
  float pitchNorm = (pitchDeg + halfRange) / maxRange;
  int col = (int)roundf(yawNorm * (AVATAR_GRID_COLS - 1));
  int row = (int)roundf(pitchNorm * (AVATAR_GRID_ROWS - 1));
  setPose(row, col);
}

// ── teardown ─────────────────────────────────────────────────────
void M5GuruguruAvatar::end() {
  _running = false;
  if (_taskHandle) {
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
  }
  for (int i = 0; i < AVATAR_NUM_DIR; i++) {
    delete _canvas[i];  _canvas[i] = nullptr;
  }
  for (int i = 0; i < AVATAR_GRID_CELLS; i++) {
    delete _canvasA[i]; _canvasA[i] = nullptr;
    delete _canvasD[i]; _canvasD[i] = nullptr;
  }
}

M5GuruguruAvatar::~M5GuruguruAvatar() {
  end();
}

// ── draw tasks ───────────────────────────────────────────────────
void M5GuruguruAvatar::drawTask9(void* arg) {
  auto* self = static_cast<M5GuruguruAvatar*>(arg);
  while (self->_running) {
    float zx = (float)M5.Display.width()  / self->_imgWidth;
    float zy = (float)M5.Display.height() / self->_imgHeight;
    M5.Display.startWrite();
    self->_canvas[self->_currentDir]->pushRotateZoom(
      M5.Display.width()  / 2,
      M5.Display.height() / 2,
      0.0f, zx, zy
    );
    M5.Display.endWrite();
    vTaskDelay(pdMS_TO_TICKS(33));  // ~30 fps
  }
  vTaskDelete(nullptr);
}

void M5GuruguruAvatar::drawTaskGrid(void* arg) {
  auto* self = static_cast<M5GuruguruAvatar*>(arg);
  while (self->_running) {
    int idx = self->_currentRow * AVATAR_GRID_COLS + self->_currentCol;
    M5Canvas* sprite = self->_blink ? self->_canvasD[idx] : self->_canvasA[idx];
    if (sprite) {
      float zx = (float)M5.Display.width()  / self->_imgWidth;
      float zy = (float)M5.Display.height() / self->_imgHeight;
      M5.Display.startWrite();
      sprite->pushRotateZoom(
        M5.Display.width()  / 2,
        M5.Display.height() / 2,
        0.0f, zx, zy
      );
      M5.Display.endWrite();
    }
    vTaskDelay(pdMS_TO_TICKS(33));  // ~30 fps
  }
  vTaskDelete(nullptr);
}

int M5GuruguruAvatar::getDirection(int touchX, int touchY) const {
  int cx = M5.Display.width()  / 2;
  int cy = M5.Display.height() / 2;
  float dx = touchX - cx;
  float dy = touchY - cy;

  if (sqrtf(dx * dx + dy * dy) < 50) return AVATAR_CENTER_DIR;

  float angle = atan2f(dy, dx) * 180.0f / M_PI;

  if (angle < -157.5f || angle > 157.5f) return 3;  // Left
  else if (angle < -112.5f)              return 0;  // Upper-Left
  else if (angle < -67.5f)               return 1;  // Up
  else if (angle < -22.5f)               return 2;  // Upper-Right
  else if (angle < 22.5f)                return 5;  // Right
  else if (angle < 67.5f)                return 8;  // Lower-Right
  else if (angle < 112.5f)               return 7;  // Down
  else if (angle < 157.5f)               return 6;  // Lower-Left
  return 3;
}
