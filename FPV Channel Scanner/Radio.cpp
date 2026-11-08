/*
 * FPV Channel Scanner - Radio.cpp
 * Revision: 97
 *
 * 5 GHz measurement: tuning (libphy phy_set_chanfreq(), or the Wi-Fi
 * path phy_change_channel(), "Tuning" setting) plus the analog RX filter
 * width ("RX filter" setting), reading
 * the baseband noise-floor estimator (or signal RSSI with "RSSI only"),
 * holding the last good value when a reading stalls,
 * radio start-up, the boot timing test and the serial timing report.
 */

#include "Scanner.h"

// One power measurement at the currently tuned frequency.
// Returns quarter-dB units (e.g. -360 == -90.0 dB), or INT16_MIN on timeout.
// A timeout seems to happen when a signal keeps the receiver busy.
int16_t measureQdb() {
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

void noteStall(uint16_t mhz, float fallbackDb) {
  for (int i = 0; i < g_nStalls; i++)
    if (g_stalls[i].mhz == mhz) { g_stalls[i].count++; g_stalls[i].lastFallbackDb = fallbackDb; return; }
  if (g_nStalls < MAX_STALL_STATS) {
    g_stalls[g_nStalls].mhz = mhz; g_stalls[g_nStalls].count = 1;
    g_stalls[g_nStalls].lastFallbackDb = fallbackDb; g_nStalls++;
  }
}

void printStalls() {
  if (g_nStalls == 0) return;
  Serial.print("  stalls (MHz x count, last value dB):");
  for (int i = 0; i < g_nStalls; i++)
    Serial.printf(" %u x%lu (%.1f)", g_stalls[i].mhz, (unsigned long)g_stalls[i].count, g_stalls[i].lastFallbackDb);
  Serial.println();
}

// Last good reading per MHz (quarter dB), INT16_MIN = none yet.
static int16_t heldQdb[HELD_MHZ_N];
static bool heldInit = false;

// "RSSI only" setting: measure with libphy's signal RSSI (dBm) instead of the
// noise-floor estimator. Switching signal-RSSI mode on stops the estimator
// on the C5 (every estimator read times out), so it is one or the other.
// Each sample is the median of SIG_READS reads SIG_READ_GAP_US apart, which
// drops a single Wi-Fi spike or VTX dip. Resolution is 1 dB per read (the
// estimator gives 0.25 dB), so averaging samples matters more here.
static bool     sigOn = false;            // signal-RSSI mode switched on at start-up
static uint32_t sigReads = 0, sigBad = 0, sigSame = 0, sigSamples = 0;
static int      sigMin = 0, sigMax = 0, sigPrev = INT16_MIN;

static bool sigOk(int v) { return v >= SIG_OK_MIN && v <= SIG_OK_MAX; }

// Median of the sane reads, or INT16_MIN if none is sane.
static int sigReadMedian() {
  int v[SIG_READS], n = 0;
  for (int k = 0; k < SIG_READS; k++) {
    int r = phy_get_sigrssi();
    sigReads++;
    if (sigOk(r)) {                                   // insertion sort as we go
      int j = n++;
      while (j > 0 && v[j - 1] > r) { v[j] = v[j - 1]; j--; }
      v[j] = r;
    } else sigBad++;
    if (k + 1 < SIG_READS) delayMicroseconds(SIG_READ_GAP_US);
  }
  return n ? v[n / 2] : INT16_MIN;
}

// One "RSSI only" sample in quarter dB (same units as measureQdb), or
// INT16_MIN if no read was sane. Also keeps the statistics for serial.
static int16_t sigMeasureQdb() {
  int v = sigReadMedian();
  if (v == INT16_MIN) { g_nfTimeouts++; return INT16_MIN; }
  if (sigSamples == 0 || v < sigMin) sigMin = v;
  if (sigSamples == 0 || v > sigMax) sigMax = v;
  if (v == sigPrev) sigSame++;                      // same as the previous sample (frozen?)
  sigPrev = v; sigSamples++;
  return (int16_t)(v * 4);
}

// Serial line per report: how many reads were out of range and how often a
// sample repeated the previous one (near 100% would mean it isn't updating).
void printRssiStats() {
  if (!sigOn) return;
  Serial.printf("  rssi only: %lu samples, %lu/%lu reads out of range, range %d..%d dBm, %lu%% repeats\n",
                (unsigned long)sigSamples, (unsigned long)sigBad, (unsigned long)sigReads, sigMin, sigMax,
                (unsigned long)(sigSamples ? sigSame * 100 / sigSamples : 0));
  sigReads = sigBad = sigSame = sigSamples = 0;
}

// ---------------------------------------------------------------- tuning
static int  tuneMode = TUNE_DIRECT;       // as actually used (falls back if libphy lacks a call)
static int  rxfCode  = -1;                // capacitor code to force after each retune, -1 = Auto
static unsigned rxfCal[2];                // calibrated register values, for the log

// RX filter width (MHz) -> capacitor code, PHY channel mode 0 (ESP-SDR /
// ESPsoup measurements, approximate full width). Linear in between.
static int rxFilterCode(int mhz) {
  static const uint8_t code[] = {  0,  4,  8, 12, 16, 24, 32, 40, 48, 60 };
  static const uint8_t wid[]  = { 23, 22, 21, 20, 18, 16, 15, 13, 12, 11 };
  const int n = sizeof(code);
  if (mhz >= wid[0]) return code[0];
  if (mhz <= wid[n - 1]) return code[n - 1];
  for (int i = 1; i < n; i++)
    if (mhz >= wid[i]) {
      int span = wid[i - 1] - wid[i];
      return code[i - 1] + ((code[i] - code[i - 1]) * (wid[i - 1] - mhz) + span / 2) / span;
    }
  return code[n - 1];
}

// Retune; then force the RX filter code (a retune reloads the calibrated
// value). Only the low 6 bits of each register are changed.
static void tuneTo(uint16_t mhz) {
  if (tuneMode == TUNE_DIRECT) {
    phy_set_chanfreq(mhz, 0);                 // 20 MHz bandwidth
  } else {
    if (tuneMode == TUNE_WIFI_CH) esp_wifi_set_channel(WIFI_TUNE_CH, WIFI_SECOND_CHAN_NONE);
    phy_change_channel(mhz, 1, 0, 0);
  }
  if (rxfCode >= 0)
    for (unsigned j = 0; j < 2; j++) {
      unsigned v = phy_chip_i2c_readReg(RXF_BLOCK, RXF_HOST, RXF_REG + j);
      phy_i2c_writeReg(RXF_BLOCK, RXF_HOST, RXF_REG + j, (v & ~63u) | (unsigned)rxfCode);
    }
}

// One measurement with whichever method is on.
static int16_t measureAny() { return sigOn ? sigMeasureQdb() : measureQdb(); }

// Tune to a frequency and return the average power in dB (relative; dBm
// with "RSSI only"). If every measurement fails, g_lastReadFallback and
// g_lastReadHeld are set (callers keep it out of floors and noise history)
// and the last good reading at that frequency is returned (NAN if none yet).
float readFreqDb(uint16_t mhz, int samples, uint32_t settleUs) {
  uint32_t t0 = micros();
  tuneTo(mhz);
  uint32_t t1 = micros();
  delayMicroseconds(settleUs);        // let the synthesizer / AGC settle
  uint32_t t2 = micros();
  int32_t sum = 0; int n = 0;
  for (int i = 0; i < samples; i++) {
    int16_t q = measureAny();
    if (q != INT16_MIN) { sum += q; n++; }
  }
  uint32_t t3 = micros();
  g_tTuneUs += t1 - t0; g_nTunes++;
  g_tMeasUs += t3 - t2; g_nMeas += samples;
  pollInput();
  if (!heldInit) { for (int i = 0; i < HELD_MHZ_N; i++) heldQdb[i] = INT16_MIN; heldInit = true; }
  int hi = (int)mhz - HELD_MHZ_LO;
  bool inRange = hi >= 0 && hi < HELD_MHZ_N;
  g_lastReadFallback = false; g_lastReadHeld = false;
  if (n == 0) {                                   // stalled: hold the last good value
    float held = (inRange && heldQdb[hi] != INT16_MIN) ? heldQdb[hi] / 4.0f : NAN;
    noteStall(mhz, held);
    g_lastReadFallback = true; g_lastReadHeld = true;
    return held;
  }
  if (inRange) heldQdb[hi] = (int16_t)(sum / n);
  return (float)sum / n / 4.0f;
}

// Print where the time went since the last report, then reset the counters.
void printTiming(uint32_t sweeps) {
  Serial.printf("  timing: retune %lu us avg, measurement %lu us avg (%lu/sweep), draw %lu ms/sweep\n",
                (unsigned long)(g_nTunes ? g_tTuneUs / g_nTunes : 0),
                (unsigned long)(g_nMeas ? g_tMeasUs / g_nMeas : 0),
                (unsigned long)(sweeps ? g_nMeas / sweeps : 0),
                (unsigned long)(sweeps ? g_tDrawUs / sweeps / 1000 : 0));
  g_tTuneUs = g_tMeasUs = g_tDrawUs = 0; g_nTunes = g_nMeas = 0;
}

void resetTiming() {
  g_tTuneUs = g_tMeasUs = g_tDrawUs = 0; g_nTunes = g_nMeas = 0;
}

// True when the USB serial buffer can take a dump of this size right now.
// If the PC isn't reading, the buffer stays full and the dump is skipped.
bool serialHasRoom(int bytes) {
  return Serial.availableForWrite() >= bytes;
}

void radioInit() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_err_t e = esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
  if (e != ESP_OK) Serial.printf("set_band_mode: %s\n", esp_err_to_name(e));
  esp_wifi_set_promiscuous(true);                        // keep the receiver running
  e = esp_wifi_set_channel(cfg.tuneMode == TUNE_DIRECT ? 149 : WIFI_TUNE_CH,
                           WIFI_SECOND_CHAN_NONE);       // brings up 5 GHz cal
  if (e != ESP_OK) Serial.printf("set_channel: %s\n", esp_err_to_name(e));
  delay(50);
  // Optional libphy modes (settings). Weak symbols: missing = not available.
  if (cfg.mode11p) {
    if (phy_11p_set) { phy_11p_set(1, 0); Serial.println("802.11p mode: on"); }
    else Serial.println("802.11p mode: not in this core's libphy, ignored");
  }
  // Tuning path and RX filter (settings). Reported so a log shows what ran.
  tuneMode = cfg.tuneMode;
  if (tuneMode != TUNE_DIRECT && !phy_change_channel) {
    tuneMode = TUNE_DIRECT;
    Serial.println("Tuning: phy_change_channel not in this core's libphy, using Direct");
  } else Serial.printf("Tuning: %s\n", tuneMode == TUNE_WIFI ? "Wi-Fi (phy_change_channel, driver on ch 140)"
                                     : tuneMode == TUNE_WIFI_CH ? "Wi-Fi+chan (set ch 140 before every retune)"
                                     : "Direct (phy_set_chanfreq)");
  if (phy_chip_i2c_readReg && phy_i2c_writeReg) {
    tuneTo(5800);                                       // read the calibrated values
    for (unsigned j = 0; j < 2; j++) rxfCal[j] = phy_chip_i2c_readReg(RXF_BLOCK, RXF_HOST, RXF_REG + j);
    if (cfg.rxFilterMhz < RXF_AUTO) rxfCode = rxFilterCode(cfg.rxFilterMhz);
    if (rxfCode >= 0) Serial.printf("RX filter: %d MHz (code %d; calibrated 0x%02x/0x%02x)\n",
                                    cfg.rxFilterMhz, rxfCode, rxfCal[0], rxfCal[1]);
    else Serial.printf("RX filter: Auto (calibrated 0x%02x/0x%02x)\n", rxfCal[0], rxfCal[1]);
  } else if (cfg.rxFilterMhz < RXF_AUTO) Serial.println("RX filter: libphy I2C calls missing, left on Auto");
  if (cfg.rssiOnly) {
    if (phy_check_sigrssi_en && phy_get_sigrssi) {
      phy_check_sigrssi_en(1); sigOn = true;
      Serial.println("RSSI only: on (signal RSSI in dBm, noise-floor estimator not used)");
    } else Serial.println("RSSI only: not in this core's libphy, using the noise-floor estimator");
  }
}

// Times the retune and a single measurement separately, so the serial log
// shows which one limits the sweep rate.
void bootTimingTest() {
  static const uint16_t testMhz[] = { 5362, 5658, 5800, 5802, 5945 };
  Serial.println("Timing test (us):  freq  retune  measure min/avg/max");
  for (size_t k = 0; k < sizeof(testMhz) / sizeof(testMhz[0]); k++) {
    uint32_t t0 = micros();
    tuneTo(testMhz[k]);
    uint32_t tune = micros() - t0;
    measureAny();                                   // discard after retune
    uint32_t mn = 0xFFFFFFFF, mx = 0, sum = 0;
    for (int i = 0; i < 10; i++) {
      uint32_t a = micros();
      measureAny();
      uint32_t d = micros() - a;
      sum += d; if (d < mn) mn = d; if (d > mx) mx = d;
    }
    Serial.printf("                   %u  %6lu  %lu/%lu/%lu\n", testMhz[k], (unsigned long)tune,
                  (unsigned long)mn, (unsigned long)(sum / 10), (unsigned long)mx);
  }
  uint32_t t0 = micros();
  tuneTo(5945);                                     // retune to the same frequency
  Serial.printf("  same-frequency retune: %lu us, timeouts=%lu\n",
                (unsigned long)(micros() - t0), (unsigned long)g_nfTimeouts);
}
