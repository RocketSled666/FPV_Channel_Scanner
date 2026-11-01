/*
 * FPV Channel Scanner - Page Spectrum.cpp
 * Revision: 34
 *
 * Spectrum page: continuous sweep across one band in SPEC_STEP_MHZ
 * steps, with channel markers, values and band buttons.
 */

#include "Scanner.h"

int freqToCol(float mhz) {
  return (int)lroundf((mhz - specLo) * (GW - 1) / (float)(specHi - specLo));
}

void specSetBand(int band) {
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

void specDrawButtons() {
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

void specDrawStatic() {
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

void specDrawColumn(int x, int h, int pk) {
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
void specDrawChannelValues(const float *flo) {
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

void specDrawFooter(float peakMhz, float aboveDb, bool learning) {
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
void specSweep() {
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
