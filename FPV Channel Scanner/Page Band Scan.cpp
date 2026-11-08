/*
 * FPV Channel Scanner - Page Band Scan.cpp
 * Revision: 97
 *
 * Band Scan page, modelled on the ArduVidRx band scanner: one sweep over
 * the whole 5.3-6.0 GHz band, measuring every FPV channel frequency plus
 * fill-in points between them (and 37 MHz past each end), drawn as a bar
 * graph in 10 MHz bins. The graph ends at "Max MHz" (6000 MHz when Off),
 * its bars widened to fill the width. Peaks are labelled with their
 * frequency, the strongest bar is yellow (others green, brighter when
 * stronger), and the footer names it ("Peak:
 * F1 5740 MHz 22 dB"). ENTER holds / releases the scan (a red "Hold" flashes
 * while held, as on the IMD-Free page).
 */

#include "Scanner.h"

// ---------------------------------------------------------------- sweep points
static uint16_t bsMhz[BS_MAX_PTS];       // sweep frequencies, ascending
static int      bsPts = 0;
static float    bsSmooth[BS_MAX_PTS];
static float    bsFloor[BS_MAX_PTS];     // 0 = not learned
static float    bsAbove[BS_MAX_PTS];     // smoothed dB above floor, last sweep
static bool     bsFloorValid = false;
static bool     bsNoSeedOnce = false;    // ENTER-hold re-learn: start from scratch
static uint32_t bsSweeps = 0, bsLearnUntil = 0;
static bool     bsPaused = false;

// ---------------------------------------------------------------- bins (bars)
static int16_t bsBinPt0[BS_BINS];        // first point in the bin, -1 = none
static int16_t bsBinPtN[BS_BINS];        // points in the bin
static int16_t bsBinNear[BS_BINS];       // empty bin inside the sweep: nearest point, -1 = blank
static int16_t bsShownH[BS_BINS];        // bar height drawn, -1 = redraw, -3 = grey (blocked)
static int8_t  bsShownKind[BS_BINS];    // colour drawn: 0 green, 1 red (busy), 2 yellow (strongest)

// ---------------------------------------------------------------- peak labels
struct BsLabel { int16_t x, y, ht, k, barX, barY; uint16_t mhz; };   // vertical text BS_VLBL_W wide, ht tall;
                                                                    // k = bar's bin, barX/barY = bar's top centre
static BsLabel bsLbl[2 * BS_BINS];
static int     bsNLbl = 0;

static int      bsRange = 0;             // graph top, dB above floor (span, or auto range)
static uint32_t bsRangeNeedMs = 0;
static bool     bsFooterDirty = true;

static int bsBins = BS_BINS;             // bars shown: 5300 MHz up to "Max MHz"
static int binX(int k) { return BS_GX + k * BS_GW / bsBins; }
static int binW(int k) { return binX(k + 1) - binX(k); }

static void bsAddPt(uint16_t f) {
  if (bsPts < BS_MAX_PTS && (bsPts == 0 || f != bsMhz[bsPts - 1])) bsMhz[bsPts++] = f;
}

// Every channel frequency, sorted and de-duplicated, with fill-in points:
// 3 between channels more than 35 MHz apart, 2 when more than 22, 1 when
// more than 9 (as in ArduVidRx's full-band scan). Starts 37 MHz below the
// lowest channel and ends 37 MHz above the highest.
static void bsBuildPoints() {
  uint16_t ch[NUM_BANDS * NUM_CH];
  int n = 0;
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) ch[n++] = FPV_MHZ[b][c];
  for (int i = 1; i < n; i++) { uint16_t k = ch[i]; int j = i - 1; while (j >= 0 && ch[j] > k) { ch[j + 1] = ch[j]; j--; } ch[j + 1] = k; }
  bsPts = 0;
  uint16_t prev = ch[0] - BS_EDGE_MHZ;
  bsAddPt(prev);
  for (int k = 0; k <= n; k++) {
    uint16_t next = (k < n) ? ch[k] : ch[n - 1] + BS_EDGE_MHZ;
    if (next == prev) continue;
    int d = next - prev;
    if (d > 35)      { int q = d / 4; bsAddPt(prev + q); bsAddPt(prev + 2 * q + 1); bsAddPt(prev + 3 * q + 1); }
    else if (d > 22) { int q = d / 3; bsAddPt(prev + q); bsAddPt(prev + 2 * q); }
    else if (d > 9)  { bsAddPt(prev + d / 2); }
    bsAddPt(next);
    prev = next;
  }
  // bars from BS_LO_MHZ up to the bin holding "Max MHz"
  int hiMhz = min(BS_LO_MHZ + BS_BINS * BS_BIN_MHZ, cfg.maxMhz - cfg.maxMhz % BS_BIN_MHZ + BS_BIN_MHZ);
  bsBins = constrain((hiMhz - BS_LO_MHZ) / BS_BIN_MHZ, 1, BS_BINS);
  // points per bin, points above "Max MHz" left out; empty bins between the
  // first and last point use the nearest point
  int lastPt = 0;
  for (int k = 0; k < BS_BINS; k++) { bsBinPt0[k] = -1; bsBinPtN[k] = 0; bsBinNear[k] = -1; }
  for (int i = 0; i < bsPts; i++) {
    int k = (bsMhz[i] - BS_LO_MHZ) / BS_BIN_MHZ;
    if (k < 0 || k >= bsBins || mhzBlocked(bsMhz[i])) continue;
    if (bsBinPt0[k] < 0) bsBinPt0[k] = i;
    bsBinPtN[k]++;
    lastPt = i;
  }
  for (int k = 0; k < bsBins; k++) {
    if (bsBinPtN[k] > 0) continue;
    float mid = BS_LO_MHZ + k * BS_BIN_MHZ + BS_BIN_MHZ / 2.0f;
    if (mid < bsMhz[0] || mid > bsMhz[lastPt]) continue;
    int best = 0; float bd = 1e9f;
    for (int i = 0; i <= lastPt; i++) { float d = fabsf(bsMhz[i] - mid); if (d < bd) { bd = d; best = i; } }
    bsBinNear[k] = best;
  }
}

// Start learning again on the next visit (the grid floor was reset).
void bscanForgetFloor() { bsFloorValid = false; }

// Floor from the learned grid floor, or learn it from scratch.
static void bsSeedFloor() {
  bool seed = gridFloorReady() && !bsNoSeedOnce;
  for (int i = 0; i < bsPts; i++) bsFloor[i] = seed ? gridFloorAt(bsMhz[i]) : 0;
  if (seed) { bsLearnUntil = bsSweeps; bsFloorValid = true; }
  else        bsLearnUntil = bsSweeps + SPEC_LEARN_SWEEPS;
  bsNoSeedOnce = false;
}

static void bsDrawAxis() {
  char s[12];
  lcdFill(0, BS_GY, BS_GX - 2, BS_GH, 0, 0, 0);
  snprintf(s, sizeof(s), "%d", bsRange);            lcdText(2, BS_GY, s, 1, C_DIM, C_BG);
  snprintf(s, sizeof(s), "%d", (bsRange + 1) / 2);  lcdText(2, BS_GY + BS_GH / 2 - 4, s, 1, C_DIM, C_BG);
  lcdText(2, BS_GY + BS_GH - 8, "0", 1, C_DIM, C_BG);
  lcdText(2, BS_GY + BS_GH - 18, "dB", 1, C_DIM, C_BG);
}

// Same vertical scale as the spectrum: the Span setting (bars above it clip
// at the top), or with "Auto range" On at least the span, growing at once to
// fit the highest bar and shrinking one step after SPEC_RANGE_HOLD_MS.
static bool bsUpdateRange(float maxAbove) {
  int need = cfg.span;
  if (cfg.specAutoRange && maxAbove > need) need = min(SPEC_RANGE_MAX, ((int)ceilf(maxAbove / SPEC_RANGE_STEP)) * SPEC_RANGE_STEP);
  uint32_t now = millis();
  int old = bsRange;
  if (!cfg.specAutoRange) bsRange = need;                // fixed at the span
  else if (need >= bsRange) { bsRange = need; bsRangeNeedMs = now; }
  else if (now - bsRangeNeedMs >= SPEC_RANGE_HOLD_MS) {
    bsRange = max(need, bsRange - SPEC_RANGE_STEP); bsRangeNeedMs = now;
  }
  if (bsRange == old) return false;
  bsDrawAxis();
  return true;
}

// While held (ENTER), a red "Hold" flashes about once a second above the
// hint, as on the IMD-Free page. force = redraw even if unchanged.
static int bsHoldShown = -1;
static void bsDrawHold(bool force) {
  int on = bsPaused && (millis() / 500) % 2 == 0;
  if (!force && on == bsHoldShown) return;
  lcdFill(0, BS_HOLD_Y, LCD_W, BS_HINT_Y - BS_HOLD_Y, 0, 0, 0);
  if (on) lcdText((LCD_W - 4 * 12) / 2, BS_HOLD_Y, "Hold", 2, C_RED, C_BG);
  bsHoldShown = on;
}

// Centred "ENTER: Hold" / "ENTER: Release" hint (as on the IMD-Free page),
// plus the flashing "Hold".
static void bsDrawHint() {
  lcdFill(0, BS_HINT_Y, 220, 8, 0, 0, 0);
  const char *act = bsPaused ? "Release" : "Hold";
  int x = (LCD_W - 6 * (7 + (int)strlen(act))) / 2;
  lcdText(x, BS_HINT_Y, "ENTER:", 1, C_TEXT, C_BG);
  lcdText(x + 42, BS_HINT_Y, act, 1, C_GOOD, C_BG);
  bsDrawHold(true);
}

void bscanDrawStatic() {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, "Band Scan", 2, C_ACC, C_BG);
  if (bsRange < cfg.span) { bsRange = cfg.span; bsRangeNeedMs = millis(); }
  bsDrawAxis();
  // graph frame
  lcdFillC(BS_GX - 1, BS_GY - 1, BS_GW + 2, 1, C_DIM);
  lcdFillC(BS_GX - 1, BS_GY + BS_GH, BS_GW + 2, 1, C_DIM);
  lcdFillC(BS_GX - 1, BS_GY - 1, 1, BS_GH + 2, C_DIM);
  lcdFillC(BS_GX + BS_GW, BS_GY - 1, 1, BS_GH + 2, C_DIM);
  // MHz labels with ticks every 100 MHz
  for (int f = BS_LO_MHZ + 100; f < BS_LO_MHZ + bsBins * BS_BIN_MHZ; f += 100) {
    int x = BS_GX + (f - BS_LO_MHZ) * BS_GW / (bsBins * BS_BIN_MHZ);
    char s[12]; snprintf(s, sizeof(s), "%d", f);
    lcdFillC(x, BS_GY + BS_GH + 1, 1, 3, C_DIM);
    lcdText(min(x - 12, LCD_W - 24), BS_AXIS_Y, s, 1, C_DIM, C_BG);
  }
  for (int k = 0; k < BS_BINS; k++) { bsShownH[k] = -1; bsShownKind[k] = -1; }
  bsNLbl = 0;
  bsFooterDirty = true;
  learnShownPct = -1; scanAnimY = -1;
  if (bsSweeps < bsLearnUntil) drawTitleStatus(0, true);
  bsDrawHint();
}

void bscanEnter() {
  if (bsPts == 0) bsBuildPoints();
  for (int i = 0; i < bsPts; i++) { bsSmooth[i] = NAN; bsAbove[i] = 0; }
  if (!bsFloorValid) bsSeedFloor();
  bsPaused = false;
  bscanDrawStatic();
}

// ENTER held: re-learn the floor from scratch.
void bscanRelearn() {
  bsFloorValid = false; bsNoSeedOnce = true;
  bscanEnter();
}

void bscanTogglePause() {
  bsPaused = !bsPaused;
  bsDrawHint();
  Serial.printf("band scan: %s\n", bsPaused ? "hold" : "released");
}

// One bar: background and grid lines above it, a 1 px gap on its right.
static void bsDrawBin(int k, int h, RGB col) {
  const int x = binX(k), w = binW(k), base = BS_GY + BS_GH, top = base - h;
  lcdFillC(x, BS_GY, w, BS_GH - h, C_BG);
  lcdFillC(x + w - 1, top, 1, h, C_BG);
  for (int q = 1; q < BS_GRID_N; q++) {
    int gy = base - q * BS_GH / BS_GRID_N;
    if (gy < top) lcdFillC(x, gy, w, 1, C_MARK);
    else          lcdFillC(x + w - 1, gy, 1, 1, C_MARK);
  }
  lcdFillC(x, top, w - 1, h, col);
}

// Channel names at a frequency ("F8/R8"), empty for a fill-in point.
static void bsChanNames(uint16_t mhz, char *buf, int len) {
  buf[0] = 0;
  for (int r = 0; r < NUM_BANDS; r++) {
    int b = btnOrder[r];
    for (int c = 0; c < NUM_CH; c++) {
      if (FPV_MHZ[b][c] != mhz) continue;
      int n = strlen(buf);
      snprintf(buf + n, len - n, "%s%c%d", n ? "/" : "", BAND_NAME[b], c + 1);
    }
  }
}

static void bsDrawFooter(int peakPt, float aboveDb) {
  learnShownPct = -1; scanAnimY = -1;
  lcdFill(0, BS_FOOT_Y, LCD_W, BS_HINT_Y - BS_FOOT_Y, 0, 0, 0);
  if (peakPt < 0) { drawScanning(BS_FOOT_Y + 2); return; }
  char names[16], s[40];
  bsChanNames(bsMhz[peakPt], names, sizeof(names));
  snprintf(s, sizeof(s), "Peak: %s%s%u MHz %d dB", names, names[0] ? " " : "",
           bsMhz[peakPt], (int)lroundf(aboveDb));
  lcdText(4, BS_FOOT_Y + 2, s, 2, C_ACC, C_BG);
}

// True if the frequency is an FPV channel (not a fill-in point).
static bool bsIsChannel(uint16_t mhz) {
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++)
      if (FPV_MHZ[b][c] == mhz) return true;
  return false;
}

// Peak label text for the "Peak labels" setting, one line (drawn rotated
// 90 degrees): Band = channel name(s) ("F1", "R7/F8"; MHz if the point isn't
// a channel), MHz = "5740", Both = "F1 5740".
static void bsLabelText(uint16_t mhz, char *buf, int len) {
  char nm[16];
  bsChanNames(mhz, nm, sizeof(nm));
  if (cfg.bsLabel == BS_LBL_MHZ || !nm[0]) snprintf(buf, len, "%u", mhz);
  else if (cfg.bsLabel == BS_LBL_BOTH)     snprintf(buf, len, "%s %u", nm, mhz);
  else                                     snprintf(buf, len, "%s", nm);
}

// Bins a label, or its leader down to its bar, lies over.
static bool bsLabelCovers(const BsLabel &L, int k) {
  const int x0 = min((int)L.x, (int)L.barX), x1 = max(L.x + BS_VLBL_W, L.barX + 1);
  return binX(k) + binW(k) > x0 && binX(k) < x1;
}

// One sweep of the whole band + redraw of the bars and labels that changed.
void bscanSweep() {
  static int lastPeakPt = -2, lastAbove = -999;
  static bool lastLearning = false;
  if (bsPaused) { bsDrawHold(false); return; }       // held: just flash "Hold"
  const uint32_t learnStart = bsLearnUntil - SPEC_LEARN_SWEEPS;
  const bool learningSweep = bsSweeps < bsLearnUntil;
  float raw[BS_MAX_PTS];
  bool  fb[BS_MAX_PTS];
  for (int i = 0; i < bsPts; i++) {
    if (mhzBlocked(bsMhz[i])) { raw[i] = NAN; fb[i] = true; }   // above "Max MHz": not measured
    else {
      raw[i] = readFreqDb(bsMhz[i], SPEC_SAMPLES, SETTLE_US);
      fb[i] = g_lastReadFallback;
    }
    if (evToggle || evEnterShort || evEnterLong || evConfig) return;  // react to input immediately
    if (learningSweep && (i % 8 == 7 || i == bsPts - 1)) {
      int done = (bsSweeps - learnStart) * bsPts + i + 1;
      drawLearnProgress(BS_FOOT_Y, done * 100 / (SPEC_LEARN_SWEEPS * bsPts));
      bsDrawHint();
    }
  }
  bsSweeps++;
  const bool learning = bsSweeps <= bsLearnUntil;
  for (int i = 0; i < bsPts; i++) {
    float v = raw[i];
    if (isnan(v)) continue;
    float &s = bsSmooth[i];
    if (isnan(s)) s = v;
    else s += (v > s ? 0.6f : 0.3f) * (v - s);
    if (fb[i]) continue;                   // fallback readings never set the floor
    if (bsFloor[i] == 0 || v < bsFloor[i]) bsFloor[i] = v;
    else if (!learning) bsFloor[i] += 0.01f;
  }
  // anti-learn clamp: no floor more than 4 dB above the median learned floor
  { float tmp[BS_MAX_PTS]; int n = 0;
    for (int i = 0; i < bsPts; i++) if (bsFloor[i] != 0) tmp[n++] = bsFloor[i];
    if (n > 0) {
      for (int i = 1; i < n; i++) { float k = tmp[i]; int j = i - 1; while (j >= 0 && tmp[j] > k) { tmp[j + 1] = tmp[j]; j--; } tmp[j + 1] = k; }
      float med = tmp[n / 2];
      for (int i = 0; i < bsPts; i++) if (bsFloor[i] > med + 4.0f) bsFloor[i] = med + 4.0f;
    } }
  if (!learning) bsFloorValid = true;
  for (int i = 0; i < bsPts; i++)
    bsAbove[i] = (isnan(bsSmooth[i]) || bsFloor[i] == 0) ? 0 : max(0.0f, bsSmooth[i] - bsFloor[i]);

  uint32_t tDraw0 = micros();
  // bin values: strongest point in each bin
  int   binPt[BS_BINS];                  // point shown by the bin, -1 = blank
  bool  binBlocked[BS_BINS];
  int   val[BS_BINS];                    // whole dB, for peak finding
  float mx = 0;
  for (int k = 0; k < bsBins; k++) {
    int p = bsBinNear[k];
    for (int j = 0; j < bsBinPtN[k]; j++) {
      int i = bsBinPt0[k] + j;
      if (p < 0 || bsAbove[i] > bsAbove[p]) p = i;
    }
    binPt[k] = p;
    binBlocked[k] = mhzBlocked(BS_LO_MHZ + k * BS_BIN_MHZ);
    val[k] = (p < 0 || binBlocked[k]) ? 0 : (int)lroundf(bsAbove[p]);
    if (!binBlocked[k] && p >= 0) mx = max(mx, bsAbove[p]);
  }
  bool rangeChanged = bsUpdateRange(mx);

  // strongest bar; a run of 3+ equal bars picks its centre
  int hot = -1;
  { int best = -1, bestV = BS_PEAK_MIN_DB - 1;
    for (int k = 0; k < bsBins; k++) if (val[k] > bestV) { bestV = val[k]; best = k; }
    if (best >= 0) {
      int j = best;
      while (j < bsBins - 1 && val[j + 1] == bestV) j++;
      if (j - best >= 3) best += (j - best) / 2;
      hot = best;
    } }

  int nPeak = 0;                                     // labels placed (below)

  // bar heights, then the labels they need
  int h[BS_BINS];
  for (int k = 0; k < bsBins; k++)
    h[k] = (binPt[k] < 0 || binBlocked[k]) ? 0 : constrain((int)lroundf(bsAbove[binPt[k]] * BS_GH / bsRange), 0, BS_GH);
  // Labels: every bar on a channel frequency at or above Busy level (the red
  // bars, and the yellow one when it's on a channel). Peaks between channels
  // aren't labelled. Placed strongest first:
  // just above the bar if free, else stacked higher (then lower, over other
  // bars) so adjacent bars all keep a label; a lifted label gets a dotted
  // leader down to its bar.
  int cand[2 * BS_BINS], nCand = 0;
  for (int k = 0; k < bsBins; k++)                  // channel bars only: a peak between
    if (!binBlocked[k] && binPt[k] >= 0 && val[k] >= BS_PEAK_MIN_DB &&   // channels gets no label
        bsIsChannel(bsMhz[binPt[k]])) cand[nCand++] = k;
  for (int a = 0; a < nCand; a++)                    // one label per frequency
    for (int b = a + 1; b < nCand; b++)
      if (cand[b] >= 0 && cand[a] >= 0 && bsMhz[binPt[cand[a]]] == bsMhz[binPt[cand[b]]]) {
        if (h[cand[b]] > h[cand[a]]) cand[a] = cand[b];
        cand[b] = -1;
      }
  { int m = 0; for (int a = 0; a < nCand; a++) if (cand[a] >= 0) cand[m++] = cand[a]; nCand = m; }
  for (int a = 1; a < nCand; a++) {                  // lowest bar first (ties: left first)
    int v = cand[a], b = a - 1;
    while (b >= 0 && (h[cand[b]] > h[v] || (h[cand[b]] == h[v] && cand[b] > v))) { cand[b + 1] = cand[b]; b--; }
    cand[b + 1] = v;
  }
  BsLabel lbl[2 * BS_BINS];
  for (int a = 0; a < nCand; a++) {
    int k = cand[a];
    BsLabel L;
    L.k = k; L.mhz = bsMhz[binPt[k]];
    char t[24];
    bsLabelText(L.mhz, t, sizeof(t));
    L.ht = 6 * (int)strlen(t);
    L.barX = binX(k) + binW(k) / 2 - 1;
    L.barY = BS_GY + BS_GH - h[k];
    // Centred on its bar. Labels go lowest bar first, each just above its
    // bar, or above any label already there, so adjacent labels step upward
    // from the lowest bar to the highest. With no room left at the top, the
    // first free spot below that (over the bar) is used.
    const int x = constrain(L.barX - BS_VLBL_W / 2 + 1, BS_GX, BS_GX + BS_GW - BS_VLBL_W);
    const int want = L.barY - L.ht - 2;
    auto hitAt = [&](int y) -> int {                 // index of a placed label in the way, -1 = free
      for (int n = 0; n < nPeak; n++) {
        const BsLabel &o = lbl[n];
        if (x < o.x + BS_VLBL_W + 1 && o.x < x + BS_VLBL_W + 1 && y < o.y + o.ht + 2 && o.y < y + L.ht + 2) return n;
      }
      return -1;
    };
    int bx = x, by = min(want, BS_GY + BS_GH - L.ht);
    for (int n; by >= BS_GY + 1 && (n = hitAt(by)) >= 0; ) by = lbl[n].y - L.ht - 2;
    if (by < BS_GY + 1) {                            // top full: first free spot below
      by = -1;
      for (int y = max(BS_GY + 1, want); y <= BS_GY + BS_GH - L.ht; y++) if (hitAt(y) < 0) { by = y; break; }
      if (by < 0) bx = -1;
    }
    if (bx < 0) continue;                            // no room anywhere: no label
    L.x = bx; L.y = by;
    lbl[nPeak++] = L;
  }
  bool lblChanged = nPeak != bsNLbl || rangeChanged;
  for (int n = 0; n < nPeak && !lblChanged; n++)
    if (lbl[n].mhz != bsLbl[n].mhz || lbl[n].x != bsLbl[n].x || lbl[n].y != bsLbl[n].y ||
        lbl[n].ht != bsLbl[n].ht || lbl[n].barY != bsLbl[n].barY) lblChanged = true;

  // redraw changed bars, and bars under a label that moved or went away
  bool drawn[BS_BINS];
  for (int k = 0; k < bsBins; k++) {
    bool under = false;
    if (lblChanged)
      for (int n = 0; n < bsNLbl; n++)
        if (bsLabelCovers(bsLbl[n], k)) { under = true; break; }
    drawn[k] = false;
    if (binBlocked[k]) {
      if (bsShownH[k] != -3 || under) { lcdFillC(binX(k), BS_GY, binW(k), BS_GH, C_BLOCKED); bsShownH[k] = -3; drawn[k] = true; }
      continue;
    }
    bool isHot = k == hot;
    const int kind = isHot ? 2 : (val[k] >= BS_PEAK_MIN_DB && bsIsChannel(bsMhz[binPt[k]])) ? 1 : 0;
    if (h[k] == bsShownH[k] && kind == bsShownKind[k] && !under) continue;
    // strongest bar yellow (like the footer); at or above Busy level and on a
    // channel frequency red (a fill-in point between channels stays green);
    // the rest green like the Activity grid; red and green brighter the taller
    // (stronger) they are, never quite black
    const float lvl = BS_BAR_MIN + (1.0f - BS_BAR_MIN) * h[k] / BS_GH;
    bsDrawBin(k, h[k], kind == 2 ? C_ACC : scaleColor(kind == 1 ? BS_C_BUSY : C_HOT, lvl));
    bsShownH[k] = h[k]; bsShownKind[k] = kind; drawn[k] = true;
    if ((k & 15) == 0) pollInput();
  }
  // leaders, then labels on top. If anything under a label or a leader was
  // redrawn, all of them are redrawn (they can overlap each other).
  bool need = lblChanged;
  for (int n = 0; n < nPeak && !need; n++)
    for (int k = 0; k < bsBins && !need; k++)
      if (drawn[k] && bsLabelCovers(lbl[n], k)) need = true;
  if (need) {
    for (int n = 0; n < nPeak; n++) {                // dotted leader: label foot down to its bar
      const BsLabel &L = lbl[n];
      for (int y = L.y + L.ht + 2; y < L.barY - 1; y += 2) lcdFillC(L.barX, y, 1, 1, C_DIM);
    }
    for (int n = 0; n < nPeak; n++) {                // text rotated 90 degrees, reads upward
      const BsLabel &L = lbl[n];
      char t[24];
      bsLabelText(L.mhz, t, sizeof(t));
      lcdTextV(L.x, L.y, t, C_TEXT, C_BG);
    }
  }
  for (int n = 0; n < nPeak; n++) bsLbl[n] = lbl[n];
  bsNLbl = nPeak;

  // footer: the strongest bar's point, or Scanning / Learning
  int peakPt = hot >= 0 ? binPt[hot] : -1;
  float peakDb = peakPt >= 0 ? bsAbove[peakPt] : 0;
  int aboveR = peakPt >= 0 ? (int)lroundf(peakDb) : -1;
  if (learning) {
    drawLearnProgress(BS_FOOT_Y, (bsSweeps - learnStart) * 100 / SPEC_LEARN_SWEEPS);
    bsDrawHint();
    lastLearning = true; bsFooterDirty = false;
  } else if (lastLearning || bsFooterDirty || peakPt != lastPeakPt || aboveR != lastAbove) {
    if (lastLearning) lcdFill(0, BS_FOOT_Y, LCD_W, LCD_H - BS_FOOT_Y, 0, 0, 0);   // learning bar and span label
    bsDrawFooter(peakPt, peakDb);
    bsDrawHint();
    lastLearning = false; bsFooterDirty = false; lastPeakPt = peakPt; lastAbove = aboveR;
  }
  updateScanDots();
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
      Serial.printf("band scan %d pts, %.1f sweeps/s, peak %u MHz %.1f dB, %d labels\n",
                    bsPts, sps, peakPt >= 0 ? bsMhz[peakPt] : 0, peakDb, nPeak);
      printTiming(sweepsThisPeriod);
      printRssiStats();
      printStalls();
    } else {
      resetTiming();
    }
  }
}
