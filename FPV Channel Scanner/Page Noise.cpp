/*
 * FPV Channel Scanner - Page Noise.cpp
 * Revision: 96
 *
 * FPV Channel Noise page: long-term statistics for every channel
 * (average dB above floor, % busy, peak), shaded quietest to busiest.
 */

#include "Scanner.h"

void actClear() {
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      actSumDb[b][c] = 0; actN[b][c] = 0; actBusyN[b][c] = 0; actPeakDb[b][c] = 0;
    }
  actLastMs = 0;
  actTrackedS = 0;
}

void actInvalidate() {
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) { actShownLevel[b][c] = -1; actShownPeak[b][c] = false; actShownRank[b][c] = -1; }
}

void actDrawStatic() {
  drawGridFrame("FPV Channel Noise", C_RED);
  actInvalidate();
  actFooterDirty = true;
}

// (internally MODE_ACTIVITY / act*). Every channel keeps its own long-term
// statistics from start-up (or the last reset): the average of its readings
// above the floor, how often it was busy, and its strongest reading. Nothing
// decays, so the longer it runs the more clearly quiet channels stand out.
// The display shades each box between the quietest channel (black) and the
// busiest (red) by long-term average.
void actAccumulate(float raw[NUM_BANDS][NUM_CH]) {
  uint32_t now = millis();
  float dt = actLastMs ? (now - actLastMs) / 1000.0f : 0.2f;
  actLastMs = now;
  if (dt > ACT_MAX_DT_S) dt = ACT_MAX_DT_S;  // a pause (other page) isn't counted
  actTrackedS += dt;
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      if (isnan(raw[b][c]) || gridFloor[b][c] == 0) continue;
      float above = raw[b][c] - gridFloor[b][c];
      if (above < 0) above = 0;
      actSumDb[b][c] += above;
      actN[b][c]++;
      if (above >= ACT_BUSY_DB) actBusyN[b][c]++;
      if (above > actPeakDb[b][c]) actPeakDb[b][c] = above;
    }
}

// Fill order[] with channel indexes (band*NUM_CH + channel), quietest first
// by long-term average. Ties keep table order so ranks don't flicker.
void actRankOrder(int *order) {
  for (int i = 0; i < NUM_BANDS * NUM_CH; i++) order[i] = i;
  for (int i = 1; i < NUM_BANDS * NUM_CH; i++) {
    int k = order[i]; float kv = actAvg(k / NUM_CH, k % NUM_CH); int j = i - 1;
    while (j >= 0 && actAvg(order[j] / NUM_CH, order[j] % NUM_CH) > kv) { order[j + 1] = order[j]; j--; }
    order[j + 1] = k;
  }
}

// Channel's long-term average signal above the floor, in dB.
float actAvg(int b, int c) {
  return actN[b][c] ? (float)(actSumDb[b][c] / actN[b][c]) : 0;
}

// Percentage of readings where the channel was busy.
int actBusyPct(int b, int c) {
  return actN[b][c] ? (int)((uint64_t)actBusyN[b][c] * 100 / actN[b][c]) : 0;
}

// rank: 1 = quietest .. 48 = busiest (fewer in HDZero mode), 0 = no rank
void actDrawCell(int b, int c, int level, bool peak, int rank) {
  if (!gridShown(b, c)) return;                     // HDZero mode: blank where HDZero can't tune
  int x = GRID_X + c * (CELL_W + CELL_GAP);
  int y = GRID_Y + bandRow[b] * (CELL_H + CELL_GAP);
  if (level == -2) { drawNoDataBox(x, y, b, c); return; }   // channel can't be measured
  RGB col = scaleColor(C_RED, level / 32.0f);          // black -> red
  drawGridBox(x, y, col, peak);
  char f[6];
  snprintf(f, sizeof(f), "%u", FPV_MHZ[b][c]);
  RGB tc = level > 16 ? RGB{ 0, 0, 0 } : C_DIM;
  lcdText(x + (CELL_W - 24) / 2, y + CELL_H - 12, f, 1, tc, col);
  if (rank > 0) {
    char r[3];
    snprintf(r, sizeof(r), "%d", rank);
    int w = 9 * (int)strlen(r);                        // 1.5x digits, centred
    RGB rc = level > 16 ? RGB{ 0, 0, 0 } : C_RANK;
    lcdTextSized(x + (CELL_W - w) / 2, y + 8, r, 9, 12, rc, col);
  }
}

void actDrawFooter(int pb, int pc, float excessDb) {
  char s[40];
  learnShownPct = -1; scanAnimY = -1;
  lcdFill(0, FOOT_Y, LCD_W, LCD_H - FOOT_Y, 0, 0, 0);
  if (pb >= 0) {
    snprintf(s, sizeof(s), "%c%d %u %.1f dB %d%%", BAND_NAME[pb], pc + 1, FPV_MHZ[pb][pc], excessDb,
             actBusyPct(pb, pc));
    lcdText(4, FOOT_Y + 2, s, 2, C_ACC, C_BG);
    char t[32];
    snprintf(t, sizeof(t), "Quietest - %s", BAND_LONG[pb]);
    lcdText(4, FOOT_Y + 20, t, 1, C_DIM, C_BG);
  } else {
    drawScanning(FOOT_Y + 2);
  }
  drawSpanLabel();
}

void actDraw(bool learning, uint32_t learnStart) {
  static int lastB = -2, lastC = -2, lastTenths = -9999;
  static bool lastLearning = false;
  float busiest = -1e9f;                    // full red
  int qb = -1, qc = -1; float quiet = 1e9f;  // black; outlined + named
  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      if (!gridShown(b, c) || chanNoData(b, c)) continue;   // shading, quietest, ranks: live drawn boxes only
      float a = actAvg(b, c);
      if (a > busiest) busiest = a;
      if (a < quiet) { quiet = a; qb = b; qc = c; }
    }
  // Quietest = black, busiest = full red. If every channel is within
  // ACT_MIN_SCALE_DB of the quietest, the spread is shown dim instead of
  // stretching tiny noise differences to full red.
  float scale = max(busiest - quiet, ACT_MIN_SCALE_DB);
  bool havePeak = !learning && actLastMs != 0;
  int pb = qb, pc = qc;

  int8_t rank[NUM_BANDS][NUM_CH];
  int order[NUM_BANDS * NUM_CH];
  actRankOrder(order);
  int nRank = 0;                            // 1 = quietest drawn box
  for (int i = 0; i < NUM_BANDS * NUM_CH; i++) {
    int b = order[i] / NUM_CH, c = order[i] % NUM_CH;
    rank[b][c] = (havePeak && gridShown(b, c) && !chanNoData(b, c)) ? (int8_t)(++nRank) : 0;
  }

  for (int b = 0; b < NUM_BANDS; b++)
    for (int c = 0; c < NUM_CH; c++) {
      float e = learning ? 0 : (actAvg(b, c) - quiet) / scale;   // 0..1
      e = constrain(e, 0.0f, 1.0f);
      int level = (int)lroundf(32.0f * powf(e, ACT_GAMMA));      // gain curve
      if (chanNoData(b, c)) level = -2;                          // no live data: "--"
      bool peak = havePeak && b == pb && c == pc;
      if (level != actShownLevel[b][c] || peak != actShownPeak[b][c] || rank[b][c] != actShownRank[b][c]) {
        actDrawCell(b, c, level, peak, rank[b][c]);
        actShownLevel[b][c] = level; actShownPeak[b][c] = peak; actShownRank[b][c] = rank[b][c];
      }
      pollInput();
    }

  int footB = havePeak ? pb : -1, footC = havePeak ? pc : -1;
  int tenths = (havePeak && qb >= 0) ? (int)lroundf(quiet * 10) : -1;   // qb < 0: no live channel
  if (learning) {
    drawLearnProgress(FOOT_Y, (gridSweeps - learnStart) * 100 / GRID_LEARN_SWEEPS);
  } else if (lastLearning) {
    actFooterDirty = true;                   // learning just finished
  }
  lastLearning = learning;
  if (!learning && (footB != lastB || footC != lastC || tenths != lastTenths || actFooterDirty)) {
    actDrawFooter(footB, footC, quiet);
    lastB = footB; lastC = footC; lastTenths = tenths; actFooterDirty = false;
  }
}
