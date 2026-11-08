/*
 * FPV Channel Scanner - Settings.cpp
 * Revision: 94
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
  0,     // hdzero
  1,     // hdzLowBand
  1,     // hdzE1F1
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
};

const SettingDef SETTINGS[] = {
  // page (menu page title), name, key, value, min, max, step, def, toggle, decimals, unit, help
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
  { "Channel Selection", "Busy level",      "busyDb",    &cfg.busyDb,        1,  15,  1,  6, 0, 0, " dB",  "Signal above floor that counts as busy" },
  { "Channel Selection", "Min history",     "quietMin",  &cfg.quietMinS,     0, 600, 10, 60, 0, 0, " s",   "Noise history needed before Quietest opens" },
  { "Channel Selection", "L Band Limit",    "lLimit",    &cfg.lBandLimit,    1,   8,  1,  8, 0, 0, "",     "Max L-band channels in Quietest and IMD lists" },
  { "Channel Selection", "IMD min spacing",     "imdSep",    &cfg.imdMinSep,    20,  80,  1, 37, 0, 0, " MHz", "Smallest gap allowed between channels in a set" },
  { "Channel Selection", "IMD use L band",      "imdLow",    &cfg.imdLowBand,    0,   1,  1,  0, 1, 0, "",     "Allow L1-L8 (HDZero mode: HDZero settings)" },
  { "Channel Selection", "IMD refresh",   "imdRefresh", &cfg.imdRefreshS,     2,    60,    1,   10, 0, 0, " s",  "How often IMD-free sets are re-picked" },
  { "HDZero", "HDZero features", "hdzero",    &cfg.hdzero,        0,   1,  1,  0, 1, 0, "",     "HDZero lists and HDZero-only grids" },
  { "HDZero", "Low band L1-L8",  "hdzLow",    &cfg.hdzLowBand,    0,   1,  1,  1, 1, 0, "",     "Include L1-L8 (needs low band unlocked)" },
  { "HDZero", "E1 and F1",       "hdzE1F1",   &cfg.hdzE1F1,       0,   1,  1,  1, 1, 0, "",     "Include E1 and F1 (not on every product page)" },
  { "Display", "Span",            "span",      &cfg.span,          5,  60,  5, 15, 0, 0, " dB",  "Activity full brightness, Spectrum height" },
  { "Display", "Auto range",      "specAuto",  &cfg.specAutoRange, 0,  1,  1,  0, 1, 0, "",     "Spectrum height grows to fit peaks" },
  { "Display", "Shading gamma",   "gamma",     &cfg.gamma100,     20, 100,  5, 55, 0, 2, "",     "Noise grid: below 1 brightens small diffs" },
  { "Display", "Full red spread", "redSpread", &cfg.redSpread10,   1,  50,  1,  5, 0, 1, " dB",  "Noise grid: spread shown as full red" },
  { "Display", "Peak decay",      "peakDecay", &cfg.peakDecay10,   2, 100,  2, 18, 0, 1, " px",  "Spectrum: peak-hold dot fall per sweep" },
  { "Display", "Waterfall log",   "wfLog",     &cfg.wfLogK,        0,  20,  1,  8, 0, 0, "",     "Spectrum waterfall: 0 = linear, higher = log" },
  { "Display", "Move fade",     "moveFade",   &cfg.moveFadeMs,      0, 10000,  250, 3000, 0, 0, " ms", "Quietest/IMD: red fade on a moved channel" },
  { "Display", "Backlight",       "backlight", &cfg.backlight,    10, 100, 10,100, 0, 0, "%",    "LCD brightness" },
  { "Controls", "Long press",    "longPress",  &cfg.longPressMs,   300, 2000,  100,  700, 0, 0, " ms", "ENTER held this long = long press" },
  { "Controls", "Config hold",   "cfgHold",    &cfg.configHoldMs, 2000, 10000, 1000, 5000, 0, 0, " ms", "Left + Right held this long opens Configuration" },
  { "Controls", "Splash time",   "splashMs",   &cfg.splashMs,        0, 10000,  500, 5000, 0, 0, " ms", "How long the start-up splash shows (0 = none)" },
  { "Controls", "Splash combo",  "splashCmb",  &cfg.splashComboMs,  20,   500,   10,   80, 0, 0, " ms", "Left + Right held this long on splash = Config" },
  { "Controls", "Pop-up time",   "popupMs",    &cfg.popupMs,       500,  5000,  250, 1500, 0, 0, " ms", "How long the Wait pop-up stays up" },
  { "Controls", "Scan dot step", "scanDot",    &cfg.scanDotMs,     100,  1000,   50,  400, 0, 0, " ms", "Speed of the Scanning dots" },
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

// Value as shown in the menu: "On"/"Off", or the number with its unit.
void formatSetting(int i, char *buf, int len) {
  const SettingDef &d = SETTINGS[i];
  int v = *d.value;
  if (measTimeoutLocked(i)) { snprintf(buf, len, "%d ms (11p)", MEAS_TIMEOUT_11P_MS); return; }
  if (d.value == &cfg.maxMhz && v >= MAX_MHZ_OFF) { snprintf(buf, len, "Off"); return; }
  if (d.value == &cfg.rxFilterMhz && v >= RXF_AUTO) { snprintf(buf, len, "Auto"); return; }
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

// Grid pages: in HDZero mode only channels an HDZero VTX can use get a box.
bool gridShown(int b, int c) {
  return !cfg.hdzero || hdzeroOk(b, c);
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

// Spectrum band buttons: in HDZero mode only bands with at least one HDZero
// channel (following the HDZero settings) can be selected.
bool bandEnabled(int b) {
  if (!cfg.hdzero) return true;
  for (int c = 0; c < NUM_CH; c++) if (hdzeroOk(b, c)) return true;
  return false;
}

// "HDZero: R1-8, L1-8, E1, F1, F2, F4", following the HDZero settings.
void hdzeroChannelsText(char *buf, int len) {
  snprintf(buf, len, "HDZero: R1-8%s%s, F2, F4",
           cfg.hdzLowBand ? ", L1-8" : "", cfg.hdzE1F1 ? ", E1, F1" : "");
}
