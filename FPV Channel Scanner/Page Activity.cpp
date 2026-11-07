/*
 * FPV Channel Scanner - Page Activity.cpp
 * Revision: 95
 *
 * FPV Channel Activity page (the live grid) and the grid sweep that
 * also feeds the Channel Noise and Quietest Channels pages.
 */

#include "Scanner.h"

void gridResetFloor() {
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) { gridFloor[b][c] = 0; gridSmooth[b][c] = NAN; }
  // Spectrum floors came from the old grid floor; re-seed them next time.
  for (int b = 0; b < NUM_BANDS; b++) specFloorValid[b] = false;
  bscanForgetFloor();                      // and the band scan floor
  actClear();                              // activity is relative to the floor
  gridLearnUntil = gridSweeps + GRID_LEARN_SWEEPS;
}

void gridInvalidate() {
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) { gridShownLevel[b][c] = -1; gridShownPeak[b][c] = false; }
}

void gridDrawStatic() {
  drawGridFrame("FPV Channel Activity", C_GOOD);   // green title
  gridInvalidate();
  gridFooterDirty = true;
}

void gridDrawCell(int b, int c, int level, bool peak) {
  if (!gridShown(b, c)) return;                     // HDZero mode: blank where HDZero can't tune
  int x = GRID_X + c * (CELL_W + CELL_GAP);
  int y = GRID_Y + bandRow[b] * (CELL_H + CELL_GAP);
  if (level == -2) { drawNoDataBox(x, y, b, c); return; }   // channel can't be measured
  RGB col = scaleColor(C_HOT, 0.06f + 0.94f * level / 32.0f);
  drawGridBox(x, y, col, peak);
  char f[6];
  snprintf(f, sizeof(f), "%u", FPV_MHZ[b][c]);
  RGB tc = level > 16 ? RGB{ 0, 0, 0 } : C_DIM;
  lcdText(x + (CELL_W - 24) / 2, y + CELL_H - 12, f, 1, tc, col);
}

void gridDrawFooter(int pb, int pc, float aboveDb) {
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

// One full grid sweep + redraw of changed cells.
// Runs on the GRID and ACTIVITY pages; both share the measurement and floor.
void gridSweep() {
  static int lastPeakB = -2, lastPeakC = -2, lastAboveRounded = -999, lastSpan = -1;

  static bool lastLearning = false;
  const uint32_t learnStart = gridLearnUntil - GRID_LEARN_SWEEPS;
  const bool learningSweep = gridSweeps < gridLearnUntil;   // this sweep counts toward learning
  const int  total = NUM_BANDS * NUM_CH;

  float raw[NUM_BANDS][NUM_CH];
  bool  fb[NUM_BANDS][NUM_CH], held[NUM_BANDS][NUM_CH];
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      if (mhzBlocked(FPV_MHZ[b][c])) {          // above "Max MHz": not measured, no data
        raw[b][c] = NAN; fb[b][c] = held[b][c] = true;
      } else {
        raw[b][c] = readFreqDb(FPV_MHZ[b][c], GRID_SAMPLES, SETTLE_US);
        fb[b][c] = g_lastReadFallback;
        held[b][c] = g_lastReadHeld;
      }
      if (evToggle || evEnterShort || evEnterLong || evConfig) return;  // react to input immediately
      if (learningSweep && c == NUM_CH - 1) {
        int done = (gridSweeps - learnStart) * total + b * NUM_CH + c + 1;
        drawLearnProgress(FOOT_Y, done * 100 / (GRID_LEARN_SWEEPS * total));
      }
    }
  gridSweeps++;

  bool learning = gridSweeps <= gridLearnUntil;
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float &hf = gridHeldFrac[b][c];
      hf += 0.1f * ((held[b][c] ? 1.0f : 0.0f) - hf);              // ~10 sweeps
      if (hf > NODATA_ON_FRAC) gridNoData[b][c] = true;            // hysteresis: no flicker
      else if (hf < NODATA_OFF_FRAC) gridNoData[b][c] = false;
      float v = raw[b][c];
      if (isnan(v)) continue;
      float &s = gridSmooth[b][c];
      if (isnan(s)) s = v;
      else s += (v > s ? 0.7f : 0.25f) * (v - s);
      if (held[b][c]) { raw[b][c] = NAN; continue; } // held: no floor, no noise history
      if (fb[b][c]) continue;                         // calibrated RSSI: noise history, but no floor
      float &f = gridFloor[b][c];
      if (f == 0 || v < f) f = v;            // track the minimum
      else if (!learning) f += 0.01f;        // slow upward drift
    }

  // Keep a channel's floor from being learned while a VTX sits on it:
  // clamp each floor to at most (median floor + 4 dB). Only learned floors
  // count and get clamped; 0 = not learned yet (held or above "Max MHz").
  float tmp[NUM_BANDS * NUM_CH]; int n = 0;
  for (int b = 0; b < NUM_BANDS; b++) for (int c = 0; c < NUM_CH; c++)
    if (gridFloor[b][c] != 0) tmp[n++] = gridFloor[b][c];
  for (int i = 1; i < n; i++) { float k = tmp[i]; int j = i - 1; while (j >= 0 && tmp[j] > k) { tmp[j+1] = tmp[j]; j--; } tmp[j+1] = k; }
  if (n > 0) {
    float medFloor = tmp[n / 2];
    for (int b = 0; b < NUM_BANDS; b++) for (int c = 0; c < NUM_CH; c++)
      if (gridFloor[b][c] != 0 && gridFloor[b][c] > medFloor + 4.0f) gridFloor[b][c] = medFloor + 4.0f;
  }

  if (!learning) actAccumulate(raw);

  uint32_t tDraw0 = micros();
  if (mode == MODE_ACTIVITY) {
    if (!popupUntilMs) actDraw(learning, learnStart);   // don't draw over the pop-up
  } else if (quietPage()) {
    if (!popupUntilMs) quietDraw(learning, learnStart);   // don't draw over the pop-up
  } else {
  int pb = -1, pc = -1; float best = -1e9f;
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      if (!gridShown(b, c) || chanNoData(b, c)) continue;   // peak only among live drawn boxes
      float above = gridSmooth[b][c] - gridFloor[b][c];
      if (isnan(above) || gridFloor[b][c] == 0) above = 0;
      if (above > best) { best = above; pb = b; pc = c; }
    }
  bool havePeak = best >= 3.0f && !learning;

  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float above = learning ? 0 : gridSmooth[b][c] - gridFloor[b][c];
      if (isnan(above) || gridFloor[b][c] == 0) above = 0;
      int level = constrain((int)lroundf(above * 32.0f / cfg.span), 0, 32);
      if (chanNoData(b, c)) level = -2;            // no live data: "--"
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
                    cfg.span != lastSpan || gridFooterDirty)) {
    gridDrawFooter(footB, footC, best);
    lastPeakB = footB; lastPeakC = footC; lastAboveRounded = aboveRounded; lastSpan = cfg.span; gridFooterDirty = false;
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
    if ((mode == MODE_ACTIVITY || quietPage()) && !learning) drawSpanLabel();   // history time
    if (serialHasRoom(1600)) {
      printTiming(sweepsThisPeriod);
      printRssiStats();
      printStalls();
      Serial.printf("grid sweep %lu  %.1f/s  timeouts=%lu\n", (unsigned long)gridSweeps,
                    sps, (unsigned long)g_nfTimeouts);
      bool noiseDump = (mode == MODE_ACTIVITY || quietPage());
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

// Noise floor at any frequency, linearly interpolated from the grid's 48
// learned channel floors (nearest value is held beyond the ends). Channels
// with no floor (0) are skipped; 0 if no channel has one.
float gridFloorAt(float mhz) {
  float lowF = 1e9f, lowV = 0, highF = -1e9f, highV = 0;     // overall ends
  float belowF = -1e9f, belowV = 0, aboveF = 1e9f, aboveV = 0; // neighbours
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float f = FPV_MHZ[b][c], v = gridFloor[b][c];
      if (v == 0) continue;
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

bool gridFloorReady() { return gridSweeps > gridLearnUntil; }
