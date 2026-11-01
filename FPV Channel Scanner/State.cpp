/*
 * FPV Channel Scanner - State.cpp
 * Revision: 41
 *
 * Definitions of all global state shared between the pages.
 */

#include "Scanner.h"

uint32_t g_nfTimeouts = 0;
StallStat g_stalls[MAX_STALL_STATS];
int       g_nStalls = 0;
bool      g_lastReadFallback = false;   // last readFreqDb() used the RSSI fallback
// timing statistics, printed and reset once per second
uint64_t g_tTuneUs = 0, g_tMeasUs = 0, g_tDrawUs = 0;
uint32_t g_nTunes = 0, g_nMeas = 0;
ScreenMode mode = MODE_GRID;
// Band display order (by lowest frequency), shared by the grid rows and the
// spectrum buttons. Filled by buildButtonOrder() at startup.
int  btnOrder[NUM_BANDS] = { 5, 2, 4, 0, 1, 3 };   // L E R A B F
int  bandRow[NUM_BANDS]  = { 3, 4, 1, 5, 2, 0 };   // inverse of btnOrder
int  spanDb = 15;            // dB above floor = full brightness (grid) and full height (spectrum)

// grid
float  gridSmooth[NUM_BANDS][NUM_CH];
float  gridFloor[NUM_BANDS][NUM_CH];
int8_t gridShownLevel[NUM_BANDS][NUM_CH];
bool   gridShownPeak[NUM_BANDS][NUM_CH];

// activity page
// Long-term statistics per channel, since start or the last reset.
double   actSumDb[NUM_BANDS][NUM_CH];   // sum of dB above floor, all readings
uint32_t actN[NUM_BANDS][NUM_CH];       // readings counted
uint32_t actBusyN[NUM_BANDS][NUM_CH];   // readings at least ACT_BUSY_DB above floor
float    actPeakDb[NUM_BANDS][NUM_CH];  // strongest reading above floor
float    actTrackedS = 0;               // seconds of history collected
uint32_t actLastMs = 0;                 // time of the last accumulation, 0 = none yet
int8_t actShownLevel[NUM_BANDS][NUM_CH];
bool   actShownPeak[NUM_BANDS][NUM_CH];
int8_t actShownRank[NUM_BANDS][NUM_CH];  // rank drawn in the box, 0 = none

// quietest-channels page: what each list slot currently shows
int16_t quietShownIdx[QUIET_LIST_N];      // band*NUM_CH + channel, -1 = empty
int16_t quietShownTenths[QUIET_LIST_N];   // avg dB x10
int8_t  quietShownBusy[QUIET_LIST_N];     // busy %
bool    quietFooterDirty = true;
uint32_t popupUntilMs = 0;               // pop-up on screen until this time, 0 = none
bool   actFooterDirty = true;
uint32_t gridSweeps = 0, gridLearnUntil = GRID_LEARN_SWEEPS;

// spectrum
int      specBand = 4;       // start on Raceband
uint16_t specLo, specHi;     // MHz
int      specPts;
float    specSmooth[SPEC_MAX_PTS];
float    specFloor[NUM_BANDS][SPEC_MAX_PTS];
bool     specFloorValid[NUM_BANDS];
uint32_t specSweeps = 0, specLearnUntil = 0;
bool     specNoSeedOnce = false;   // ENTER-hold re-learn: start from scratch
int16_t  colShownH[GW];      // bar height currently drawn per column
int16_t  colShownPk[GW];     // peak-hold row currently drawn per column
float    colPeak[GW];        // peak-hold value per column (px)
bool     colIsMarker[GW];
int8_t   chValShown[NUM_CH];  // dB value currently drawn under each channel
bool     chValHot[NUM_CH];

// input
ButtonState btnKeyL  = { PIN_KEY_L,     false, 0, false };
ButtonState btnKeyR  = { PIN_KEY_R,     false, 0, false };
ButtonState btnEnter = { PIN_BTN_ENTER, false, 0, false };
bool evEnterShort = false, evEnterLong = false, evToggle = false;
int  pageDir = 1;              // +1 = KEY_R (forward), -1 = KEY_L (back)
bool hdzeroMode = false;       // chosen at start-up: HDZero page + outlines
uint32_t lastInputPollMs = 0;
bool  gridFooterDirty = true, specFooterDirty = true;
int   learnShownPct = -1, learnShownY = -1;   // progress bar currently drawn
int   scanAnimY = -1;          // y of the "Scanning" text on screen, -1 = not shown
int   scanDots = 0;
uint32_t scanDotsMs = 0;
