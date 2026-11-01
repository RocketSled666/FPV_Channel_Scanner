/*
 * FPV Channel Scanner - Config.h
 * Revision: 46
 *
 * Hardware pins, tuning constants, shared types, frequency table, colors
 * and the undocumented ESP32-C5 radio access used by the scanner.
 */

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include "esp_wifi.h"

// ---------------------------------------------------------------- types (keep above all functions)
struct RGB { uint8_t r, g, b; };
// Pages. MODE_GRID is "FPV Channel Activity", MODE_ACTIVITY is "FPV Channel
// Noise", MODE_QUIET is "Quietest Channels", MODE_HDZERO is "Quietest for
// HDZero". Order is set by pageAfter().
enum ScreenMode { MODE_GRID = 0, MODE_SPECTRUM = 1, MODE_ACTIVITY = 2, MODE_QUIET = 3,
                  MODE_HDZERO = 4 };
struct StallStat {        // frequencies whose measurement timed out
  uint16_t mhz;
  uint32_t count;
  float    lastFallbackDb;
};
// User settings, edited in the Configuration menu (KEY_L + KEY_R held) and kept in flash
// (Settings.cpp). Everything is an int so the menu can treat them alike:
// toggles are 0/1, fractional values are stored scaled (see each field).
struct Settings {
  int span;          // dB above floor = full brightness / full graph height
  int learnSweeps;   // sweeps spent learning the noise floor (grid and spectrum)
  int gridSamples;   // measurements averaged per grid channel (~3.2 ms each)
  int specStep;      // spectrum step, MHz
  int specSamples;   // measurements averaged per spectrum point
  int peakDecay10;   // peak-hold fall, tenths of a pixel per sweep
  int busyDb;        // noise page: reading this far above floor counts as busy
  int gamma100;      // noise page shading curve, hundredths (55 = 0.55)
  int redSpread10;   // noise page: quietest-to-busiest spread for full red, tenths of dB
  int quietMinS;     // noise history (s) needed before the Quietest page opens
  int hdzero;        // HDZero features on/off
  int hdzLowBand;    // HDZero lists/outlines include L1-L8
  int hdzE1F1;       // HDZero lists/outlines include E1 and F1
  int backlight;     // LCD backlight, %
};

// One line in the Configuration menu (Settings.cpp)
struct SettingDef {
  const char *page;     // menu page = title of the screen it configures
  const char *name;     // label shown in the list
  const char *key;      // flash key (max 15 chars)
  int *value;           // field in cfg
  int  minV, maxV, step;
  int  def;             // default value
  uint8_t isToggle;     // 1 = On/Off, Enter flips it
  uint8_t decimals;     // stored scaled by 10^decimals
  const char *unit;     // shown after the value ("" for none)
  const char *help;     // one-line description
};

struct ButtonState {
  uint8_t  pin;
  bool     down;
  uint32_t downSinceMs;
  bool     longFired;
};

// ---------------------------------------------------------------- pins
#define PIN_LCD_SCK   6
#define PIN_LCD_MOSI  7
#define PIN_LCD_MISO  5
#define PIN_LCD_DC    4
#define PIN_LCD_CS    8
#define PIN_SD_CS     9     // shares the SPI bus, keep deselected
#define PIN_I2C_SDA   27
#define PIN_I2C_SCL   26
#define PIN_KEY_L     2     // silkscreen KEY_L (Waveshare docs: PREV)
#define PIN_KEY_R     3     // silkscreen KEY_R (Waveshare docs: NEXT)
#define PIN_BTN_ENTER 28

#define EXIO_ADDR       0x24  // CH32V003 IO expander
#define EXIO_REG_MODE   0x02
#define EXIO_REG_OUT    0x03
#define EXIO_REG_PWM    0x05
#define EXIO_PIN_LCDRST 0

#define LCD_W 320
#define LCD_H 320
#define LCD_SPI_HZ 40000000

// ---------------------------------------------------------------- tuning knobs
#define LONG_PRESS_MS     700
#define CONFIG_HOLD_MS    5000    // hold KEY_L + KEY_R this long to open the Configuration menu
#define SPLASH_MS         5000    // start-up splash screen time
#define SPLASH_COMBO_MS   80      // KEY_L + KEY_R held this long on the splash = Configuration

// The values below come from cfg (Settings.cpp), so they can be changed in
// the Configuration menu. Defaults are in the settings table.
#define SPEC_STEP_MHZ     (cfg.specStep)            // spectrum resolution (receiver is 20 MHz wide)
#define SPEC_MARGIN_MHZ   20      // extra span beyond the outer channels
#define SPEC_MAX_PTS      200
#define ACT_BUSY_DB       ((float)cfg.busyDb)       // noise page: this far above floor counts as busy
#define ACT_MAX_DT_S      1.0f    // longest gap counted (e.g. after leaving the page)
#define ACT_MIN_SCALE_DB  (cfg.redSpread10 / 10.0f) // quietest-to-busiest spread needed for full red (dB)
#define QUIET_LIST_N      16      // channels listed on the QUIETEST CHANNELS page
#define QUIET_MIN_HISTORY_S (cfg.quietMinS)        // noise history needed before Quietest can open
#define POPUP_MS          1500    // how long the "Wait >60s" pop-up stays up
#define POPUP_W           220     // pop-up box, centred horizontally
#define POPUP_H           56
#define POPUP_X           ((LCD_W - POPUP_W) / 2)
#define POPUP_Y           (GRID_Y + 90)
#define ACT_GAMMA         (cfg.gamma100 / 100.0f)   // shading curve: <1 brightens small differences
#define PEAK_DECAY_PX     (cfg.peakDecay10 / 10.0f) // peak-hold dots fall this many pixels per sweep
#define SPEC_SAMPLES      (cfg.specSamples)         // measurements averaged per spectrum point
#define GRID_SAMPLES      (cfg.gridSamples)         // measurements averaged per grid channel
#define SETTLE_US         100     // wait after retune before measuring
#define MEAS_TIMEOUT_US   5000    // a measurement normally takes ~3.2 ms
#define MAX_STALL_STATS   16
#define GRID_LEARN_SWEEPS (cfg.learnSweeps)       // sweeps spent learning the noise floor
#define SPEC_LEARN_SWEEPS (cfg.learnSweeps)

// ---------------------------------------------------------------- FPV table
#define NUM_BANDS 6
#define NUM_CH    8
static const char BAND_NAME[NUM_BANDS] = { 'A', 'B', 'E', 'F', 'R', 'L' };
static const char * const BAND_LONG[NUM_BANDS] = { "Boscam A", "Boscam B", "Boscam E",
                                            "Fatshark", "Raceband", "Lowband" };
// Channels an HDZero VTX can be set to (same band order as FPV_MHZ). The
// settings can drop L1-L8 and E1/F1 from this; use hdzeroOk(), not the table.
// Channels an HDZero VTX can be set to:
// R1-R8, E1, F1, F2, F4, plus L1-L8 once low band is unlocked on the VTX.
// Source: HDZero docs, VTX "Getting Started" (docs.hd-zero.com/vtx-general):
// "All VTXes support R1-R8, E1, F1, F2 and F4 channels. You will get an
// additional 8 channels L1-L8 if you unlock the low band."
static const bool HDZERO_OK[NUM_BANDS][NUM_CH] = {
  { 0, 0, 0, 0, 0, 0, 0, 0 },  // A  none
  { 0, 0, 0, 0, 0, 0, 0, 0 },  // B  none
  { 1, 0, 0, 0, 0, 0, 0, 0 },  // E  E1
  { 1, 1, 0, 1, 0, 0, 0, 0 },  // F  F1 F2 F4
  { 1, 1, 1, 1, 1, 1, 1, 1 },  // R  R1-R8
  { 1, 1, 1, 1, 1, 1, 1, 1 },  // L  L1-L8 (low band unlocked)
};

static const uint16_t FPV_MHZ[NUM_BANDS][NUM_CH] = {
  { 5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725 },  // A  Boscam A
  { 5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866 },  // B  Boscam B
  { 5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945 },  // E  Boscam E
  { 5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880 },  // F  Fatshark / ImmersionRC
  { 5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917 },  // R  Raceband
  { 5362, 5399, 5436, 5473, 5510, 5547, 5584, 5621 },  // L  Lowband
};

// ---------------------------------------------------------------- PHY access
// Internal ESP32-C5 libphy function (no public header).
extern "C" int phy_set_chanfreq(uint32_t chan_or_mhz, uint8_t bw40);
// Receiver signal strength in dB: noise-floor register / 4 + AGC RSSI field
// (from disassembly of libphy). Used only when the estimator stalls.
extern "C" int16_t phy_get_rssi(void);

// Baseband noise-floor estimator (from libphy check_noise_floor()).
#define BB_NF_CTRL   (*(volatile uint32_t *)0x600A7018)
#define BB_NF_VALUE  (*(volatile uint32_t *)0x600A7050)
#define NF_EN_BIT    (1u << 23)
#define NF_DONE_BIT  (1u << 24)
#define NF_START_BIT (1u << 25)

// ---------------------------------------------------------------- layout
// grid
#define TITLE_H   24
#define HDR_Y     28
#define GRID_X    34
#define GRID_Y    46
#define CELL_W    34
#define CELL_H    38
#define CELL_GAP  2
#define FOOT_Y    (GRID_Y + NUM_BANDS * (CELL_H + CELL_GAP) + 2)
// spectrum
#define GX        30                 // graph left
#define GY        28                 // graph top
#define GW        286                // graph width  (px)
#define GH        200                // graph height (px)
#define CH_LBL_Y  (GY + GH + 4)      // channel numbers under the graph
#define CH_VAL_Y  (CH_LBL_Y + 10)    // channel dB values under the numbers
#define BTN_Y     252
#define BTN_H     30
#define BTN_W     48
#define BTN_GAP   5
#define SPEC_FOOT_Y 290

// ---------------------------------------------------------------- colors
static const RGB C_BG    = {   0,   0,   0 };
static const RGB C_TEXT  = { 230, 230, 230 };
static const RGB C_DIM   = { 120, 120, 120 };
static const RGB C_ACC   = { 255, 180,   0 };
static const RGB C_HOT   = {  60, 255, 120 };  // full-brightness signal color
static const RGB C_MARK  = {  40,  40,  55 };  // channel marker lines in the graph
static const RGB C_PEAKH = { 255, 255, 255 };  // peak-hold trace
static const RGB C_RED   = { 255,   0,   0 };  // full-intensity activity color
static const RGB C_RANK  = { 160, 160, 160 };  // rank numbers on the noise page
static const RGB C_HDZ   = {  40, 120, 255 };  // outline: channel an HDZero VTX can use

// ---------------------------------------------------------------- layout / UI constants
// (moved here from the page code when the sketch was split into files)
#define SCAN_DOT_MS 400                // dot animation step
#define LEARN_BAR_W 240
#define QUIET_Y0     34      // first row
#define QUIET_PITCH  31      // row spacing
#define QUIET_COL_W  158
