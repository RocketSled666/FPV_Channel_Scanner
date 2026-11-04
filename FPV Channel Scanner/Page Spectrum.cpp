/*
 * FPV Channel Scanner - Page Spectrum.cpp
 * Revision: 87
 *
 * Spectrum page: continuous sweep across one band in SPEC_STEP_MHZ
 * steps, with channel markers, values and band buttons. The sweep point
 * nearest each channel is moved onto the channel's exact frequency, so every
 * channel is measured directly; points are drawn by their real frequency.
 * Under the graph, in a white box, a WF_H-pixel waterfall shows the last WF_H sweeps, one
 * pixel row each, newest at the top: weak signal is dark blue, strong signal
 * bright yellow (through purple and orange), on a log curve ("Waterfall
 * log" setting) so weak signals show up more.
 */

#include "Scanner.h"

// ---------------------------------------------------------------- waterfall
static uint8_t wfLevel[WF_H][GW];   // 0..WF_LEVELS-1 per column, ring buffer of sweeps
static int     wfHead = 0;          // row index of the newest sweep
static int     wfRows = 0;          // sweeps stored so far (0..WF_H)
#define WF_LEVELS 64
#define WF_BLOCKED 255                  // column above "Max MHz": drawn grey
static uint8_t wfLut[WF_LEVELS][3];

// Colour ramp: dark blue -> purple -> orange -> bright yellow.
static void wfBuildLut() {
  static const float stop[4][4] = {      // position, R, G, B
    { 0.00f,   0,   0,  35 },     // darker blue at the floor
    { 0.35f, 110,   0, 170 },
    { 0.70f, 240,  90,  30 },
    { 1.00f, 255, 255,   0 },
  };
  for (int i = 0; i < WF_LEVELS; i++) {
    float t = i / (float)(WF_LEVELS - 1);
    int k = 0;
    while (k < 2 && t > stop[k + 1][0]) k++;
    float u = (t - stop[k][0]) / (stop[k + 1][0] - stop[k][0]);
    for (int c = 0; c < 3; c++)
      wfLut[i][c] = (uint8_t)lroundf(stop[k][c + 1] + (stop[k + 1][c + 1] - stop[k][c + 1]) * u);
  }
}

// Forget the history (new band, or the page was redrawn) and draw it empty.
static void wfReset() {
  static bool lutReady = false;
  if (!lutReady) { wfBuildLut(); lutReady = true; }
  wfHead = 0; wfRows = 0;
  lcdFill(GX, WF_Y, GW, WF_H, 0, 0, 0);
}

// Redraw the waterfall: newest sweep on the top row, older ones below it;
// rows with no sweep yet stay black.
static void wfDraw() {
  static uint8_t row[GW * 3];
  for (int r = 0; r < wfRows; r++) {
    const uint8_t *lv = wfLevel[(wfHead - r + WF_H) % WF_H];
    for (int x = 0; x < GW; x++) {
      static const uint8_t grey[3] = { C_BLOCKED.r, C_BLOCKED.g, C_BLOCKED.b };
      const uint8_t *c = lv[x] == WF_BLOCKED ? grey : wfLut[lv[x]];
      row[x * 3] = c[0]; row[x * 3 + 1] = c[1]; row[x * 3 + 2] = c[2];
    }
    lcdDrawRgbRow(GX, WF_Y + r, GW, row);
  }
}

// Auto range: the graph's top (dB above floor). At least the Span setting;
// grows at once to fit the highest point (rounded up to SPEC_RANGE_STEP), and
// after SPEC_RANGE_HOLD_MS without needing that much, shrinks one step at a
// time. The waterfall keeps the fixed Span so its colours stay comparable.
static int      specRange = 0;
static uint32_t specRangeNeedMs = 0;

static void specDrawAxis() {
  char s[8];
  lcdFill(0, GY, GX - 2, GH, 0, 0, 0);
  snprintf(s, sizeof(s), "%d", specRange);            lcdText(2, GY, s, 1, C_DIM, C_BG);
  snprintf(s, sizeof(s), "%d", (specRange + 1) / 2);  lcdText(2, GY + GH / 2 - 4, s, 1, C_DIM, C_BG);
  lcdText(2, GY + GH - 8, "0", 1, C_DIM, C_BG);
  lcdText(2, GY + GH - 18, "dB", 1, C_DIM, C_BG);
}

// Pick the range for this sweep from its highest point. Returns true if it
// changed (the axis is redrawn and peak-hold dots rescaled; columns redraw
// themselves because their heights change).
static bool specUpdateRange(float maxAbove) {
  int need = cfg.span;
  if (maxAbove > need) need = min(SPEC_RANGE_MAX, ((int)ceilf(maxAbove / SPEC_RANGE_STEP)) * SPEC_RANGE_STEP);
  uint32_t now = millis();
  int old = specRange;
  if (need >= specRange) { specRange = need; specRangeNeedMs = now; }
  else if (now - specRangeNeedMs >= SPEC_RANGE_HOLD_MS) {
    specRange = max(need, specRange - SPEC_RANGE_STEP); specRangeNeedMs = now;
  }
  if (specRange == old) return false;
  for (int x = 0; x < GW; x++) colPeak[x] = colPeak[x] * old / specRange;
  specDrawAxis();
  return true;
}

int freqToCol(float mhz) {
  return (int)lroundf((mhz - specLo) * (GW - 1) / (float)(specHi - specLo));
}

void specSetBand(int band) {
  if (!bandEnabled(band)) band = nextBandInButtonOrder(band);   // HDZero mode: never a disabled band
  specBand = band;
  uint16_t lo = 65535, hi = 0;
  for (int c = 0; c < NUM_CH; c++) { lo = min(lo, FPV_MHZ[band][c]); hi = max(hi, FPV_MHZ[band][c]); }
  lo -= SPEC_MARGIN_MHZ; hi += SPEC_MARGIN_MHZ;
  lo -= lo % SPEC_STEP_MHZ;
  specPts = (hi - lo) / SPEC_STEP_MHZ + 1;
  if (specPts > SPEC_MAX_PTS) specPts = SPEC_MAX_PTS;
  specLo = lo;
  specHi = lo + (specPts - 1) * SPEC_STEP_MHZ;
  // Even grid, then snap the nearest point onto each channel (moves it at
  // most SPEC_STEP_MHZ / 2; channels are >= 19 MHz apart, so no two channels
  // share a point and the points stay in order).
  for (int i = 0; i < specPts; i++) specMhz[i] = lo + i * SPEC_STEP_MHZ;
  for (int c = 0; c < NUM_CH; c++) {
    int i = (int)lroundf((FPV_MHZ[band][c] - lo) / (float)SPEC_STEP_MHZ);
    i = constrain(i, 0, specPts - 1);
    specMhz[i] = FPV_MHZ[band][c];
    specChPt[c] = i;
  }
  for (int i = 0; i < specPts; i++) specSmooth[i] = NAN;
  if (!specFloorValid[band]) {
    bool seed = gridFloorReady() && !specNoSeedOnce;
    for (int i = 0; i < SPEC_MAX_PTS; i++)
      specFloor[band][i] = (seed && i < specPts) ? gridFloorAt(specMhz[i]) : 0;
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

// Band buttons: square-cornered, in a row under the title, spread so the
// row lines up exactly with the left and right edges of the graph box.
// Selected: solid green with a black letter; others greyed out.
void specDrawButtons() {
  int x0 = GX - 1;                                             // graph box left edge
  for (int i = 0; i < NUM_BANDS; i++) {
    int b = btnOrder[i];
    int x = x0 + i * (BTN_W + BTN_GAP);
    bool sel = b == specBand;
    // selected: solid green, black letter; others greyed out; disabled
    // (HDZero mode, no HDZero channel in the band) greyed out more darkly
    bool off = !bandEnabled(b);
    RGB edge = sel ? C_BTN_SEL : off ? RGB{ 35, 35, 35 } : RGB{ 70, 70, 70 };
    RGB fill = sel ? C_BTN_SEL : off ? RGB{ 10, 10, 10 } : RGB{ 20, 20, 20 };
    RGB text = sel ? RGB{ 0, 0, 0 } : off ? RGB{ 50, 50, 50 } : RGB{ 130, 130, 130 };
    lcdFillC(x, BTN_Y, BTN_W, BTN_H, edge);                     // 1 px outline
    lcdFillC(x + 1, BTN_Y + 1, BTN_W - 2, BTN_H - 2, fill);
    char s[2] = { BAND_NAME[b], 0 };
    lcdText(x + (BTN_W - 12) / 2 + 1, BTN_Y + 2, s, 2, text, fill);
  }
}

void specDrawStatic() {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  char t[32];
  snprintf(t, sizeof(t), "Spectrum: %s", BAND_LONG[specBand]);
  lcdText(4, 4, t, 1, C_ACC, C_BG);
  snprintf(t, sizeof(t), "%u-%u MHz", specLo, specHi);
  lcdText(4, 14, t, 1, C_DIM, C_BG);

  // y axis labels (dB above floor, auto range)
  if (specRange < cfg.span) { specRange = cfg.span; specRangeNeedMs = millis(); }
  specDrawAxis();

  // graph frame
  lcdFillC(GX - 1, GY - 1, GW + 2, 1, C_DIM);
  lcdFillC(GX - 1, GY + GH, GW + 2, 1, C_DIM);
  lcdFillC(GX - 1, GY - 1, 1, GH + 2, C_DIM);
  lcdFillC(GX + GW, GY - 1, 1, GH + 2, C_DIM);

  // white box around the waterfall
  lcdFillC(GX - 1, WF_Y - 1,    GW + 2, 1, C_PEAKH);
  lcdFillC(GX - 1, WF_Y + WF_H, GW + 2, 1, C_PEAKH);
  lcdFillC(GX - 1, WF_Y - 1,    1, WF_H + 2, C_PEAKH);
  lcdFillC(GX + GW, WF_Y - 1,   1, WF_H + 2, C_PEAKH);

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
  wfReset();
  specFooterDirty = true;
  learnShownPct = -1; scanAnimY = -1;
}

// Column above "Max MHz": plain grey, nothing measured.
static void specDrawBlockedColumn(int x) {
  if (colShownH[x] == -3) return;
  lcdFillC(GX + x, GY, 1, GH, C_BLOCKED);
  colShownH[x] = -3; colShownPk[x] = -3;
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
// Read straight from the sweep point that sits on the channel's frequency.
void specDrawChannelValues(const float *flo) {
  int val[NUM_CH]; int best = -1, bestV = 2;
  for (int c = 0; c < NUM_CH; c++) {
    if (mhzBlocked(FPV_MHZ[specBand][c])) { val[c] = -3; continue; }   // above "Max MHz": "--"
    int i = specChPt[c];
    float a = (isnan(specSmooth[i]) || flo[i] == 0) ? 0 : specSmooth[i] - flo[i];
    int v = (int)lroundf(a);
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
    } else if (val[c] == -3) {
      lcdText(constrain(x - 6, 0, LCD_W - 12), CH_VAL_Y, "--", 1, RGB{ 90, 90, 90 }, C_BG);
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
    if (mhzBlocked(specMhz[i])) { raw[i] = NAN; fb[i] = true; }   // above "Max MHz": not measured
    else {
      raw[i] = readFreqDb(specMhz[i], SPEC_SAMPLES, SETTLE_US);
      fb[i] = g_lastReadFallback;
    }
    if (evToggle || evEnterShort || evEnterLong || evConfig) return;  // react to input immediately
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

  // draw: each pixel column interpolates, by frequency, between the two
  // sweep points either side of it (points aren't evenly spaced any more)
  uint32_t tDraw0 = micros();
  float bestAbove = -1e9f, bestMhz = specLo;
  int seg = 0;
  wfHead = (wfHead + 1) % WF_H;             // this sweep's waterfall row
  if (wfRows < WF_H) wfRows++;
  uint8_t *wfNew = wfLevel[wfHead];
  { float mx = 0;                            // highest point decides the range
    for (int i = 0; i < specPts; i++)
      if (!isnan(specSmooth[i]) && flo[i] != 0) mx = max(mx, specSmooth[i] - flo[i]);
    specUpdateRange(mx); }
  for (int x = 0; x < GW; x++) {
    float f = specLo + x * (float)(specHi - specLo) / (GW - 1);
    if (mhzBlocked(f)) {                    // above "Max MHz": grey, never the peak
      colPeak[x] = 0; wfLevel[wfHead][x] = WF_BLOCKED;
      specDrawBlockedColumn(x);
      continue;
    }
    while (seg < specPts - 2 && specMhz[seg + 1] < f) seg++;
    int i0 = seg, i1 = min(seg + 1, specPts - 1);
    float span = (float)(specMhz[i1] - specMhz[i0]);
    float t = span > 0 ? constrain((f - specMhz[i0]) / span, 0.0f, 1.0f) : 0;
    float a0 = (isnan(specSmooth[i0]) || flo[i0] == 0) ? 0 : specSmooth[i0] - flo[i0];
    float a1 = (isnan(specSmooth[i1]) || flo[i1] == 0) ? 0 : specSmooth[i1] - flo[i1];
    float above = a0 + (a1 - a0) * t;     // trace is drawn even while learning
    if (above > bestAbove) { bestAbove = above; bestMhz = f; }
    int h = constrain((int)lroundf(above * GH / specRange), 0, GH);
    // Waterfall colour: 0..1 of the span, through a log curve set by
    // "Waterfall log" (0 = linear; higher lifts weak signals).
    float wx = constrain(above / cfg.span, 0.0f, 1.0f);
    if (cfg.wfLogK > 0) wx = logf(1.0f + cfg.wfLogK * wx) / logf(1.0f + cfg.wfLogK);
    wfNew[x] = (uint8_t)constrain((int)lroundf(wx * (WF_LEVELS - 1)), 0, WF_LEVELS - 1);
    colPeak[x] = max(colPeak[x] - PEAK_DECAY_PX, (float)h); // peak hold, falling
    specDrawColumn(x, h, (int)colPeak[x]);
    if ((x & 31) == 0) pollInput();
  }

  // The trace is straight lines between measured points, so its peak is at a
  // point: report that point's exact frequency rather than the pixel column's.
  { int bi = 0; float bd = 1e9f;
    for (int i = 0; i < specPts; i++) { float d = fabsf(specMhz[i] - bestMhz); if (d < bd) { bd = d; bi = i; } }
    bestMhz = specMhz[bi]; }

  wfDraw();
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
      printRssiStats();
      printStalls();
    } else {
      resetTiming();
    }
  }
}
