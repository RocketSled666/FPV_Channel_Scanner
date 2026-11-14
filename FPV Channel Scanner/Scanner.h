/*
 * FPV Channel Scanner - Scanner.h
 * Revision: 103
 *
 * Shared declarations: every global variable (defined in State.cpp) and
 * every function, grouped by the file that defines it. Each .cpp file
 * includes this header.
 */

#pragma once

#include "Config.h"

// ---------------------------------------------------------------- settings (Settings.cpp)
extern Settings cfg;
extern const SettingDef SETTINGS[];
extern const int NUM_SETTINGS;
void loadSettings();
void saveSettings();
void resetSettingsToDefaults();
void saveSpan();
ScreenMode loadLastPage();        // last page shown (sets specBand too)
void saveLastPage();              // current page and spectrum band
void formatSetting(int i, char *buf, int len);
bool settingVisible(int i);       // shown in the menu (Video System page: per system)
void stepSetting(int i, int dir);
bool hdzeroOk(int b, int c);
// Video system channels ("Video system" setting): name ("R1", "CH3", "149"),
// the system's own frequency, and the measured grid channel standing in for it.
struct SysChan { char name[6]; uint16_t mhz; int8_t b, c; };
bool sysOn();                     // a video system other than analog is selected
bool sysOk(int b, int c);         // grid channel b,c stands in for a system channel
const SysChan *sysAt(int b, int c);   // that system channel, or nullptr
const char *sysName();            // "HDZero", "DJI", "Walksnail", "OpenIPC"
const char *sysTag();             // "HDZ", "DJI", "WS", "IPC" (grid corner marker)
void sysChannelsText(char *buf, int len);   // footer line: what the system uses
int sysCount();                   // the system's channels (0 for All bands/chans)...
const SysChan *sysChan(int i);    // ...and each one, in table order
int sysSig();                     // changes when any setting the channel table depends on changes
void drawSysMarker();             // blue video system tag ("HDZ") at the top left, under the title
bool gridShown(int b, int c);
bool bandEnabled(int b);          // spectrum band button usable (HDZero mode: band has an HDZero channel)     // false = box left blank (HDZero mode, channel HDZero can't use)

// ---------------------------------------------------------------- Splash.cpp
void showSplash();
bool splashWait(uint32_t startMs);

// ---------------------------------------------------------------- Config Menu.cpp
void runConfigMenu();

// ---------------------------------------------------------------- globals (State.cpp)
extern uint32_t g_nfTimeouts;
extern StallStat g_stalls[MAX_STALL_STATS];
extern int g_nStalls;
extern bool g_lastReadFallback;   // last readFreqDb() stalled (calibrated RSSI or held value)
extern bool g_lastReadHeld;       // ...and repeated the last good value (no live data)
extern float gridHeldFrac[NUM_BANDS][NUM_CH];   // recent share of held readings per channel
extern bool  gridNoData[NUM_BANDS][NUM_CH];     // channel currently "no data" (with hysteresis)
bool chanNoData(int b, int c);
bool mhzBlocked(float mhz);       // above the "Max MHz" setting
bool measTimeoutLocked(int i);    // setting i is "Meas timeout" and 802.11p mode is on (fixed 12 ms)    // channel's readings are mostly held: show "--", leave out of lists
void printRssiStats();            // serial: "RSSI only" read statistics (when on)
// timing statistics, printed and reset once per second
extern uint64_t g_tTuneUs, g_tMeasUs, g_tDrawUs;
extern uint32_t g_nTunes, g_nMeas;
extern ScreenMode mode;
// Band display order (by lowest frequency), shared by the grid rows and the
// spectrum buttons. Filled by buildButtonOrder() at startup.
extern int btnOrder[NUM_BANDS];   // L E R A B F
extern int bandRow[NUM_BANDS];   // inverse of btnOrder

// grid
extern float gridSmooth[NUM_BANDS][NUM_CH];
extern float gridFloor[NUM_BANDS][NUM_CH];
extern int8_t gridShownLevel[NUM_BANDS][NUM_CH];
extern bool gridShownPeak[NUM_BANDS][NUM_CH];

// activity page
// Long-term statistics per channel, since start or the last reset.
extern double actSumDb[NUM_BANDS][NUM_CH];   // sum of dB above floor, all readings
extern uint32_t actN[NUM_BANDS][NUM_CH];       // readings counted
extern uint32_t actBusyN[NUM_BANDS][NUM_CH];   // readings at least ACT_BUSY_DB above floor
extern float actPeakDb[NUM_BANDS][NUM_CH];  // strongest reading above floor
extern float actTrackedS;               // seconds of history collected
extern uint32_t actLastMs;                 // time of the last accumulation, 0 = none yet
extern int8_t actShownLevel[NUM_BANDS][NUM_CH];
extern bool actShownPeak[NUM_BANDS][NUM_CH];
extern int8_t actShownRank[NUM_BANDS][NUM_CH];  // rank drawn in the box, 0 = none

// quietest-channels page: what each list slot currently shows
extern int16_t quietShownIdx[QUIET_LIST_N];      // band*NUM_CH + channel, -1 = empty
extern int16_t quietShownTenths[QUIET_LIST_N];   // avg dB x10
extern int8_t quietShownBusy[QUIET_LIST_N];     // busy %
extern bool quietFooterDirty;
extern uint32_t popupUntilMs;               // pop-up on screen until this time, 0 = none
extern bool actFooterDirty;
extern uint32_t gridSweeps, gridLearnUntil;   // set by gridResetFloor() at start-up

// spectrum
extern int specBand;       // start on Raceband
extern uint16_t specLo, specHi;     // MHz
extern int specPts;
extern float specSmooth[SPEC_MAX_PTS];
extern uint16_t specMhz[SPEC_MAX_PTS];    // frequency of each sweep point (channels snapped exactly)
extern int16_t specChPt[NUM_CH];         // sweep point index that sits on each channel
extern float specFloor[NUM_BANDS][SPEC_MAX_PTS];
extern float g_floorMhz[FLOOR_N];    // shared floor by frequency, 0 = none
extern uint32_t specSweeps, specLearnUntil;
extern bool specNoSeedOnce;   // ENTER-hold re-learn: start from scratch
extern int16_t colShownH[GW];      // bar height currently drawn per column
extern int16_t colShownPk[GW];     // peak-hold row currently drawn per column
extern float colPeak[GW];        // peak-hold value per column (px)
extern bool colIsMarker[GW];
extern int8_t chValShown[NUM_CH];  // dB value currently drawn under each channel
extern bool chValHot[NUM_CH];

// input
extern ButtonState btnKeyL;
extern ButtonState btnKeyR;
extern ButtonState btnEnter;
extern bool evEnterShort, evEnterLong, evToggle;
extern bool evConfig;                // KEY_L + KEY_R held CONFIG_HOLD_MS: open the menu
extern int pageDir;              // +1 = KEY_R (forward), -1 = KEY_L (back)
extern uint32_t lastInputPollMs;
extern bool gridFooterDirty, specFooterDirty;
extern int learnShownPct, learnShownY;   // progress bar currently drawn
extern int scanAnimY;          // y of the "Scanning" text on screen, -1 = not shown
extern int scanDots;
extern uint32_t scanDotsMs;

// ---------------------------------------------------------------- Display.cpp
void exioWrite(const uint8_t *d, size_t n);
void exioPin(uint8_t pin, bool level);
void exioBacklight(uint8_t pct);
void lcdCmd(uint8_t c, const uint8_t *data, size_t n);
void lcdCmd0(uint8_t c);
void lcdInit();
void lcdWindow(int x, int y, int w, int h);
void lcdFill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
void lcdFillC(int x, int y, int w, int h, RGB c);
void lcdText(int x, int y, const char *s, int scale, RGB fg, RGB bg);
void lcdDrawRgbRow(int x, int y, int w, const uint8_t *rgb);
void lcdTextSized(int x, int y, const char *s, int cw, int ch, RGB fg, RGB bg);
void lcdTextV(int x, int y, const char *s, RGB fg, RGB bg);   // rotated 90 degrees, reads upward
const uint8_t *wfColor(uint8_t level);   // waterfall colour (Page Spectrum.cpp)
uint8_t wfLevelOf(float above);          // waterfall level for dB above the floor
RGB scaleColor(RGB c, float k);

// ---------------------------------------------------------------- Input.cpp
void pollButton(ButtonState &b, uint32_t now, bool isEnter);
void pollInput();

// ---------------------------------------------------------------- Radio.cpp
int16_t measureQdb();
void noteStall(uint16_t mhz, float fallbackDb);
void printStalls();
float readFreqDb(uint16_t mhz, int samples, uint32_t settleUs);
void printTiming(uint32_t sweeps);
void resetTiming();
bool serialHasRoom(int bytes);
void radioInit();
void bootTimingTest();

// ---------------------------------------------------------------- Bands.cpp
void buildButtonOrder();
int nextBandInButtonOrder(int band);

// ---------------------------------------------------------------- Screen Common.cpp
void drawGridFrame(const char *title, RGB titleColor);
void drawGridBox(int x, int y, RGB fill, bool highlight);
void drawNoDataBox(int x, int y, int b, int c);   // grid box for a channel with no live data
void updateScanDots();
void drawScanning(int y);
void drawSpanLabel();
int gridRangeNow();               // current scale top (dB) of the Activity grid...
int specRangeNow();               // ...the Spectrum graph...
int bsRangeNow();                 // ...and the Band Scan graph (the span, or more with Auto range)
void drawLearnProgress(int footY, int pct);
void drawTitleStatus(float sweepsPerSec, bool learning);
ScreenMode pageAfter(ScreenMode m, int dir);
void drawHintLine(const char *l, const char *e, const char *r);
void drawPageHints();
void drawLongPressLegend(const char *what);
void showPopup(const char *msg);
void enterMode(ScreenMode m);

// ---------------------------------------------------------------- Page Activity.cpp
void gridResetFloor();
void gridInvalidate();
void gridDrawStatic();
void gridDrawCell(int b, int c, int level, bool peak);
void gridDrawFooter(int pb, int pc, float aboveDb);
void gridSweep();
void  floorStore(uint16_t mhz, float v);   // a page's learned floor at mhz
float floorKnownAt(float mhz);              // stored / interpolated floor, 0 = none
void  gridSeed();                           // grid floor from the shared floor (learn if too little)

// ---------------------------------------------------------------- Page Noise.cpp
void actClear();
void actInvalidate();
void actDrawStatic();
void actAccumulate(float raw[NUM_BANDS][NUM_CH]);
void actRankOrder(int *order);
float actAvg(int b, int c);
int actBusyPct(int b, int c);
void actDrawCell(int b, int c, int level, bool peak, int rank);
void actDrawFooter(int pb, int pc, float excessDb);
void actDraw(bool learning, uint32_t learnStart);

// ---------------------------------------------------------------- Page Quietest.cpp
bool quietPage();
void quietInvalidate();
void quietDrawStatic();
void quietDrawEntry(int slot, int idx);
void quietDraw(bool learning, uint32_t learnStart);
extern bool imdHold;              // IMD-Free page: sets held (kept when the page is left)
void imdToggleBestFirst();        // IMD-Free page, held: ENTER held = Quietest first / Best IMD first
void imdEnterShort();            // IMD-Free page: ENTER = Hold / Release, or Help / Close with no sets
void imdToggleHold();             // IMD-Free page: ENTER freezes / releases the sets
void imdRepaint();                // IMD-Free page: redraw the table after the pop-up

// ---------------------------------------------------------------- Page Spectrum.cpp
int freqToCol(float mhz);
void specSetBand(int band);
void specDrawButtons();
void specDrawStatic();
void specDrawColumn(int x, int h, int pk);
void specDrawChannelValues(const float *flo);
void specDrawFooter(float peakMhz, float aboveDb, bool learning);
void specSweep();

// ---------------------------------------------------------------- Page Band Scan.cpp
void bscanDrawStatic();
void bscanEnter();
void bscanRelearn();
void bscanTogglePause();
void bscanSweep();
