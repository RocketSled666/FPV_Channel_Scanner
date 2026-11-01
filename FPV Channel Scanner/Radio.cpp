/*
 * FPV Channel Scanner - Radio.cpp
 * Revision: 46
 *
 * 5 GHz measurement: tuning with the internal phy_set_chanfreq(), reading
 * the baseband noise-floor estimator, the RSSI fallback when it stalls,
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
  Serial.print("  stalls (MHz x count, RSSI fallback dB):");
  for (int i = 0; i < g_nStalls; i++)
    Serial.printf(" %u x%lu (%.1f)", g_stalls[i].mhz, (unsigned long)g_stalls[i].count, g_stalls[i].lastFallbackDb);
  Serial.println();
}

// Tune to a frequency and return the average power in dB (relative).
// If every measurement stalls, the receiver RSSI register is used instead and
// g_lastReadFallback is set, so callers can keep it out of floor learning.
float readFreqDb(uint16_t mhz, int samples, uint32_t settleUs) {
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
  e = esp_wifi_set_channel(149, WIFI_SECOND_CHAN_NONE);  // brings up 5 GHz cal
  if (e != ESP_OK) Serial.printf("set_channel: %s\n", esp_err_to_name(e));
  delay(50);
}

// Times the retune and a single measurement separately, so the serial log
// shows which one limits the sweep rate.
void bootTimingTest() {
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
