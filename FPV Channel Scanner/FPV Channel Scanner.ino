/*
 * FPV Channel Scanner
 * Revision: 33
 * Board : Waveshare ESP32-C5-LCD-2.73 (ESP32-C5, ILI9488 320x320 SPI LCD)
 * Core  : Arduino-ESP32 3.3.x  (Tools > Board > "ESP32C5 Dev Module")
 *
 * Uses the ESP32-C5's own 5 GHz Wi-Fi radio as an energy detector:
 *   1. Wi-Fi is started only to power up and calibrate the 5 GHz RF path.
 *   2. The RF synthesizer is tuned directly to a frequency in MHz with the PHY
 *      library's internal phy_set_chanfreq() (the same call the Wi-Fi driver
 *      uses; values > 14 are taken as MHz).
 *   3. The baseband's hardware noise-floor estimator measures the in-band
 *      power (20 MHz bandwidth) - no Wi-Fi packets are needed, so analog FPV
 *      video carriers show up.
 *
 * phy_set_chanfreq() and the baseband registers are UNDOCUMENTED and were
 * found by disassembling Espressif's libphy.a for the ESP32-C5. They may
 * change between core versions. Readings are relative, not calibrated dBm.
 *
 * Four pages, in KEY_R order:
 *   FPV CHANNEL ACTIVITY (grid) - all 48 FPV channels, square brightness =
 *               power above floor right now
 *   SPECTRUM  - continuous sweep across one band in 4 MHz steps
 *   FPV CHANNEL NOISE - same layout as the grid; each box = its signal above
 *               the floor averaged over all time since start or the last
 *               reset, shaded from the quietest channel (black) to the
 *               busiest (bright red); the quietest is outlined and named
 *   QUIETEST CHANNELS - the 16 quietest channels from the Channel Noise
 *               history, in order (1 = quietest), with average dB and busy %.
 *               Reached only by KEY_R from Channel Noise, once there is at
 *               least 60 s of history; before that a "Wait longer" pop-up
 *               shows and the page doesn't change.
 *
 *   KEY_R: Activity -> Spectrum -> Noise -> Quietest -> Activity
 *   KEY_L: Activity -> Noise -> Spectrum -> Activity;  Quietest -> Noise
 *
 * Buttons:
 *   KEY_L / KEY_R (GPIO2 / GPIO3) = previous / next page (order above)
 *   ENTER (short) on GRID     = cycle the span (dB)
 *   ENTER (short) on NOISE or QUIETEST = clear the noise history
 *   ENTER (short) on SPECTRUM = select the next band button under the graph
 *   ENTER (hold 0.7 s)        = re-learn the noise floor (do it with VTXs off)
 *
 * Arduino IDE note: all custom types are declared before the first function
 * so the IDE's auto-generated prototypes compile, and no function uses
 * default arguments.
 *
 * Revision history:
 *   1  Initial grid scanner
 *   2  Fix IDE auto-prototype error (RGB type moved up, no default args)
 *   3  Files renamed with spaces
 *   4  Spectrum sweep page with band buttons; accelerometer tap to switch
 *   5  KEY_L / KEY_R switch pages; ENTER selects band / span / re-learn
 *   6  Spectrum scale fixed at 0-15 dB; include font5x7.h
 *   7  Learning cut to 4 sweeps; timing breakdown on serial
 *   8  Revision number added to header
 *   9  Speed: 1 measurement per point (each is a fixed ~3.2 ms hardware
 *      window), no discarded sample, 4 MHz spectrum steps
 *  10  Spectrum floor seeded from the learned grid floor (interpolated), so
 *      the trace shows at once; LEARNING still shown while it refines
 *  11  Measurement timeout 10 -> 5 ms; a stalled measurement falls back to
 *      the receiver RSSI register; per-frequency stall report on serial;
 *      spectrum shows each channel's dB under its number; '+' dropped
 *  12  Spectrum graph height follows the grid's span setting (default 15 dB)
 *  13  Learning shows a 0-100% readout and progress bar in the footer
 *  14  SPAN label on the spectrum page in the same spot as the grid page,
 *      and kept visible on both pages while learning; removed the
 *      "ENTER: BAND" hint from the spectrum page; USB serial can no longer
 *      slow the scanner when nothing on the PC is reading it (non-blocking
 *      writes, 4 KB buffer, periodic dumps skipped when the buffer is full)
 *  15  Spectrum bands use the grid's learned floor directly - no second
 *      learning pass. Re-learning the grid also refreshes the spectrum.
 *  16  Spectrum band buttons ordered by increasing start frequency
 *      (L E R A B F); ENTER steps through them left to right
 *  17  Peak-hold trace falls 3x faster (0.6 -> 1.8 px per sweep)
 *  18  "NO SIGNAL" replaced by "SCANNING" with animated . .. ... dots
 *      (only the dots are redrawn)
 *  19  Grid rows use the same band order as the spectrum buttons (L E R A B F)
 *  20  ACTIVITY page: per-box moving sum of signal above the floor, minus the
 *      average over all boxes, shown black -> red. KEY_R / KEY_L step
 *      forward / back through GRID -> ACTIVITY -> SPECTRUM
 *  21  ACTIVITY highlights and names the quietest channel instead of the
 *      busiest; averaging window is 60 seconds (time-based, not sweeps)
 *  22  ACTIVITY averaging window shortened to 15 seconds
 *  23  Noise page tracks each channel's average signal over time and shades
 *      quietest (black) to busiest (bright red); no average subtraction.
 *      Page titles: "FPV Channel Activity" (grid), "FPV Channel Noise"
 *  24  Noise page: noisy channels rise in ~15 s but fade over ~60 s, so a
 *      busy channel stays red after the signal stops
 *  25  Noise page keeps long-term statistics for every channel (average dB
 *      above floor, % of time busy, peak) since start / last reset; shading
 *      and the quietest pick use the long-term average. Footer shows how
 *      much history has been collected
 *  26  Noise page shading uses a gain curve (gamma 0.4) so small differences
 *      between channels show as clearly brighter red; full-red spread 0.5 dB
 *  27  Noise page ranks all 48 channels from quietest (1) to busiest (48)
 *      and shows the rank inside each box, above the frequency
 *  28  ACT_GAMMA 0.4 -> 0.55
 *  29  Rank numbers 1.5x size (was 2x) and light grey instead of white
 *  30  New QUIETEST CHANNELS page after Channel Noise: the 16 quietest
 *      channels from the noise history, listed quietest first
 *  31  Page order Activity -> Spectrum -> Noise -> Quietest. Quietest is
 *      reached only by KEY_R from Noise, and only after 60 s of noise
 *      history (otherwise a "WAIT LONGER" pop-up). KEY_L from Activity
 *      goes to Noise
 *  32  Pop-up area is cleared to black before the grid is redrawn, so no
 *      yellow border is left in the gaps between boxes
 *  33  Lowercase letters added to font5x7.h; all on-screen text uses normal
 *      capitalization ("dB", "MHz", "Scanning", ...). Band/channel codes such
 *      as "R4" stay in capitals. Band order re-checked on every page
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "font5x7.h"

// ---------------------------------------------------------------- types (keep above all functions)
struct RGB { uint8_t r, g, b; };
// Pages. MODE_GRID is "FPV Channel Activity", MODE_ACTIVITY is "FPV Channel
// Noise", MODE_QUIET is "Quietest Channels". Order is set by pageAfter().
enum ScreenMode { MODE_GRID = 0, MODE_SPECTRUM = 1, MODE_ACTIVITY = 2, MODE_QUIET = 3 };
struct StallStat {        // frequencies whose measurement timed out
  uint16_t mhz;
  uint32_t count;
  float    lastFallbackDb;
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

#define SPEC_STEP_MHZ     4       // spectrum resolution (receiver is 20 MHz wide)
#define SPEC_MARGIN_MHZ   20      // extra span beyond the outer channels
#define SPEC_MAX_PTS      200
#define ACT_BUSY_DB       3.0f    // noise page: a reading this far above floor counts as busy
#define ACT_MAX_DT_S      1.0f    // longest gap counted (e.g. after leaving the page)
#define ACT_MIN_SCALE_DB  0.5f    // quietest-to-busiest spread needed for full red (dB)
#define QUIET_LIST_N      16      // channels listed on the QUIETEST CHANNELS page
#define QUIET_MIN_HISTORY_S 60    // noise history needed before Quietest can open
#define POPUP_MS          1500    // how long the "Wait longer" pop-up stays up
#define POPUP_W           220     // pop-up box, centred horizontally
#define POPUP_H           56
#define POPUP_X           ((LCD_W - POPUP_W) / 2)
#define POPUP_Y           (GRID_Y + 90)
#define ACT_GAMMA         0.55f   // shading curve: <1 brightens small differences (1 = linear)
#define PEAK_DECAY_PX     1.8f    // peak-hold dots fall this many pixels per sweep
#define SPEC_SAMPLES      1       // measurements averaged per spectrum point (~3.2 ms each)
#define GRID_SAMPLES      1       // measurements averaged per grid channel   (~3.2 ms each)
#define SETTLE_US         100     // wait after retune before measuring
#define MEAS_TIMEOUT_US   5000    // a measurement normally takes ~3.2 ms
#define MAX_STALL_STATS   16
#define GRID_LEARN_SWEEPS 4       // sweeps spent learning the noise floor
#define SPEC_LEARN_SWEEPS 4

// ---------------------------------------------------------------- FPV table
#define NUM_BANDS 6
#define NUM_CH    8
static const char BAND_NAME[NUM_BANDS] = { 'A', 'B', 'E', 'F', 'R', 'L' };
static const char *BAND_LONG[NUM_BANDS] = { "Boscam A", "Boscam B", "Boscam E",
                                            "Fatshark", "Raceband", "Lowband" };
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

// ---------------------------------------------------------------- state
static uint32_t g_nfTimeouts = 0;
static StallStat g_stalls[MAX_STALL_STATS];
static int       g_nStalls = 0;
static bool      g_lastReadFallback = false;   // last readFreqDb() used the RSSI fallback
// timing statistics, printed and reset once per second
static uint64_t g_tTuneUs = 0, g_tMeasUs = 0, g_tDrawUs = 0;
static uint32_t g_nTunes = 0, g_nMeas = 0;
static ScreenMode mode = MODE_GRID;
// Band display order (by lowest frequency), shared by the grid rows and the
// spectrum buttons. Filled by buildButtonOrder() at startup.
static int  btnOrder[NUM_BANDS] = { 5, 2, 4, 0, 1, 3 };   // L E R A B F
static int  bandRow[NUM_BANDS]  = { 3, 4, 1, 5, 2, 0 };   // inverse of btnOrder
static int  spanDb = 15;            // dB above floor = full brightness (grid) and full height (spectrum)

// grid
static float  gridSmooth[NUM_BANDS][NUM_CH];
static float  gridFloor[NUM_BANDS][NUM_CH];
static int8_t gridShownLevel[NUM_BANDS][NUM_CH];
static bool   gridShownPeak[NUM_BANDS][NUM_CH];

// activity page
// Long-term statistics per channel, since start or the last reset.
static double   actSumDb[NUM_BANDS][NUM_CH];   // sum of dB above floor, all readings
static uint32_t actN[NUM_BANDS][NUM_CH];       // readings counted
static uint32_t actBusyN[NUM_BANDS][NUM_CH];   // readings at least ACT_BUSY_DB above floor
static float    actPeakDb[NUM_BANDS][NUM_CH];  // strongest reading above floor
static float    actTrackedS = 0;               // seconds of history collected
static uint32_t actLastMs = 0;                 // time of the last accumulation, 0 = none yet
static int8_t actShownLevel[NUM_BANDS][NUM_CH];
static bool   actShownPeak[NUM_BANDS][NUM_CH];
static int8_t actShownRank[NUM_BANDS][NUM_CH];  // rank drawn in the box, 0 = none

// quietest-channels page: what each list slot currently shows
static int16_t quietShownIdx[QUIET_LIST_N];      // band*NUM_CH + channel, -1 = empty
static int16_t quietShownTenths[QUIET_LIST_N];   // avg dB x10
static int8_t  quietShownBusy[QUIET_LIST_N];     // busy %
static bool    quietFooterDirty = true;
static uint32_t popupUntilMs = 0;               // pop-up on screen until this time, 0 = none
static bool   actFooterDirty = true;
static uint32_t gridSweeps = 0, gridLearnUntil = GRID_LEARN_SWEEPS;

// spectrum
static int      specBand = 4;       // start on Raceband
static uint16_t specLo, specHi;     // MHz
static int      specPts;
static float    specSmooth[SPEC_MAX_PTS];
static float    specFloor[NUM_BANDS][SPEC_MAX_PTS];
static bool     specFloorValid[NUM_BANDS];
static uint32_t specSweeps = 0, specLearnUntil = 0;
static bool     specNoSeedOnce = false;   // ENTER-hold re-learn: start from scratch
static int16_t  colShownH[GW];      // bar height currently drawn per column
static int16_t  colShownPk[GW];     // peak-hold row currently drawn per column
static float    colPeak[GW];        // peak-hold value per column (px)
static bool     colIsMarker[GW];
static int8_t   chValShown[NUM_CH];  // dB value currently drawn under each channel
static bool     chValHot[NUM_CH];

// input
static ButtonState btnKeyL  = { PIN_KEY_L,     false, 0, false };
static ButtonState btnKeyR  = { PIN_KEY_R,     false, 0, false };
static ButtonState btnEnter = { PIN_BTN_ENTER, false, 0, false };
static bool evEnterShort = false, evEnterLong = false, evToggle = false;
static int  pageDir = 1;              // +1 = KEY_R (forward), -1 = KEY_L (back)
static uint32_t lastInputPollMs = 0;
static bool  gridFooterDirty = true, specFooterDirty = true;
static int   learnShownPct = -1, learnShownY = -1;   // progress bar currently drawn
static int   scanAnimY = -1;          // y of the "Scanning" text on screen, -1 = not shown
static int   scanDots = 0;
static uint32_t scanDotsMs = 0;
#define SCAN_DOT_MS 400                // dot animation step

// ---------------------------------------------------------------- IO expander
static uint16_t exioOut = 0xFFFF;
static void exioWrite(const uint8_t *d, size_t n) {
  Wire.beginTransmission(EXIO_ADDR);
  Wire.write(d, n);
  Wire.endTransmission();
}
static void exioPin(uint8_t pin, bool level) {
  if (level) exioOut |= (1u << pin); else exioOut &= ~(1u << pin);
  uint8_t d[3] = { EXIO_REG_OUT, (uint8_t)exioOut, (uint8_t)(exioOut >> 8) };
  exioWrite(d, 3);
}
static void exioBacklight(uint8_t pct) {      // same scaling as Waveshare BSP
  if (pct > 97) pct = 97;
  uint8_t d[2] = { EXIO_REG_PWM, (uint8_t)(pct * 255 / 100) };
  exioWrite(d, 2);
}

// ---------------------------------------------------------------- input polling
// Called often, including from inside the radio sweeps, so presses are caught
// even while a sweep is running.
static void pollButton(ButtonState &b, uint32_t now, bool isEnter) {
  bool d = digitalRead(b.pin) == LOW;
  if (d && !b.down) {                       // pressed
    b.down = true; b.downSinceMs = now; b.longFired = false;
    if (!isEnter) {                         // KEY_L / KEY_R switch page on press
      evToggle = true;
      pageDir = (b.pin == PIN_KEY_L) ? -1 : 1;
    }
  } else if (!d && b.down) {                // released
    b.down = false;
    if (isEnter && !b.longFired && now - b.downSinceMs >= 30) evEnterShort = true;
  } else if (d && isEnter && !b.longFired && now - b.downSinceMs >= LONG_PRESS_MS) {
    b.longFired = true; evEnterLong = true;
  }
}

static void updateScanDots();   // defined after the LCD code

static void pollInput() {
  uint32_t now = millis();
  if (now - lastInputPollMs < 4) return;
  lastInputPollMs = now;
  pollButton(btnKeyL,  now, false);
  pollButton(btnKeyR,  now, false);
  pollButton(btnEnter, now, true);
  updateScanDots();
}

// ---------------------------------------------------------------- radio measurement
// One power measurement at the currently tuned frequency.
// Returns quarter-dB units (e.g. -360 == -90.0 dB), or INT16_MIN on timeout.
// A timeout seems to happen when a signal keeps the receiver busy.
static int16_t measureQdb() {
  BB_NF_CTRL &= ~NF_EN_BIT;
  BB_NF_CTRL |=  NF_START_BIT;
  BB_NF_CTRL &= ~NF_START_BIT;
  BB_NF_CTRL |=  NF_EN_BIT;
  uint32_t t0 = micros();
  while (!(BB_NF_CTRL & NF_DONE_BIT)) {
    if (micros() - t0 > MEAS_TIMEOUT_US) { g_nfTimeouts++; return INT16_MIN; }
  }
  // 10-bit field, always negative; sign-extend the same way libphy does.
  return (int16_t)(int32_t)((BB_NF_VALUE & 0x3FF) | 0xFFFFFC00u);
}

static void noteStall(uint16_t mhz, float fallbackDb) {
  for (int i = 0; i < g_nStalls; i++)
    if (g_stalls[i].mhz == mhz) { g_stalls[i].count++; g_stalls[i].lastFallbackDb = fallbackDb; return; }
  if (g_nStalls < MAX_STALL_STATS) {
    g_stalls[g_nStalls].mhz = mhz; g_stalls[g_nStalls].count = 1;
    g_stalls[g_nStalls].lastFallbackDb = fallbackDb; g_nStalls++;
  }
}

static void printStalls() {
  if (g_nStalls == 0) return;
  Serial.print("  stalls (MHz x count, RSSI fallback dB):");
  for (int i = 0; i < g_nStalls; i++)
    Serial.printf(" %u x%lu (%.1f)", g_stalls[i].mhz, (unsigned long)g_stalls[i].count, g_stalls[i].lastFallbackDb);
  Serial.println();
}

// Tune to a frequency and return the average power in dB (relative).
// If every measurement stalls, the receiver RSSI register is used instead and
// g_lastReadFallback is set, so callers can keep it out of floor learning.
static float readFreqDb(uint16_t mhz, int samples, uint32_t settleUs) {
  uint32_t t0 = micros();
  phy_set_chanfreq(mhz, 0);           // 20 MHz bandwidth
  uint32_t t1 = micros();
  delayMicroseconds(settleUs);        // let the synthesizer / AGC settle
  uint32_t t2 = micros();
  int32_t sum = 0; int n = 0;
  for (int i = 0; i < samples; i++) {
    int16_t q = measureQdb();
    if (q != INT16_MIN) { sum += q; n++; }
  }
  uint32_t t3 = micros();
  g_tTuneUs += t1 - t0; g_nTunes++;
  g_tMeasUs += t3 - t2; g_nMeas += samples;
  pollInput();
  g_lastReadFallback = false;
  if (n == 0) {
    float fb = (float)phy_get_rssi();
    noteStall(mhz, fb);
    g_lastReadFallback = true;
    return fb;
  }
  return (float)sum / n / 4.0f;
}

// ---------------------------------------------------------------- LCD (ILI9488, RGB666)
static SPISettings lcdSpi(LCD_SPI_HZ, MSBFIRST, SPI_MODE0);
static uint8_t lineBuf[LCD_W * 3];

static void lcdCmd(uint8_t c, const uint8_t *data, size_t n) {
  SPI.beginTransaction(lcdSpi);
  digitalWrite(PIN_LCD_CS, LOW);
  digitalWrite(PIN_LCD_DC, LOW);
  SPI.transfer(c);
  if (n) { digitalWrite(PIN_LCD_DC, HIGH); SPI.writeBytes(data, n); }
  digitalWrite(PIN_LCD_CS, HIGH);
  SPI.endTransaction();
}
static void lcdCmd0(uint8_t c) { lcdCmd(c, nullptr, 0); }
#define LCMD(c, ...) do { static const uint8_t _d[] = { __VA_ARGS__ }; lcdCmd(c, _d, sizeof(_d)); } while (0)

static void lcdInit() {
  // Hardware reset through the IO expander
  uint8_t modeCfg[3] = { EXIO_REG_MODE, 0xF7, 0xFF };  // as Waveshare BSP
  exioWrite(modeCfg, 3);
  exioPin(EXIO_PIN_LCDRST, 0); delay(10);
  exioPin(EXIO_PIN_LCDRST, 1); delay(120);

  // Init sequence taken from Waveshare's ILI9488 driver for this panel
  LCMD(0xC0, 0x10, 0x12);
  LCMD(0xC1, 0x66);
  LCMD(0xC5, 0x00, 0x45, 0x80);
  LCMD(0x36, 0x48);                 // MADCTL: MX | BGR
  LCMD(0x3A, 0x66);                 // 18-bit color
  LCMD(0xB0, 0x80);
  LCMD(0xB1, 0xA0);
  LCMD(0xB4, 0x02);
  LCMD(0xB6, 0x00, 0x02, 0x3B);
  LCMD(0xE0, 0x00,0x08,0x19,0x05,0x16,0x09,0x35,0x47,0x4A,0x04,0x0D,0x0C,0x30,0x33,0x0F);
  LCMD(0xE1, 0x00,0x08,0x19,0x05,0x16,0x09,0x35,0x47,0x4A,0x04,0x0D,0x0C,0x30,0x33,0x0F);
  LCMD(0xE9, 0x00);
  LCMD(0xF7, 0xA9, 0x51, 0x2C, 0x82);
  lcdCmd0(0x21);                    // inversion on (panel needs it)
  lcdCmd0(0x11); delay(120);        // sleep out
  lcdCmd0(0x29); delay(20);         // display on
}

static void lcdWindow(int x, int y, int w, int h) {
  uint8_t ca[4] = { (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)((x + w - 1) >> 8), (uint8_t)(x + w - 1) };
  uint8_t ra[4] = { (uint8_t)(y >> 8), (uint8_t)y, (uint8_t)((y + h - 1) >> 8), (uint8_t)(y + h - 1) };
  lcdCmd(0x2A, ca, 4);
  lcdCmd(0x2B, ra, 4);
}

static void lcdFill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  if (w <= 0 || h <= 0) return;
  lcdWindow(x, y, w, h);
  for (int i = 0; i < w; i++) { lineBuf[i*3] = r; lineBuf[i*3+1] = g; lineBuf[i*3+2] = b; }
  SPI.beginTransaction(lcdSpi);
  digitalWrite(PIN_LCD_CS, LOW);
  digitalWrite(PIN_LCD_DC, LOW);
  SPI.transfer(0x2C);
  digitalWrite(PIN_LCD_DC, HIGH);
  for (int j = 0; j < h; j++) SPI.writeBytes(lineBuf, w * 3);
  digitalWrite(PIN_LCD_CS, HIGH);
  SPI.endTransaction();
}
static void lcdFillC(int x, int y, int w, int h, RGB c) { lcdFill(x, y, w, h, c.r, c.g, c.b); }

// Draw text with a solid background (glyph box drawn as one window).
static void lcdText(int x, int y, const char *s, int scale, RGB fg, RGB bg) {
  for (; *s; s++, x += 6 * scale) {
    char c = *s;
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *g = FONT5X7[c - 0x20];
    lcdWindow(x, y, 6 * scale, 8 * scale);
    SPI.beginTransaction(lcdSpi);
    digitalWrite(PIN_LCD_CS, LOW);
    digitalWrite(PIN_LCD_DC, LOW);
    SPI.transfer(0x2C);
    digitalWrite(PIN_LCD_DC, HIGH);
    for (int row = 0; row < 8; row++) {
      int n = 0;
      for (int col = 0; col < 6; col++) {
        bool on = col < 5 && ((g[col] >> row) & 1);
        RGB p = on ? fg : bg;
        for (int k = 0; k < scale; k++) { lineBuf[n++] = p.r; lineBuf[n++] = p.g; lineBuf[n++] = p.b; }
      }
      for (int k = 0; k < scale; k++) SPI.writeBytes(lineBuf, n);
    }
    digitalWrite(PIN_LCD_CS, HIGH);
    SPI.endTransaction();
  }
}

// Text at any character cell size (cw x ch pixels, normal font is 6 x 8),
// nearest-neighbour scaled, for sizes between the integer scales.
static void lcdTextSized(int x, int y, const char *s, int cw, int ch, RGB fg, RGB bg) {
  for (; *s; s++, x += cw) {
    char c = *s;
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *g = FONT5X7[c - 0x20];
    lcdWindow(x, y, cw, ch);
    SPI.beginTransaction(lcdSpi);
    digitalWrite(PIN_LCD_CS, LOW);
    digitalWrite(PIN_LCD_DC, LOW);
    SPI.transfer(0x2C);
    digitalWrite(PIN_LCD_DC, HIGH);
    for (int py = 0; py < ch; py++) {
      int row = py * 8 / ch, n = 0;
      for (int px = 0; px < cw; px++) {
        int col = px * 6 / cw;
        bool on = col < 5 && row < 7 && ((g[col] >> row) & 1);
        RGB p = on ? fg : bg;
        lineBuf[n++] = p.r; lineBuf[n++] = p.g; lineBuf[n++] = p.b;
      }
      SPI.writeBytes(lineBuf, n);
    }
    digitalWrite(PIN_LCD_CS, HIGH);
    SPI.endTransaction();
  }
}

static RGB scaleColor(RGB c, float k) {
  RGB o = { (uint8_t)(c.r * k), (uint8_t)(c.g * k), (uint8_t)(c.b * k) };
  return o;
}

// ---------------------------------------------------------------- GRID screen
static void gridResetFloor() {
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) { gridFloor[b][c] = 0; gridSmooth[b][c] = NAN; }
  // Spectrum floors came from the old grid floor; re-seed them next time.
  for (int b = 0; b < NUM_BANDS; b++) specFloorValid[b] = false;
  actClear();                              // activity is relative to the floor
  gridLearnUntil = gridSweeps + GRID_LEARN_SWEEPS;
}

static void gridInvalidate() {
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) { gridShownLevel[b][c] = -1; gridShownPeak[b][c] = false; }
}

static void actClear() {
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      actSumDb[b][c] = 0; actN[b][c] = 0; actBusyN[b][c] = 0; actPeakDb[b][c] = 0;
    }
  actLastMs = 0;
  actTrackedS = 0;
}

static void actInvalidate() {
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) { actShownLevel[b][c] = -1; actShownPeak[b][c] = false; actShownRank[b][c] = -1; }
}

// Title, channel numbers and band letters - shared by GRID and ACTIVITY.
static void drawGridFrame(const char *title, RGB titleColor) {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, title, 2, titleColor, C_BG);
  for (int c = 0; c < NUM_CH; c++) {
    char s[2] = { (char)('1' + c), 0 };
    lcdText(GRID_X + c * (CELL_W + CELL_GAP) + (CELL_W - 12) / 2, HDR_Y, s, 2, C_DIM, C_BG);
  }
  for (int r = 0; r < NUM_BANDS; r++) {
    char s[2] = { BAND_NAME[btnOrder[r]], 0 };
    lcdText(8, GRID_Y + r * (CELL_H + CELL_GAP) + (CELL_H - 24) / 2, s, 3, C_TEXT, C_BG);
  }
  learnShownPct = -1; scanAnimY = -1;
}

static void actDrawStatic() {
  drawGridFrame("FPV Channel Noise", C_RED);
  actInvalidate();
  actFooterDirty = true;
}

static void gridDrawStatic() {
  drawGridFrame("FPV Channel Activity", C_ACC);
  gridInvalidate();
  gridFooterDirty = true;
}

static void gridDrawCell(int b, int c, int level, bool peak) {
  int x = GRID_X + c * (CELL_W + CELL_GAP);
  int y = GRID_Y + bandRow[b] * (CELL_H + CELL_GAP);
  RGB col = scaleColor(C_HOT, 0.06f + 0.94f * level / 32.0f);
  if (peak) {
    lcdFill(x, y, CELL_W, CELL_H, 255, 255, 255);
    lcdFillC(x + 2, y + 2, CELL_W - 4, CELL_H - 4, col);
  } else {
    lcdFillC(x, y, CELL_W, CELL_H, col);
  }
  char f[6];
  snprintf(f, sizeof(f), "%u", FPV_MHZ[b][c]);
  RGB tc = level > 16 ? RGB{ 0, 0, 0 } : C_DIM;
  lcdText(x + (CELL_W - 24) / 2, y + CELL_H - 11, f, 1, tc, col);
}

// "Scanning" followed by 1, 2, 3 dots, repeating. The word is drawn once;
// updateScanDots() redraws only the three dot positions.
static void updateScanDots() {
  if (scanAnimY < 0) return;
  uint32_t now = millis();
  if (now - scanDotsMs < SCAN_DOT_MS) return;
  scanDotsMs = now;
  scanDots = scanDots % 3 + 1;
  char d[4] = { ' ', ' ', ' ', 0 };
  for (int i = 0; i < scanDots; i++) d[i] = '.';
  lcdText(4 + 8 * 12, scanAnimY, d, 2, C_DIM, C_BG);   // right after "Scanning"
}

static void drawScanning(int y) {
  lcdText(4, y, "Scanning", 2, C_DIM, C_BG);
  scanAnimY = y;
  scanDots = 0;
  scanDotsMs = 0;          // show the first dot right away
  updateScanDots();
}

// "SPAN nndB" at the bottom right; same screen position on both pages.
static void drawSpanLabel() {
  char s[20];
  if (mode == MODE_ACTIVITY || mode == MODE_QUIET) {
    uint32_t t = (uint32_t)actTrackedS;    // fixed width so no leftovers when it changes
    if (t < 3600) snprintf(s, sizeof(s), "History %2lum%02lus", (unsigned long)(t / 60), (unsigned long)(t % 60));
    else          snprintf(s, sizeof(s), "History %2luh%02lum", (unsigned long)(t / 3600), (unsigned long)(t / 60 % 60));
  }
  else snprintf(s, sizeof(s), "Span %d dB", spanDb);
  lcdText(LCD_W - 6 * (int)strlen(s) - 4, FOOT_Y + 20, s, 1, C_DIM, C_BG);
}

// "LEARNING nn%" plus a progress bar, drawn in a page's footer area.
// Only the parts that change are redrawn.
#define LEARN_BAR_W 240
static void drawLearnProgress(int footY, int pct) {
  pct = constrain(pct, 0, 100);
  if (learnShownY != footY || learnShownPct < 0) {
    lcdFill(0, footY, LCD_W, LCD_H - footY, 0, 0, 0);
    lcdFillC(4, footY + 20, LEARN_BAR_W + 4, 10, C_DIM);      // bar outline
    lcdFill(6, footY + 22, LEARN_BAR_W, 6, 0, 0, 0);
    drawSpanLabel();
    learnShownPct = -1; scanAnimY = -1;
  }
  if (pct == learnShownPct) return;
  char s[16];
  snprintf(s, sizeof(s), "Learning %3d%%", pct);
  lcdText(4, footY + 2, s, 2, C_ACC, C_BG);
  int w = LEARN_BAR_W * pct / 100;
  lcdFillC(6, footY + 22, w, 6, C_ACC);
  lcdFill(6 + w, footY + 22, LEARN_BAR_W - w, 6, 0, 0, 0);
  learnShownPct = pct; learnShownY = footY;
}

static void gridDrawFooter(int pb, int pc, float aboveDb) {
  char s[40];
  learnShownPct = -1; scanAnimY = -1;
  lcdFill(0, FOOT_Y, LCD_W, LCD_H - FOOT_Y, 0, 0, 0);
  if (pb >= 0) {
    snprintf(s, sizeof(s), "%c%d %u %d dB", BAND_NAME[pb], pc + 1, FPV_MHZ[pb][pc], (int)lroundf(aboveDb));
    lcdText(4, FOOT_Y + 2, s, 2, C_ACC, C_BG);
    lcdText(4, FOOT_Y + 20, BAND_LONG[pb], 1, C_DIM, C_BG);
  } else {
    drawScanning(FOOT_Y + 2);
  }
  drawSpanLabel();
}

static void drawTitleStatus(float sweepsPerSec, bool learning) {
  char s[24];
  if (learning) snprintf(s, sizeof(s), "Learning  ");
  else          snprintf(s, sizeof(s), "%4.1f sw/s", sweepsPerSec);
  lcdText(LCD_W - 6 * 10 - 4, 8, s, 1, learning ? C_ACC : C_DIM, C_BG);
}

// Print where the time went since the last report, then reset the counters.
static void printTiming(uint32_t sweeps) {
  Serial.printf("  timing: retune %lu us avg, measurement %lu us avg (%lu/sweep), draw %lu ms/sweep\n",
                (unsigned long)(g_nTunes ? g_tTuneUs / g_nTunes : 0),
                (unsigned long)(g_nMeas ? g_tMeasUs / g_nMeas : 0),
                (unsigned long)(sweeps ? g_nMeas / sweeps : 0),
                (unsigned long)(sweeps ? g_tDrawUs / sweeps / 1000 : 0));
  g_tTuneUs = g_tMeasUs = g_tDrawUs = 0; g_nTunes = g_nMeas = 0;
}

static void resetTiming() {
  g_tTuneUs = g_tMeasUs = g_tDrawUs = 0; g_nTunes = g_nMeas = 0;
}

// True when the USB serial buffer can take a dump of this size right now.
// If the PC isn't reading, the buffer stays full and the dump is skipped.
static bool serialHasRoom(int bytes) {
  return Serial.availableForWrite() >= bytes;
}

// ---------------------------------------------------------------- NOISE screen
// (internally MODE_ACTIVITY / act*). Every channel keeps its own long-term
// statistics from start-up (or the last reset): the average of its readings
// above the floor, how often it was busy, and its strongest reading. Nothing
// decays, so the longer it runs the more clearly quiet channels stand out.
// The display shades each box between the quietest channel (black) and the
// busiest (red) by long-term average.
static void actAccumulate(float raw[NUM_BANDS][NUM_CH]) {
  uint32_t now = millis();
  float dt = actLastMs ? (now - actLastMs) / 1000.0f : 0.2f;
  actLastMs = now;
  if (dt > ACT_MAX_DT_S) dt = ACT_MAX_DT_S;  // a pause (other page) isn't counted
  actTrackedS += dt;
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      if (isnan(raw[b][c]) || gridFloor[b][c] == 0) continue;
      float above = raw[b][c] - gridFloor[b][c];
      if (above < 0) above = 0;
      actSumDb[b][c] += above;
      actN[b][c]++;
      if (above >= ACT_BUSY_DB) actBusyN[b][c]++;
      if (above > actPeakDb[b][c]) actPeakDb[b][c] = above;
    }
}

// Channel's long-term average signal above the floor, in dB.
static float actAvg(int b, int c);

// Fill order[] with channel indexes (band*NUM_CH + channel), quietest first
// by long-term average. Ties keep table order so ranks don't flicker.
static void actRankOrder(int *order) {
  for (int i = 0; i < NUM_BANDS * NUM_CH; i++) order[i] = i;
  for (int i = 1; i < NUM_BANDS * NUM_CH; i++) {
    int k = order[i]; float kv = actAvg(k / NUM_CH, k % NUM_CH); int j = i - 1;
    while (j >= 0 && actAvg(order[j] / NUM_CH, order[j] % NUM_CH) > kv) { order[j + 1] = order[j]; j--; }
    order[j + 1] = k;
  }
}

// Channel's long-term average signal above the floor, in dB.
static float actAvg(int b, int c) {
  return actN[b][c] ? (float)(actSumDb[b][c] / actN[b][c]) : 0;
}

// Percentage of readings where the channel was busy.
static int actBusyPct(int b, int c) {
  return actN[b][c] ? (int)((uint64_t)actBusyN[b][c] * 100 / actN[b][c]) : 0;
}

// rank: 1 = quietest .. 48 = busiest, 0 = don't show a rank
static void actDrawCell(int b, int c, int level, bool peak, int rank) {
  int x = GRID_X + c * (CELL_W + CELL_GAP);
  int y = GRID_Y + bandRow[b] * (CELL_H + CELL_GAP);
  RGB col = scaleColor(C_RED, level / 32.0f);          // black -> red
  if (peak) {
    lcdFill(x, y, CELL_W, CELL_H, 255, 255, 255);
    lcdFillC(x + 2, y + 2, CELL_W - 4, CELL_H - 4, col);
  } else {
    lcdFillC(x, y, CELL_W, CELL_H, col);
  }
  char f[6];
  snprintf(f, sizeof(f), "%u", FPV_MHZ[b][c]);
  RGB tc = level > 16 ? RGB{ 0, 0, 0 } : C_DIM;
  lcdText(x + (CELL_W - 24) / 2, y + CELL_H - 11, f, 1, tc, col);
  if (rank > 0) {
    char r[3];
    snprintf(r, sizeof(r), "%d", rank);
    int w = 9 * (int)strlen(r);                        // 1.5x digits, centred
    RGB rc = level > 16 ? RGB{ 0, 0, 0 } : C_RANK;
    lcdTextSized(x + (CELL_W - w) / 2, y + 8, r, 9, 12, rc, col);
  }
}

static void actDrawFooter(int pb, int pc, float excessDb) {
  char s[40];
  learnShownPct = -1; scanAnimY = -1;
  lcdFill(0, FOOT_Y, LCD_W, LCD_H - FOOT_Y, 0, 0, 0);
  if (pb >= 0) {
    snprintf(s, sizeof(s), "%c%d %u %.1f dB %d%%", BAND_NAME[pb], pc + 1, FPV_MHZ[pb][pc], excessDb,
             actBusyPct(pb, pc));
    lcdText(4, FOOT_Y + 2, s, 2, C_ACC, C_BG);
    char t[32];
    snprintf(t, sizeof(t), "Quietest - %s", BAND_LONG[pb]);
    lcdText(4, FOOT_Y + 20, t, 1, C_DIM, C_BG);
  } else {
    drawScanning(FOOT_Y + 2);
  }
  drawSpanLabel();
}

static void actDraw(bool learning, uint32_t learnStart) {
  static int lastB = -2, lastC = -2, lastTenths = -9999;
  static bool lastLearning = false;
  float busiest = -1e9f;                    // full red
  int qb = -1, qc = -1; float quiet = 1e9f;  // black; outlined + named
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float a = actAvg(b, c);
      if (a > busiest) busiest = a;
      if (a < quiet) { quiet = a; qb = b; qc = c; }
    }
  // Quietest = black, busiest = full red. If every channel is within
  // ACT_MIN_SCALE_DB of the quietest, the spread is shown dim instead of
  // stretching tiny noise differences to full red.
  float scale = max(busiest - quiet, ACT_MIN_SCALE_DB);
  bool havePeak = !learning && actLastMs != 0;
  int pb = qb, pc = qc;

  int8_t rank[NUM_BANDS][NUM_CH];
  int order[NUM_BANDS * NUM_CH];
  actRankOrder(order);
  for (int i = 0; i < NUM_BANDS * NUM_CH; i++)
    rank[order[i] / NUM_CH][order[i] % NUM_CH] = havePeak ? (int8_t)(i + 1) : 0;

  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float e = learning ? 0 : (actAvg(b, c) - quiet) / scale;   // 0..1
      e = constrain(e, 0.0f, 1.0f);
      int level = (int)lroundf(32.0f * powf(e, ACT_GAMMA));      // gain curve
      bool peak = havePeak && b == pb && c == pc;
      if (level != actShownLevel[b][c] || peak != actShownPeak[b][c] || rank[b][c] != actShownRank[b][c]) {
        actDrawCell(b, c, level, peak, rank[b][c]);
        actShownLevel[b][c] = level; actShownPeak[b][c] = peak; actShownRank[b][c] = rank[b][c];
      }
      pollInput();
    }

  int footB = havePeak ? pb : -1, footC = havePeak ? pc : -1;
  int tenths = havePeak ? (int)lroundf(quiet * 10) : -1;
  if (learning) {
    drawLearnProgress(FOOT_Y, (gridSweeps - learnStart) * 100 / GRID_LEARN_SWEEPS);
  } else if (lastLearning) {
    actFooterDirty = true;                   // learning just finished
  }
  lastLearning = learning;
  if (!learning && (footB != lastB || footC != lastC || tenths != lastTenths || actFooterDirty)) {
    actDrawFooter(footB, footC, quiet);
    lastB = footB; lastC = footC; lastTenths = tenths; actFooterDirty = false;
  }
}

// ---------------------------------------------------------------- QUIETEST CHANNELS screen
// The QUIET_LIST_N quietest channels from the Channel Noise history, in two
// columns of 8: left column 1-8, right column 9-16. Measurement keeps running
// on this page, so the history and the list stay live.
#define QUIET_Y0     34      // first row
#define QUIET_PITCH  31      // row spacing
#define QUIET_COL_W  158

static void quietInvalidate() {
  for (int i = 0; i < QUIET_LIST_N; i++) { quietShownIdx[i] = -2; quietShownTenths[i] = -1; quietShownBusy[i] = -1; }
}

static void quietDrawStatic() {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, "Quietest Channels", 2, C_ACC, C_BG);
  lcdFillC(QUIET_COL_W, QUIET_Y0 - 2, 1, 8 * QUIET_PITCH, C_MARK);   // column divider
  quietInvalidate();
  quietFooterDirty = true;
  learnShownPct = -1; scanAnimY = -1;
}

// slot 0..15; idx = band*NUM_CH + channel, or -1 to blank the slot
static void quietDrawEntry(int slot, int idx) {
  int x = 6 + (slot / 8) * QUIET_COL_W;
  int y = QUIET_Y0 + (slot % 8) * QUIET_PITCH;
  lcdFill(x, y, QUIET_COL_W - 10, QUIET_PITCH - 2, 0, 0, 0);
  if (idx < 0) return;
  int b = idx / NUM_CH, c = idx % NUM_CH;
  char t[24];
  snprintf(t, sizeof(t), "%2d", slot + 1);
  lcdText(x, y, t, 2, C_RANK, C_BG);                       // position in the list
  snprintf(t, sizeof(t), "%c%d", BAND_NAME[b], c + 1);
  lcdText(x + 30, y, t, 2, C_ACC, C_BG);                   // band / channel
  snprintf(t, sizeof(t), "%u", FPV_MHZ[b][c]);
  lcdText(x + 62, y, t, 2, C_TEXT, C_BG);                  // frequency
  snprintf(t, sizeof(t), "%.1f dB, %d%% busy", actAvg(b, c), actBusyPct(b, c));
  lcdText(x + 30, y + 18, t, 1, C_DIM, C_BG);
}

static void quietDraw(bool learning, uint32_t learnStart) {
  static bool lastLearning = false;
  bool ready = !learning && actLastMs != 0;
  int order[NUM_BANDS * NUM_CH];
  actRankOrder(order);
  for (int i = 0; i < QUIET_LIST_N; i++) {
    int idx = ready ? order[i] : -1;
    int tenths = -1, busy = -1;
    if (idx >= 0) {
      tenths = (int)lroundf(actAvg(idx / NUM_CH, idx % NUM_CH) * 10);
      busy = actBusyPct(idx / NUM_CH, idx % NUM_CH);
    }
    if (idx != quietShownIdx[i] || tenths != quietShownTenths[i] || busy != quietShownBusy[i]) {
      quietDrawEntry(i, idx);
      quietShownIdx[i] = idx; quietShownTenths[i] = tenths; quietShownBusy[i] = busy;
    }
    pollInput();
  }

  if (learning) {
    drawLearnProgress(FOOT_Y, (gridSweeps - learnStart) * 100 / GRID_LEARN_SWEEPS);
  } else if (lastLearning) {
    quietFooterDirty = true;                 // learning just finished
  }
  lastLearning = learning;
  static bool lastReady = false;
  if (ready != lastReady) { quietFooterDirty = true; lastReady = ready; }
  if (!learning && quietFooterDirty) {
    learnShownPct = -1; scanAnimY = -1;
    lcdFill(0, FOOT_Y, LCD_W, LCD_H - FOOT_Y, 0, 0, 0);
    if (ready) lcdText(4, FOOT_Y + 20, "1 = quietest", 1, C_DIM, C_BG);
    else       drawScanning(FOOT_Y + 2);
    drawSpanLabel();
    quietFooterDirty = false;
  }
}

// One full grid sweep + redraw of changed cells.
// Runs on the GRID and ACTIVITY pages; both share the measurement and floor.
static void gridSweep() {
  static int lastPeakB = -2, lastPeakC = -2, lastAboveRounded = -999, lastSpan = -1;

  static bool lastLearning = false;
  const uint32_t learnStart = gridLearnUntil - GRID_LEARN_SWEEPS;
  const bool learningSweep = gridSweeps < gridLearnUntil;   // this sweep counts toward learning
  const int  total = NUM_BANDS * NUM_CH;

  float raw[NUM_BANDS][NUM_CH];
  bool  fb[NUM_BANDS][NUM_CH];
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      raw[b][c] = readFreqDb(FPV_MHZ[b][c], GRID_SAMPLES, SETTLE_US);
      fb[b][c] = g_lastReadFallback;
      if (evToggle || evEnterShort || evEnterLong) return;  // react to input immediately
      if (learningSweep && c == NUM_CH - 1) {
        int done = (gridSweeps - learnStart) * total + b * NUM_CH + c + 1;
        drawLearnProgress(FOOT_Y, done * 100 / (GRID_LEARN_SWEEPS * total));
      }
    }
  gridSweeps++;

  bool learning = gridSweeps <= gridLearnUntil;
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float v = raw[b][c];
      if (isnan(v)) continue;
      float &s = gridSmooth[b][c];
      if (isnan(s)) s = v;
      else s += (v > s ? 0.7f : 0.25f) * (v - s);
      if (fb[b][c]) continue;                // fallback readings never set the floor
      float &f = gridFloor[b][c];
      if (f == 0 || v < f) f = v;            // track the minimum
      else if (!learning) f += 0.01f;        // slow upward drift
    }

  // Keep a channel's floor from being learned while a VTX sits on it:
  // clamp each floor to at most (median floor + 4 dB).
  float tmp[NUM_BANDS * NUM_CH]; int n = 0;
  for (int b = 0; b < NUM_BANDS; b++) for (int c = 0; c < NUM_CH; c++) tmp[n++] = gridFloor[b][c];
  for (int i = 1; i < n; i++) { float k = tmp[i]; int j = i - 1; while (j >= 0 && tmp[j] > k) { tmp[j+1] = tmp[j]; j--; } tmp[j+1] = k; }
  float medFloor = tmp[n / 2];
  for (int b = 0; b < NUM_BANDS; b++) for (int c = 0; c < NUM_CH; c++)
    if (gridFloor[b][c] > medFloor + 4.0f) gridFloor[b][c] = medFloor + 4.0f;

  if (!learning) actAccumulate(raw);

  uint32_t tDraw0 = micros();
  if (mode == MODE_ACTIVITY) {
    if (!popupUntilMs) actDraw(learning, learnStart);   // don't draw over the pop-up
  } else if (mode == MODE_QUIET) {
    quietDraw(learning, learnStart);
  } else {
  int pb = -1, pc = -1; float best = -1e9f;
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float above = gridSmooth[b][c] - gridFloor[b][c];
      if (isnan(above) || gridFloor[b][c] == 0) above = 0;
      if (above > best) { best = above; pb = b; pc = c; }
    }
  bool havePeak = best >= 3.0f && !learning;

  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float above = learning ? 0 : gridSmooth[b][c] - gridFloor[b][c];
      if (isnan(above) || gridFloor[b][c] == 0) above = 0;
      int level = constrain((int)lroundf(above * 32.0f / spanDb), 0, 32);
      bool peak = havePeak && b == pb && c == pc;
      if (level != gridShownLevel[b][c] || peak != gridShownPeak[b][c]) {
        gridDrawCell(b, c, level, peak);
        gridShownLevel[b][c] = level; gridShownPeak[b][c] = peak;
      }
      pollInput();
    }

  int aboveRounded = havePeak ? (int)lroundf(best) : -1;
  int footB = havePeak ? pb : -1, footC = havePeak ? pc : -1;
  if (learning) {
    drawLearnProgress(FOOT_Y, (gridSweeps - learnStart) * 100 / GRID_LEARN_SWEEPS);
  } else if (lastLearning) {
    gridFooterDirty = true;                  // learning just finished
  }
  lastLearning = learning;
  if (!learning && (footB != lastPeakB || footC != lastPeakC || aboveRounded != lastAboveRounded ||
                    spanDb != lastSpan || gridFooterDirty)) {
    gridDrawFooter(footB, footC, best);
    lastPeakB = footB; lastPeakC = footC; lastAboveRounded = aboveRounded; lastSpan = spanDb; gridFooterDirty = false;
  }
  }   // end GRID drawing
  g_tDrawUs += micros() - tDraw0;

  static uint32_t lastRateMs = 0, rateSweeps = 0;
  rateSweeps++;
  uint32_t now = millis();
  if (now - lastRateMs >= 1000) {
    float sps = rateSweeps * 1000.0f / (now - lastRateMs);
    uint32_t sweepsThisPeriod = rateSweeps;
    rateSweeps = 0; lastRateMs = now;
    drawTitleStatus(sps, learning);
    if ((mode == MODE_ACTIVITY || mode == MODE_QUIET) && !learning) drawSpanLabel();   // history time
    if (serialHasRoom(1600)) {
      printTiming(sweepsThisPeriod);
      printStalls();
      Serial.printf("grid sweep %lu  %.1f/s  timeouts=%lu\n", (unsigned long)gridSweeps,
                    sps, (unsigned long)g_nfTimeouts);
      bool noiseDump = (mode == MODE_ACTIVITY || mode == MODE_QUIET);
      if (noiseDump)
        Serial.printf("  noise, %lu s of history (avg dB above floor / %% busy / peak dB):\n",
                      (unsigned long)actTrackedS);
      for (int r = 0; r < NUM_BANDS; r++) {
        int b = btnOrder[r];
        Serial.printf("  %c:", BAND_NAME[b]);
        for (int c = 0; c < NUM_CH; c++) {
          if (noiseDump)
            Serial.printf(" %4u=%4.2f/%d%%/%.0f", FPV_MHZ[b][c], actAvg(b, c), actBusyPct(b, c), actPeakDb[b][c]);
          else
            Serial.printf(" %4u=%6.1f(%+5.1f)", FPV_MHZ[b][c], raw[b][c], gridSmooth[b][c] - gridFloor[b][c]);
        }
        Serial.println();
      }
    } else {
      resetTiming();
    }
  }
}

// ---------------------------------------------------------------- SPECTRUM screen
static int freqToCol(float mhz) {
  return (int)lroundf((mhz - specLo) * (GW - 1) / (float)(specHi - specLo));
}

// Noise floor at any frequency, linearly interpolated from the grid's 48
// learned channel floors (nearest value is held beyond the ends).
static float gridFloorAt(float mhz) {
  float lowF = 1e9f, lowV = 0, highF = -1e9f, highV = 0;     // overall ends
  float belowF = -1e9f, belowV = 0, aboveF = 1e9f, aboveV = 0; // neighbours
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float f = FPV_MHZ[b][c], v = gridFloor[b][c];
      if (f < lowF)  { lowF = f;  lowV = v; }
      if (f > highF) { highF = f; highV = v; }
      if (f <= mhz && f > belowF) { belowF = f; belowV = v; }
      if (f >= mhz && f < aboveF) { aboveF = f; aboveV = v; }
    }
  if (mhz <= lowF)  return lowV;
  if (mhz >= highF) return highV;
  if (aboveF == belowF) return belowV;
  return belowV + (aboveV - belowV) * (mhz - belowF) / (aboveF - belowF);
}

static bool gridFloorReady() { return gridSweeps > gridLearnUntil; }

static void specSetBand(int band) {
  specBand = band;
  uint16_t lo = 65535, hi = 0;
  for (int c = 0; c < NUM_CH; c++) { lo = min(lo, FPV_MHZ[band][c]); hi = max(hi, FPV_MHZ[band][c]); }
  lo -= SPEC_MARGIN_MHZ; hi += SPEC_MARGIN_MHZ;
  lo -= lo % SPEC_STEP_MHZ;
  specPts = (hi - lo) / SPEC_STEP_MHZ + 1;
  if (specPts > SPEC_MAX_PTS) specPts = SPEC_MAX_PTS;
  specLo = lo;
  specHi = lo + (specPts - 1) * SPEC_STEP_MHZ;
  for (int i = 0; i < specPts; i++) specSmooth[i] = NAN;
  if (!specFloorValid[band]) {
    bool seed = gridFloorReady() && !specNoSeedOnce;
    for (int i = 0; i < SPEC_MAX_PTS; i++)
      specFloor[band][i] = (seed && i < specPts) ? gridFloorAt(lo + i * SPEC_STEP_MHZ) : 0;
    if (seed) {
      // Grid has learned: use its floor as-is. Normal min-tracking keeps
      // refining it during use, so no separate learning pass is needed.
      specLearnUntil = specSweeps;
      specFloorValid[band] = true;
    } else {
      // Grid not learned yet, or an explicit ENTER-hold re-learn on this page.
      specLearnUntil = specSweeps + SPEC_LEARN_SWEEPS;
    }
    specNoSeedOnce = false;
  }
  for (int x = 0; x < GW; x++) { colPeak[x] = 0; colIsMarker[x] = false; }
  for (int c = 0; c < NUM_CH; c++) {
    int x = freqToCol(FPV_MHZ[band][c]);
    if (x >= 0 && x < GW) colIsMarker[x] = true;
  }
}

// Band buttons, left to right, in order of each band's lowest frequency.
static void buildButtonOrder() {
  uint16_t lowest[NUM_BANDS];
  for (int b = 0; b < NUM_BANDS; b++) {
    lowest[b] = 65535;
    for (int c = 0; c < NUM_CH; c++) lowest[b] = min(lowest[b], FPV_MHZ[b][c]);
    btnOrder[b] = b;
  }
  for (int i = 1; i < NUM_BANDS; i++) {          // insertion sort by lowest freq
    int k = btnOrder[i]; int j = i - 1;
    while (j >= 0 && lowest[btnOrder[j]] > lowest[k]) { btnOrder[j + 1] = btnOrder[j]; j--; }
    btnOrder[j + 1] = k;
  }
  for (int i = 0; i < NUM_BANDS; i++) bandRow[btnOrder[i]] = i;
}
// Band of the button to the right of the current one (wraps around).
static int nextBandInButtonOrder(int band) {
  for (int i = 0; i < NUM_BANDS; i++)
    if (btnOrder[i] == band) return btnOrder[(i + 1) % NUM_BANDS];
  return btnOrder[0];
}

static void specDrawButtons() {
  int totalW = NUM_BANDS * BTN_W + (NUM_BANDS - 1) * BTN_GAP;
  int x0 = (LCD_W - totalW) / 2;
  for (int i = 0; i < NUM_BANDS; i++) {
    int b = btnOrder[i];
    int x = x0 + i * (BTN_W + BTN_GAP);
    bool sel = b == specBand;
    RGB edge = sel ? C_ACC : C_DIM;
    RGB fill = sel ? C_ACC : RGB{ 20, 20, 20 };
    lcdFillC(x, BTN_Y, BTN_W, BTN_H, edge);
    lcdFillC(x + 2, BTN_Y + 2, BTN_W - 4, BTN_H - 4, fill);
    char s[2] = { BAND_NAME[b], 0 };
    lcdText(x + (BTN_W - 12) / 2, BTN_Y + (BTN_H - 16) / 2, s, 2, sel ? RGB{ 0, 0, 0 } : C_TEXT, fill);
  }
}

static void specDrawStatic() {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  char t[32];
  snprintf(t, sizeof(t), "Spectrum: %s", BAND_LONG[specBand]);
  lcdText(4, 4, t, 1, C_ACC, C_BG);
  snprintf(t, sizeof(t), "%u-%u MHz", specLo, specHi);
  lcdText(4, 14, t, 1, C_DIM, C_BG);

  // y axis labels (dB above floor)
  char s[8];
  snprintf(s, sizeof(s), "%d", spanDb);       lcdText(2, GY, s, 1, C_DIM, C_BG);
  snprintf(s, sizeof(s), "%d", (spanDb + 1) / 2);   lcdText(2, GY + GH / 2 - 4, s, 1, C_DIM, C_BG);
  lcdText(2, GY + GH - 8, "0", 1, C_DIM, C_BG);
  lcdText(2, GY + GH - 18, "dB", 1, C_DIM, C_BG);

  // graph frame
  lcdFillC(GX - 1, GY - 1, GW + 2, 1, C_DIM);
  lcdFillC(GX - 1, GY + GH, GW + 2, 1, C_DIM);
  lcdFillC(GX - 1, GY - 1, 1, GH + 2, C_DIM);
  lcdFillC(GX + GW, GY - 1, 1, GH + 2, C_DIM);

  // channel numbers under their marker lines
  for (int c = 0; c < NUM_CH; c++) {
    int x = GX + freqToCol(FPV_MHZ[specBand][c]);
    char n[2] = { (char)('1' + c), 0 };
    lcdText(constrain(x - 3, 0, LCD_W - 6), CH_LBL_Y, n, 1, C_TEXT, C_BG);
  }
  specDrawButtons();
  if (specSweeps < specLearnUntil) drawTitleStatus(0, true);

  for (int x = 0; x < GW; x++) { colShownH[x] = -1; colShownPk[x] = -1; }
  for (int c = 0; c < NUM_CH; c++) { chValShown[c] = -2; chValHot[c] = false; }
  specFooterDirty = true;
  learnShownPct = -1; scanAnimY = -1;
}

static void specDrawColumn(int x, int h, int pk) {
  if (h == colShownH[x] && pk == colShownPk[x]) return;
  int sx = GX + x;
  RGB bg = colIsMarker[x] ? C_MARK : C_BG;
  RGB fg = scaleColor(C_HOT, 0.35f + 0.65f * h / (float)GH);
  lcdFillC(sx, GY, 1, GH - h, bg);
  lcdFillC(sx, GY + GH - h, 1, h, fg);
  if (pk > h + 1 && pk < GH) lcdFillC(sx, GY + GH - pk, 1, 1, C_PEAKH);   // peak-hold dot
  colShownH[x] = h; colShownPk[x] = pk;
}

// dB value under each channel number (blank below 3 dB, strongest in accent).
static void specDrawChannelValues(const float *flo) {
  int val[NUM_CH]; int best = -1, bestV = 2;
  for (int c = 0; c < NUM_CH; c++) {
    float pos = (FPV_MHZ[specBand][c] - specLo) / (float)SPEC_STEP_MHZ;
    int i0 = constrain((int)pos, 0, specPts - 1); int i1 = min(i0 + 1, specPts - 1); float t = pos - i0;
    float a0 = (isnan(specSmooth[i0]) || flo[i0] == 0) ? 0 : specSmooth[i0] - flo[i0];
    float a1 = (isnan(specSmooth[i1]) || flo[i1] == 0) ? 0 : specSmooth[i1] - flo[i1];
    int v = (int)lroundf(a0 + (a1 - a0) * t);
    val[c] = (v < 3) ? -1 : min(v, 99);
    if (val[c] > bestV) { bestV = val[c]; best = c; }
  }
  for (int c = 0; c < NUM_CH; c++) {
    bool hot = (c == best);
    if (val[c] == chValShown[c] && hot == chValHot[c]) continue;
    int x = GX + freqToCol(FPV_MHZ[specBand][c]);
    lcdFill(constrain(x - 7, 0, LCD_W - 14), CH_VAL_Y, 14, 8, 0, 0, 0);
    if (val[c] >= 0) {
      char s[4]; snprintf(s, sizeof(s), "%d", val[c]);
      int w = 6 * (int)strlen(s);
      lcdText(constrain(x - w / 2, 0, LCD_W - w), CH_VAL_Y, s, 1, hot ? C_ACC : C_TEXT, C_BG);
    }
    chValShown[c] = (int8_t)val[c]; chValHot[c] = hot;
  }
}

static void specDrawFooter(float peakMhz, float aboveDb, bool learning) {
  char s[40];
  learnShownPct = -1; scanAnimY = -1;
  lcdFill(0, SPEC_FOOT_Y, LCD_W, LCD_H - SPEC_FOOT_Y, 0, 0, 0);
  drawSpanLabel();
  if (learning) {
    lcdText(4, SPEC_FOOT_Y + 6, "Learning", 2, C_ACC, C_BG);
    return;
  }
  if (aboveDb < 3.0f) {
    drawScanning(SPEC_FOOT_Y + 6);
    return;
  }
  // nearest channel in this band
  int nc = 0; float nd = 1e9f;
  for (int c = 0; c < NUM_CH; c++) {
    float d = fabsf(FPV_MHZ[specBand][c] - peakMhz);
    if (d < nd) { nd = d; nc = c; }
  }
  if (nd <= 10.0f)
    snprintf(s, sizeof(s), "%u %d dB %c%d", (unsigned)lroundf(peakMhz), (int)lroundf(aboveDb), BAND_NAME[specBand], nc + 1);
  else
    snprintf(s, sizeof(s), "%u %d dB", (unsigned)lroundf(peakMhz), (int)lroundf(aboveDb));
  lcdText(4, SPEC_FOOT_Y + 6, s, 2, C_ACC, C_BG);
}

// One spectrum sweep across the selected band + redraw of changed columns.
static void specSweep() {
  static float lastPeakMhz = -1; static int lastAbove = -999; static bool lastLearning = false;
  static int lastBandDrawn = -1;
  const uint32_t learnStart = specLearnUntil - SPEC_LEARN_SWEEPS;
  const bool learningSweep = specSweeps < specLearnUntil;   // this sweep counts toward learning
  float raw[SPEC_MAX_PTS];
  bool  fb[SPEC_MAX_PTS];
  for (int i = 0; i < specPts; i++) {
    raw[i] = readFreqDb(specLo + i * SPEC_STEP_MHZ, SPEC_SAMPLES, SETTLE_US);
    fb[i] = g_lastReadFallback;
    if (evToggle || evEnterShort || evEnterLong) return;  // react to input immediately
    if (learningSweep && (i % 8 == 7 || i == specPts - 1)) {
      int done = (specSweeps - learnStart) * specPts + i + 1;
      drawLearnProgress(SPEC_FOOT_Y, done * 100 / (SPEC_LEARN_SWEEPS * specPts));
    }
  }
  specSweeps++;
  bool learning = specSweeps <= specLearnUntil;
  float *flo = specFloor[specBand];
  for (int i = 0; i < specPts; i++) {
    float v = raw[i];
    if (isnan(v)) continue;
    float &s = specSmooth[i];
    if (isnan(s)) s = v;
    else s += (v > s ? 0.6f : 0.3f) * (v - s);
    if (fb[i]) continue;                   // fallback readings never set the floor
    if (flo[i] == 0 || v < flo[i]) flo[i] = v;
    else if (!learning) flo[i] += 0.01f;
  }
  // same anti-learn clamp as the grid: no point's floor > median + 4 dB
  float tmp[SPEC_MAX_PTS];
  for (int i = 0; i < specPts; i++) tmp[i] = flo[i];
  for (int i = 1; i < specPts; i++) { float k = tmp[i]; int j = i - 1; while (j >= 0 && tmp[j] > k) { tmp[j+1] = tmp[j]; j--; } tmp[j+1] = k; }
  float med = tmp[specPts / 2];
  for (int i = 0; i < specPts; i++) if (flo[i] > med + 4.0f) flo[i] = med + 4.0f;
  if (!learning) specFloorValid[specBand] = true;

  // draw: each pixel column interpolates between sweep points
  uint32_t tDraw0 = micros();
  float bestAbove = -1e9f, bestMhz = specLo;
  for (int x = 0; x < GW; x++) {
    float pos = x * (specPts - 1) / (float)(GW - 1);
    int i0 = (int)pos; int i1 = min(i0 + 1, specPts - 1); float t = pos - i0;
    float a0 = (isnan(specSmooth[i0]) || flo[i0] == 0) ? 0 : specSmooth[i0] - flo[i0];
    float a1 = (isnan(specSmooth[i1]) || flo[i1] == 0) ? 0 : specSmooth[i1] - flo[i1];
    float above = a0 + (a1 - a0) * t;     // trace is drawn even while learning
    if (above > bestAbove) { bestAbove = above; bestMhz = specLo + pos * SPEC_STEP_MHZ; }
    int h = constrain((int)lroundf(above * GH / spanDb), 0, GH);
    colPeak[x] = max(colPeak[x] - PEAK_DECAY_PX, (float)h); // peak hold, falling
    specDrawColumn(x, h, (int)colPeak[x]);
    if ((x & 31) == 0) pollInput();
  }

  specDrawChannelValues(flo);

  // Below 3 dB the footer just says SCANNING, so noise must not trigger redraws.
  bool sig = bestAbove >= 3.0f;
  int aboveR = sig ? (int)lroundf(bestAbove) : -1;
  bool peakChanged = !learning && (aboveR != lastAbove || (sig && fabsf(bestMhz - lastPeakMhz) >= 1.0f));
  if (learning) {
    drawLearnProgress(SPEC_FOOT_Y, (specSweeps - learnStart) * 100 / SPEC_LEARN_SWEEPS);
    lastLearning = true; lastBandDrawn = specBand; specFooterDirty = false;
  } else if (lastLearning || peakChanged || lastBandDrawn != specBand || specFooterDirty) {
    specDrawFooter(bestMhz, bestAbove, false);
    lastLearning = learning; lastAbove = aboveR; lastPeakMhz = bestMhz; lastBandDrawn = specBand; specFooterDirty = false;
  }
  g_tDrawUs += micros() - tDraw0;

  static uint32_t lastRateMs = 0, rateSweeps = 0;
  rateSweeps++;
  uint32_t now = millis();
  if (now - lastRateMs >= 1000) {
    float sps = rateSweeps * 1000.0f / (now - lastRateMs);
    uint32_t sweepsThisPeriod = rateSweeps;
    rateSweeps = 0; lastRateMs = now;
    drawTitleStatus(sps, learning);
    if (serialHasRoom(600)) {
      Serial.printf("spectrum %c %u-%u MHz, %d pts, %.1f sweeps/s, peak %.0f MHz %.1f dB\n",
                    BAND_NAME[specBand], specLo, specHi, specPts, sps, bestMhz, bestAbove);
      printTiming(sweepsThisPeriod);
      printStalls();
    } else {
      resetTiming();
    }
  }
}

// ---------------------------------------------------------------- screen switching
// Page reached from page m by KEY_R (dir +1) or KEY_L (dir -1).
static ScreenMode pageAfter(ScreenMode m, int dir) {
  if (dir > 0) {
    switch (m) {
      case MODE_GRID:     return MODE_SPECTRUM;
      case MODE_SPECTRUM: return MODE_ACTIVITY;
      case MODE_ACTIVITY: return MODE_QUIET;
      default:            return MODE_GRID;       // from Quietest
    }
  }
  switch (m) {
    case MODE_GRID:     return MODE_ACTIVITY;     // skips Quietest
    case MODE_SPECTRUM: return MODE_GRID;
    case MODE_ACTIVITY: return MODE_SPECTRUM;
    default:            return MODE_ACTIVITY;     // from Quietest
  }
}

// Small centred message box; removed by loop() after POPUP_MS.
static void showPopup(const char *msg) {
  const int w = POPUP_W, h = POPUP_H, x = POPUP_X, y = POPUP_Y;
  lcdFillC(x, y, w, h, C_ACC);                       // border
  lcdFill(x + 3, y + 3, w - 6, h - 6, 30, 30, 30);
  int tw = 12 * (int)strlen(msg);
  lcdText((LCD_W - tw) / 2, y + (h - 16) / 2, msg, 2, C_TEXT, RGB{ 30, 30, 30 });
  popupUntilMs = millis() + POPUP_MS;
  if (popupUntilMs == 0) popupUntilMs = 1;
}

static void enterMode(ScreenMode m) {
  mode = m;
  popupUntilMs = 0;
  if (mode == MODE_GRID) {
    gridDrawStatic();
  } else if (mode == MODE_ACTIVITY) {
    actDrawStatic();
  } else if (mode == MODE_QUIET) {
    quietDrawStatic();
  } else {
    specSetBand(specBand);
    specDrawStatic();
  }
}

// ---------------------------------------------------------------- radio setup
static void radioInit() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_err_t e = esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
  if (e != ESP_OK) Serial.printf("set_band_mode: %s\n", esp_err_to_name(e));
  esp_wifi_set_promiscuous(true);                        // keep the receiver running
  e = esp_wifi_set_channel(149, WIFI_SECOND_CHAN_NONE);  // brings up 5 GHz cal
  if (e != ESP_OK) Serial.printf("set_channel: %s\n", esp_err_to_name(e));
  delay(50);
}

// ---------------------------------------------------------------- boot timing test
// Times the retune and a single measurement separately, so the serial log
// shows which one limits the sweep rate.
static void bootTimingTest() {
  static const uint16_t testMhz[] = { 5362, 5658, 5800, 5802, 5945 };
  Serial.println("Timing test (us):  freq  retune  measure min/avg/max");
  for (size_t k = 0; k < sizeof(testMhz) / sizeof(testMhz[0]); k++) {
    uint32_t t0 = micros();
    phy_set_chanfreq(testMhz[k], 0);
    uint32_t tune = micros() - t0;
    measureQdb();                                   // discard after retune
    uint32_t mn = 0xFFFFFFFF, mx = 0, sum = 0;
    for (int i = 0; i < 10; i++) {
      uint32_t a = micros();
      measureQdb();
      uint32_t d = micros() - a;
      sum += d; if (d < mn) mn = d; if (d > mx) mx = d;
    }
    Serial.printf("                   %u  %6lu  %lu/%lu/%lu\n", testMhz[k], (unsigned long)tune,
                  (unsigned long)mn, (unsigned long)(sum / 10), (unsigned long)mx);
  }
  uint32_t t0 = micros();
  phy_set_chanfreq(5945, 0);                        // retune to the same frequency
  Serial.printf("  same-frequency retune: %lu us, timeouts=%lu\n",
                (unsigned long)(micros() - t0), (unsigned long)g_nfTimeouts);
}

// ---------------------------------------------------------------- main
void setup() {
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  // Native USB serial: never wait on the PC. Without this, an open-but-unread
  // port (e.g. right after an IDE upload) stalls each print for up to 2 s.
  Serial.setTxBufferSize(4096);   // must be set before begin()
  Serial.setTxTimeoutMs(0);
#endif
  Serial.begin(115200);
  buildButtonOrder();          // band order for grid rows and spectrum buttons
  delay(200);
  Serial.println("\nFPV Channel Scanner (ESP32-C5)");

  pinMode(PIN_LCD_CS, OUTPUT); digitalWrite(PIN_LCD_CS, HIGH);
  pinMode(PIN_SD_CS,  OUTPUT); digitalWrite(PIN_SD_CS,  HIGH);
  pinMode(PIN_LCD_DC, OUTPUT);
  pinMode(PIN_KEY_L,     INPUT_PULLUP);
  pinMode(PIN_KEY_R,     INPUT_PULLUP);
  pinMode(PIN_BTN_ENTER, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  SPI.begin(PIN_LCD_SCK, PIN_LCD_MISO, PIN_LCD_MOSI, -1);

  lcdInit();
  exioBacklight(100);
  gridDrawStatic();
  lcdText(4, FOOT_Y + 2, "Starting radio", 2, C_DIM, C_BG);

  radioInit();
  gridResetFloor();
  for (int b = 0; b < NUM_BANDS; b++) specFloorValid[b] = false;
  specSetBand(specBand);

  uint32_t t0 = micros();
  float t = readFreqDb(5800, GRID_SAMPLES, SETTLE_US);
  Serial.printf("Self-test 5800 MHz: %.1f dB, %lu us/channel, timeouts=%lu\n",
                t, (unsigned long)(micros() - t0), (unsigned long)g_nfTimeouts);
  bootTimingTest();
  g_tTuneUs = g_tMeasUs = g_tDrawUs = 0; g_nTunes = g_nMeas = 0;
  gridInvalidate();
}

void loop() {
  if (mode == MODE_SPECTRUM) specSweep(); else gridSweep();
  pollInput();

  if (evToggle) {
    evToggle = false; evEnterShort = evEnterLong = false;
    ScreenMode next = pageAfter(mode, pageDir);
    if (next == MODE_QUIET && actTrackedS < QUIET_MIN_HISTORY_S) {
      showPopup("Wait longer");                      // not enough history yet
      Serial.printf("quietest: %lu of %d s of history, wait longer\n",
                    (unsigned long)actTrackedS, QUIET_MIN_HISTORY_S);
    } else {
      enterMode(next);
      Serial.printf("page -> %s\n", mode == MODE_GRID ? "channel activity" : mode == MODE_ACTIVITY ? "channel noise" :
                                mode == MODE_QUIET ? "quietest channels" : "spectrum");
    }
  }
  if (popupUntilMs && (int32_t)(millis() - popupUntilMs) >= 0) {
    popupUntilMs = 0;
    lcdFill(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, 0, 0, 0);   // clear it, gaps included
    actInvalidate();                         // redraw the boxes the pop-up covered
  }
  if (evEnterLong) {                        // hold: re-learn the noise floor
    evEnterLong = false;
    if (mode == MODE_GRID) { gridResetFloor(); gridInvalidate(); gridFooterDirty = true; drawTitleStatus(0, true); }
    else if (mode == MODE_ACTIVITY) { gridResetFloor(); actInvalidate(); actFooterDirty = true; drawTitleStatus(0, true); }
    else if (mode == MODE_QUIET) { gridResetFloor(); quietInvalidate(); quietFooterDirty = true; drawTitleStatus(0, true); }
    else { specFloorValid[specBand] = false; specNoSeedOnce = true; specSetBand(specBand); specDrawStatic(); }
  }
  if (evEnterShort) {
    evEnterShort = false;
    if (mode == MODE_GRID) {                // cycle brightness span
      spanDb = (spanDb >= 40) ? 15 : spanDb + 5;
      gridInvalidate(); gridFooterDirty = true;
    } else if (mode == MODE_ACTIVITY) {     // restart the activity sums
      actClear(); actInvalidate(); actFooterDirty = true;
    } else if (mode == MODE_QUIET) {        // same history as the noise page
      actClear(); quietInvalidate(); quietFooterDirty = true;
    } else {                                // next band button
      specSetBand(nextBandInButtonOrder(specBand));
      specDrawStatic();
    }
  }
  delay(1);   // let the Wi-Fi task and watchdog breathe
}
