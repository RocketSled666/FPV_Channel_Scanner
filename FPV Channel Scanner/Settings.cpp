/*
 * FPV Channel Scanner - Settings.cpp
 * Revision: 101
 *
 * User settings: the values, their limits and defaults, loading them from
 * flash at start-up and saving them, plus helpers that depend on them.
 * The table below is also what the Configuration menu lists, in
 * this order, grouped into pages by the "page" field.
 */

#include "Scanner.h"
#include <Preferences.h>

#define PREFS_NAMESPACE "fpvscan"

// Defaults here must match the "def" column of the table below.
Settings cfg = {
  15,    // span
  4,     // learnSweeps
  3,     // gridSamples   (1 when RSSI only is Off)
  4,     // specStep
  1,     // specSamples
  18,    // peakDecay10   (1.8 px)
  6,     // busyDb
  55,    // gamma100      (0.55)
  5,     // redSpread10   (0.5 dB)
  60,    // quietMinS
  VS_ANALOG,  // vidSys
  1,     // hdzLowBand
  1,     // hdzE1F1
  DJI_O4,  // djiModel
  0,     // fccChannels
  0,     // wsRace
  100,   // backlight
  37,    // imdMinSep
  0,     // imdLowBand
  700,   // longPressMs
  5000,  // configHoldMs
  5000,  // splashMs
  80,    // splashComboMs
  1500,  // popupMs
  400,   // scanDotMs
  10,    // imdRefreshS
  3000,  // moveFadeMs
  8,     // lBandLimit
  8,     // wfLogK
  8,     // measTimeoutMs
  1,     // rssiOnly
  0,     // mode11p
  MAX_MHZ_OFF,  // maxMhz
  TUNE_WIFI,    // tuneMode
  16,           // rxFilterMhz
  0,            // specAutoRange
  BS_LBL_BAND,  // bsLabel
  1,            // restorePage
};

const SettingDef SETTINGS[] = {
  // page (menu page title), name, key, value, min, max, step, def, toggle, decimals, unit, help
  // "hdzero" was the old HDZero on/off key: 0 = analog, 1 = HDZero still mean the same
  { "Video System", "Video system", "hdzero",  &cfg.vidSys,  VS_ANALOG, VS_OPEN, 1, VS_ANALOG, 0, 0, "", "Limit grids and lists to this system" },
  { "Video System", "HDZ low band", "hdzLow",  &cfg.hdzLowBand,    0,   1,  1,  1, 1, 0, "",     "HDZero: include L1-L8 (low band unlocked)" },
  { "Video System", "HDZ E1 and F1","hdzE1F1", &cfg.hdzE1F1,       0,   1,  1,  1, 1, 0, "",     "HDZero: include E1 and F1" },
  { "Video System", "DJI model",    "djiModel",&cfg.djiModel,  DJI_V1, DJI_O4_RACE, 1, DJI_O4, 0, 0, "", "V1/Vista, O3, O4, or O4 race mode (R1-R8)" },
  { "Video System", "WS race mode", "wsRace",  &cfg.wsRace,        0,   1,  1,  0, 1, 0, "",     "Walksnail race mode: R1-R8" },
  { "Video System", "FCC channels", "fccCh",   &cfg.fccChannels,   0,   1,  1,  0, 1, 0, "",     "DJI / Walksnail: FCC (unlocked) channels" },
  { "Radio", "RSSI only",       "rssiOnly",  &cfg.rssiOnly,      0,   1,  1,  1, 1, 0, "",     "Signal RSSI (On) or noise-floor estimator (Off)" },
  { "Radio", "Tuning",          "tuneMode",  &cfg.tuneMode,  TUNE_DIRECT, TUNE_WIFI_CH, 1, TUNE_WIFI, 0, 0, "", "Wi-Fi path may hear better above 5750 MHz" },
  { "Radio", "RX filter",       "rxFilt",    &cfg.rxFilterMhz, RXF_MIN, RXF_AUTO, 1, 16, 0, 0, " MHz", "Analog filter width; narrower = sharper" },
  { "Radio", "Max MHz",         "maxMhz",    &cfg.maxMhz,    5800, MAX_MHZ_OFF, 5, MAX_MHZ_OFF, 0, 0, " MHz", "Channels above this show -- (bad top band)" },
  { "Radio", "Meas timeout",    "measTmo",   &cfg.measTimeoutMs, 4,  20,  1,  8, 0, 0, " ms",  "Give up on a noise-floor reading after this" },
  { "Radio", "802.11p mode",    "mode11p",   &cfg.mode11p,       0,   1,  1,  0, 1, 0, "",     "May help 5750-5990 MHz (experimental)" },
  { "Scanning", "Samples/channel", "gridSmp",   &cfg.gridSamples,   1,   8,  1,  3, 0, 0, "",     "Grids: readings averaged per channel" },
  { "Scanning", "Learn sweeps",    "learn",     &cfg.learnSweeps,   1,  20,  1,  4, 0, 0, "",     "Sweeps used to learn the noise floor" },
  { "Scanning", "Spectrum step",            "specStep",  &cfg.specStep,      2,  10,  1,  4, 0, 0, " MHz", "Spectrum sweep step; channels hit exactly" },
  { "Scanning", "Spectrum samples",   "specSmp",   &cfg.specSamples,   1,   8,  1,  1, 0, 0, "",     "Spectrum: readings per point" },
  { "Channel Selection", "Busy level",      "busyDb",    &cfg.busyDb,        1,  15,  1,  6, 0, 0, " dB",  "Counts as busy; Band Scan peaks turn red" },
  { "Channel Selection", "Min history",     "quietMin",  &cfg.quietMinS,     0, 600, 10, 60, 0, 0, " s",   "Noise history needed before Quietest opens" },
  { "Channel Selection", "L Band Limit",    "lLimit",    &cfg.lBandLimit,    1,   8,  1,  8, 0, 0, "",     "Max L-band channels in Quietest and IMD lists" },
  { "Channel Selection", "IMD min spacing",     "imdSep",    &cfg.imdMinSep,    20,  80,  1, 37, 0, 0, " MHz", "Smallest gap allowed between channels in a set" },
  { "Channel Selection", "IMD use L band",      "imdLow",    &cfg.imdLowBand,    0,   1,  1,  0, 1, 0, "",     "Allow L1-L8 (not used with a video system)" },
  { "Channel Selection", "IMD refresh",   "imdRefresh", &cfg.imdRefreshS,     2,    60,    1,   10, 0, 0, " s",  "How often IMD-free sets are re-picked" },
  { "Display", "Span",            "span",      &cfg.span,          5,  60,  5, 15, 0, 0, " dB",  "Activity full brightness, Spectrum height" },
  { "Display", "Auto range",      "specAuto",  &cfg.specAutoRange, 0,  1,  1,  0, 1, 0, "",     "Activity, Spectrum, Band Scan fit peaks" },
  { "Display", "Peak labels",     "bsLabel",   &cfg.bsLabel,    BS_LBL_BAND, BS_LBL_BOTH, 1, BS_LBL_BAND, 0, 0, "", "Band Scan: label on peaks over Busy level" },
  { "Display", "Peak decay",      "peakDecay", &cfg.peakDecay10,   2, 100,  2, 18, 0, 1, " px",  "Spectrum: peak-hold dot fall per sweep" },
  { "Display", "Waterfall log",   "wfLog",     &cfg.wfLogK,        0,  20,  1,  8, 0, 0, "",     "Spectrum waterfall: 0 = linear, higher = log" },
  { "Display", "Shading gamma",   "gamma",     &cfg.gamma100,     20, 100,  5, 55, 0, 2, "",     "Noise grid: below 1 brightens small diffs" },
  { "Display", "Full red spread", "redSpread", &cfg.redSpread10,   1,  50,  1,  5, 0, 1, " dB",  "Noise grid: spread shown as full red" },
  { "Display", "Move fade",     "moveFade",   &cfg.moveFadeMs,      0, 10000,  250, 3000, 0, 0, " ms", "Quietest/IMD: red fade on a moved channel" },
  { "Controls", "Long press",    "longPress",  &cfg.longPressMs,   300, 2000,  100,  700, 0, 0, " ms", "ENTER held this long = long press" },
  { "Controls", "Config hold",   "cfgHold",    &cfg.configHoldMs, 2000, 10000, 1000, 5000, 0, 0, " ms", "Left + Right held this long opens Configuration" },
  { "Controls", "Splash time",   "splashMs",   &cfg.splashMs,        0, 10000,  500, 5000, 0, 0, " ms", "How long the start-up splash shows (0 = none)" },
  { "Controls", "Splash combo",  "splashCmb",  &cfg.splashComboMs,  20,   500,   10,   80, 0, 0, " ms", "Left + Right held this long on splash = Config" },
  { "Controls", "Last page",     "restorePg",  &cfg.restorePage,     0,     1,    1,    1, 1, 0, "",    "Start on the last page shown (Off = Activity)" },
  { "Controls", "Pop-up time",   "popupMs",    &cfg.popupMs,       500,  5000,  250, 1500, 0, 0, " ms", "How long the Wait pop-up stays up" },
  { "Controls", "Scan dot step", "scanDot",    &cfg.scanDotMs,     100,  1000,   50,  400, 0, 0, " ms", "Speed of the Scanning dots" },
  { "Controls", "Backlight",     "backlight",  &cfg.backlight,     10,   100,   10,  100, 0, 0, "%",   "LCD brightness" },
};
const int NUM_SETTINGS = sizeof(SETTINGS) / sizeof(SETTINGS[0]);

// Keep a value inside its limits and on its step grid.
static int clampSetting(const SettingDef &d, int v) {
  if (v < d.minV) v = d.minV;
  if (v > d.maxV) v = d.maxV;
  v = d.minV + ((v - d.minV) / d.step) * d.step;
  return v;
}

void loadSettings() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false);
  for (int i = 0; i < NUM_SETTINGS; i++) {
    const SettingDef &d = SETTINGS[i];
    int v = d.isToggle ? (prefs.getBool(d.key, d.def != 0) ? 1 : 0)   // toggles stored as bool
                       : prefs.getInt(d.key, d.def);
    *d.value = clampSetting(d, v);
  }
  prefs.end();
}

// Writes only the values that differ from what's stored, to spare the flash.
void saveSettings() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false);
  for (int i = 0; i < NUM_SETTINGS; i++) {
    const SettingDef &d = SETTINGS[i];
    if (d.isToggle) {
      bool v = *d.value != 0;
      if (!prefs.isKey(d.key) || prefs.getBool(d.key, !v) != v) prefs.putBool(d.key, v);
    } else {
      if (!prefs.isKey(d.key) || prefs.getInt(d.key, *d.value + 1) != *d.value) prefs.putInt(d.key, *d.value);
    }
  }
  prefs.end();
}

void resetSettingsToDefaults() {
  for (int i = 0; i < NUM_SETTINGS; i++) *SETTINGS[i].value = SETTINGS[i].def;
}

// Span is also changed with ENTER on the Activity page; saved on its own.
void saveSpan() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false);
  if (prefs.getInt("span", -1) != cfg.span) prefs.putInt("span", cfg.span);
  prefs.end();
}

// Last page shown and the spectrum band, saved apart from the settings and
// restored at start-up when "Last page" is On. A saved Quietest page
// opens as the one that matches the current Video system setting.
ScreenMode loadLastPage() {
  if (!cfg.restorePage) return MODE_GRID;
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, true);
  int m = prefs.getInt("page", MODE_GRID);
  int b = prefs.getInt("specBand", specBand);
  prefs.end();
  if (b >= 0 && b < NUM_BANDS) specBand = b;
  if (m < MODE_GRID || m > MODE_BANDSCAN) m = MODE_GRID;
  if (m == MODE_QUIET && sysOn()) m = MODE_HDZERO;
  if (m == MODE_HDZERO && !sysOn()) m = MODE_QUIET;
  return (ScreenMode)m;
}

// Writes only what changed.
void saveLastPage() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false);
  if (prefs.getInt("page", -1) != (int)mode) prefs.putInt("page", (int)mode);
  if (prefs.getInt("specBand", -1) != specBand) prefs.putInt("specBand", specBand);
  prefs.end();
}

// Video System page: only the settings for the selected system are shown
// (FCC channels not with a race mode). A setting that hides others comes
// before them on the page, so changing it never moves the selected row.
bool settingVisible(int i) {
  const int *v = SETTINGS[i].value;
  if (v == &cfg.hdzLowBand || v == &cfg.hdzE1F1) return cfg.vidSys == VS_HDZERO;
  if (v == &cfg.djiModel)    return cfg.vidSys == VS_DJI;
  if (v == &cfg.wsRace)      return cfg.vidSys == VS_WALKSNAIL;
  if (v == &cfg.fccChannels) return (cfg.vidSys == VS_DJI && cfg.djiModel != DJI_O4_RACE) ||
                                    (cfg.vidSys == VS_WALKSNAIL && !cfg.wsRace);
  return true;
}

// Value as shown in the menu: "On"/"Off", or the number with its unit.
void formatSetting(int i, char *buf, int len) {
  const SettingDef &d = SETTINGS[i];
  int v = *d.value;
  if (measTimeoutLocked(i)) { snprintf(buf, len, "%d ms (11p)", MEAS_TIMEOUT_11P_MS); return; }
  if (d.value == &cfg.maxMhz && v >= MAX_MHZ_OFF) { snprintf(buf, len, "Off"); return; }
  if (d.value == &cfg.rxFilterMhz && v >= RXF_AUTO) { snprintf(buf, len, "Auto"); return; }
  if (d.value == &cfg.bsLabel) {
    snprintf(buf, len, "%s", v == BS_LBL_MHZ ? "MHz" : v == BS_LBL_BOTH ? "Both" : "Band");
    return;
  }
  if (d.value == &cfg.vidSys) {
    static const char *const N[] = { "All bands/chans", "HDZero", "DJI", "Walksnail", "OpenIPC" };
    snprintf(buf, len, "%s", N[constrain(v, 0, 4)]);
    return;
  }
  if (d.value == &cfg.djiModel) {
    static const char *const N[] = { "V1/Vista", "O3", "O4", "O4 race" };
    snprintf(buf, len, "%s", N[constrain(v, 0, 3)]);
    return;
  }
  if (d.value == &cfg.tuneMode) {
    snprintf(buf, len, "%s", v == TUNE_WIFI ? "Wi-Fi" : v == TUNE_WIFI_CH ? "Wi-Fi+chan" : "Direct");
    return;
  }
  if (d.isToggle)          snprintf(buf, len, "%s", v ? "On" : "Off");
  else if (d.decimals == 1) snprintf(buf, len, "%d.%d%s", v / 10, v % 10, d.unit);
  else if (d.decimals == 2) snprintf(buf, len, "%d.%02d%s", v / 100, v % 100, d.unit);
  else                      snprintf(buf, len, "%d%s", v, d.unit);
}

// "Meas timeout" is fixed at MEAS_TIMEOUT_11P_MS while 802.11p mode is on;
// the saved value is kept and used again when 802.11p is turned off.
bool measTimeoutLocked(int i) {
  return SETTINGS[i].value == &cfg.measTimeoutMs && cfg.mode11p;
}

// Step a numeric setting by +1 / -1 steps, staying inside its limits.
void stepSetting(int i, int dir) {
  const SettingDef &d = SETTINGS[i];
  *d.value = clampSetting(d, *d.value + dir * d.step);
}

// True if an HDZero VTX can use this channel, given the HDZero settings.
bool hdzeroOk(int b, int c) {
  if (!HDZERO_OK[b][c]) return false;
  if (BAND_NAME[b] == 'L' && !cfg.hdzLowBand) return false;
  if (!cfg.hdzE1F1 && ((BAND_NAME[b] == 'E' && c == 0) || (BAND_NAME[b] == 'F' && c == 0))) return false;
  return true;
}

// ---------------------------------------------------------------- video systems
// The selected system's channels, each tied to the nearest measured grid
// channel within SYS_MATCH_MHZ (none: the channel can't be shown). Rebuilt
// whenever a setting it depends on changes.
static SysChan sysCh[SYS_MAX_CH];
static int     sysN = 0;

static void sysAdd(const char *name, uint16_t mhz) {
  static const int PREF[NUM_BANDS] = { 4, 3, 2, 0, 1, 5 };   // ties: R F E A B L
  int bb = -1, bc = -1, bd = SYS_MATCH_MHZ + 1;
  for (int p = 0; p < NUM_BANDS; p++)
    for (int c = 0; c < NUM_CH; c++) {
      int b = PREF[p], d = abs((int)FPV_MHZ[b][c] - (int)mhz);
      if (d < bd) { bd = d; bb = b; bc = c; }
    }
  if (bb < 0 || sysN >= SYS_MAX_CH) return;              // nothing measured near it
  SysChan &s = sysCh[sysN++];
  snprintf(s.name, sizeof(s.name), "%s", name);
  s.mhz = mhz; s.b = bb; s.c = bc;
}

static void sysAddRaceband() {
  for (int c = 0; c < NUM_CH; c++) {
    char n[6]; snprintf(n, sizeof(n), "R%d", c + 1);
    sysAdd(n, FPV_MHZ[4][c]);                              // band 4 = R
  }
}

static void sysBuild() {
  sysN = 0;
  char n[6];
  switch (cfg.vidSys) {
    case VS_HDZERO: {
      static const int ORDER[NUM_BANDS] = { 4, 3, 2, 5, 0, 1 };   // R F E L (A B: none)
      for (int p = 0; p < NUM_BANDS; p++)
        for (int c = 0; c < NUM_CH; c++) {
          int b = ORDER[p];
          if (!hdzeroOk(b, c)) continue;
          snprintf(n, sizeof(n), "%c%d", BAND_NAME[b], c + 1);
          sysAdd(n, FPV_MHZ[b][c]);
        }
      break; }
    case VS_DJI:
      if (cfg.djiModel == DJI_O4_RACE) { sysAddRaceband(); break; }
      if (cfg.djiModel == DJI_V1) {
        for (int i = 0; i < 8; i++)
          if (cfg.fccChannels || V1_STOCK[i]) { snprintf(n, sizeof(n), "CH%d", i + 1); sysAdd(n, V1_MHZ[i]); }
      } else {
        const uint16_t *f = cfg.djiModel == DJI_O3 ? O3_MHZ : O4_MHZ;
        const bool *st    = cfg.djiModel == DJI_O3 ? O3_STOCK : O4_STOCK;
        for (int i = 0; i < 7; i++)
          if (cfg.fccChannels || st[i]) { snprintf(n, sizeof(n), "CH%d", i + 1); sysAdd(n, f[i]); }
      }
      break;
    case VS_WALKSNAIL:
      if (cfg.wsRace) { sysAddRaceband(); break; }
      for (int i = 0; i < 8; i++)
        if (cfg.fccChannels || V1_STOCK[i]) { snprintf(n, sizeof(n), "CH%d", i + 1); sysAdd(n, V1_MHZ[i]); }
      break;
    case VS_OPEN:
      for (unsigned i = 0; i < sizeof(WIFI_CH); i++) {
        snprintf(n, sizeof(n), "%u", WIFI_CH[i]);
        sysAdd(n, 5000 + 5 * WIFI_CH[i]);
      }
      break;
  }
}

// Rebuilds the table when a setting it depends on changed (cheap check).
int sysSig() {
  return cfg.vidSys | cfg.djiModel << 4 | cfg.fccChannels << 8 | cfg.wsRace << 9 |
         cfg.hdzLowBand << 10 | cfg.hdzE1F1 << 11;
}

static void sysCheck() {
  static int sig = -1;
  int s = sysSig();
  if (s != sig) { sig = s; sysBuild(); }
}

// The selected system's channels in table order (none for All bands/chans).
int sysCount() { sysCheck(); return sysOn() ? sysN : 0; }
const SysChan *sysChan(int i) { sysCheck(); return (i >= 0 && i < sysN) ? &sysCh[i] : nullptr; }

bool sysOn() { return cfg.vidSys != VS_ANALOG; }

const SysChan *sysAt(int b, int c) {
  sysCheck();
  for (int i = 0; i < sysN; i++) if (sysCh[i].b == b && sysCh[i].c == c) return &sysCh[i];
  return nullptr;
}

bool sysOk(int b, int c) { return sysAt(b, c) != nullptr; }

const char *sysName() {
  static const char *const N[] = { "All bands/chans", "HDZero", "DJI", "Walksnail", "OpenIPC" };
  return N[constrain(cfg.vidSys, 0, 4)];
}

const char *sysTag() {
  static const char *const T[] = { "", "HDZ", "DJI", "WS", "IPC" };
  return T[constrain(cfg.vidSys, 0, 4)];
}

// Footer line on the system's Quietest and IMD-free pages, e.g.
// "HDZero only: R1-8, L1-8, E1, F1, F2, F4" or
// "DJI O3 FCC only: 7 ch, 5669-5912 MHz".
void sysChannelsText(char *buf, int len) {
  sysCheck();
  if (cfg.vidSys == VS_HDZERO) {
    snprintf(buf, len, "HDZero only: R1-8%s%s, F2, F4",
             cfg.hdzLowBand ? ", L1-8" : "", cfg.hdzE1F1 ? ", E1, F1" : "");
    return;
  }
  static const char *const DM[] = { "V1/Vista", "O3", "O4", "O4 race" };
  char what[32];
  if (cfg.vidSys == VS_DJI)
    snprintf(what, sizeof(what), "DJI %s%s", DM[constrain(cfg.djiModel, 0, 3)],
             cfg.djiModel == DJI_O4_RACE ? "" : cfg.fccChannels ? " FCC" : " stock");
  else if (cfg.vidSys == VS_WALKSNAIL)
    snprintf(what, sizeof(what), "Walksnail %s", cfg.wsRace ? "race" : cfg.fccChannels ? "FCC" : "stock");
  else snprintf(what, sizeof(what), "OpenIPC (Wi-Fi)");
  uint16_t lo = 9999, hi = 0;
  for (int i = 0; i < sysN; i++) { lo = min(lo, sysCh[i].mhz); hi = max(hi, sysCh[i].mhz); }
  if (sysN == 0) snprintf(buf, len, "%s: no measurable channels", what);
  else snprintf(buf, len, "%s only: %d ch, %u-%u MHz", what, sysN, lo, hi);
}

// Grid pages: with a video system selected, only the measured channels that
// stand in for its channels get a box.
bool gridShown(int b, int c) {
  return !sysOn() || sysOk(b, c);
}

// A channel whose readings are mostly held (measurement stalls with no
// calibrated RSSI to stand in) has no live data: the grids show "--" and the
// Noise ranking, Quietest lists and IMD-free sets leave it out.
// Channels above "Max MHz" count as no data too.
bool chanNoData(int b, int c) {
  return gridNoData[b][c] || mhzBlocked(FPV_MHZ[b][c]);
}

// Above "Max MHz": not measured (this board reads junk up there).
bool mhzBlocked(float mhz) {
  return mhz > cfg.maxMhz;
}

// Spectrum band buttons: with a video system selected, only bands holding
// at least one of its channels can be selected.
bool bandEnabled(int b) {
  if (!sysOn()) return true;
  for (int c = 0; c < NUM_CH; c++) if (sysOk(b, c)) return true;
  return false;
}
