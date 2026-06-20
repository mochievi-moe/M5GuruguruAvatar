#pragma once

#include <LittleFS.h>
#include <M5Unified.h>

// ── 9-direction touch mode (original) ──────────────────────────
//   /dir0.png 〜 /dir8.png を LittleFS から読み込み、タッチ座標→9方向で切替
static constexpr int AVATAR_NUM_DIR = 9;
static constexpr int AVATAR_CENTER_DIR = AVATAR_NUM_DIR / 2;  // 4

// ── 25-direction grid mode (new, for sensor-bus driven) ────────
//   /A_r{0..4}c{0..4}.png  目開け 25枚
//   /D_r{0..4}c{0..4}.png  目閉じ 25枚 (blink表示用)
//   合計50枚、yaw→col(0左..4右)、pitch→row(0上..4下) でマッピング
static constexpr int AVATAR_GRID_ROWS = 5;
static constexpr int AVATAR_GRID_COLS = 5;
static constexpr int AVATAR_GRID_CELLS = AVATAR_GRID_ROWS * AVATAR_GRID_COLS;  // 25
static constexpr int AVATAR_GRID_CENTER_ROW = 2;
static constexpr int AVATAR_GRID_CENTER_COL = 2;

class M5GuruguruAvatar {
public:
  M5GuruguruAvatar() = default;
  ~M5GuruguruAvatar();

  // ── original: 9-dir touch mode ───────────────────────────────
  bool init(int imgWidth = 251, int imgHeight = 240);
  void trackFace(int touchX, int touchY);

  // ── new: 25-grid sensor-bus driven mode with blink ───────────
  // A シート + D シート (合計50枚) を LittleFS から読み込み、内部スプライト常駐
  bool initGrid(int imgWidth = 251, int imgHeight = 240);

  // 顔の向きを (row, col) で直接指定。row=0(上)..4(下), col=0(左)..4(右)
  void setPose(int row, int col);

  // 表情シート切替。false=A(目開け)、true=D(目閉じ)
  void setBlink(bool eyesClosed);

  // yaw/pitch (deg, -180..+180) から自動マッピングして setPose 呼び出し
  // maxRange はマッピング全幅(±maxRange/2 で col 0..4 / row 0..4 にスケール)
  void updateFromYawPitch(float yawDeg, float pitchDeg, float maxRange = 60.0f);

  // タッチ位置 (display座標) を 5x5 grid に投影して setPose 呼び出し
  void trackFaceGrid(int touchX, int touchY);

  void end();

private:
  enum class Mode { Touch9, Grid25 };

  static void drawTask9(void* arg);
  static void drawTaskGrid(void* arg);
  int getDirection(int touchX, int touchY) const;

  int _imgWidth  = 251;
  int _imgHeight = 240;
  Mode _mode = Mode::Touch9;

  // Touch9 mode
  int _currentDir = AVATAR_CENTER_DIR;
  M5Canvas* _canvas[AVATAR_NUM_DIR] = {};

  // Grid25 mode
  int _currentRow = AVATAR_GRID_CENTER_ROW;
  int _currentCol = AVATAR_GRID_CENTER_COL;
  bool _blink = false;
  M5Canvas* _canvasA[AVATAR_GRID_CELLS] = {};  // 目開け 25
  M5Canvas* _canvasD[AVATAR_GRID_CELLS] = {};  // 目閉じ 25

  TaskHandle_t _taskHandle = nullptr;
  bool _running = false;
};
