/*
 * FPV Channel Scanner - Page Quietest.cpp
 * Revision: 34
 *
 * Quietest Channels page: the QUIET_LIST_N quietest channels from the
 * Channel Noise history, in two columns of 8: left column 1-8, right
 * column 9-16. Measurement keeps running on this page, so the history
 * and the list stay live.
 */

#include "Scanner.h"

void quietInvalidate() {
  for (int i = 0; i < QUIET_LIST_N; i++) { quietShownIdx[i] = -2; quietShownTenths[i] = -1; quietShownBusy[i] = -1; }
}

void quietDrawStatic() {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, "Quietest Channels", 2, C_ACC, C_BG);
  lcdFillC(QUIET_COL_W, QUIET_Y0 - 2, 1, 8 * QUIET_PITCH, C_MARK);   // column divider
  quietInvalidate();
  quietFooterDirty = true;
  learnShownPct = -1; scanAnimY = -1;
}

// slot 0..15; idx = band*NUM_CH + channel, or -1 to blank the slot
void quietDrawEntry(int slot, int idx) {
  int x = 6 + (slot / 8) * QUIET_COL_W;
  int y = QUIET_Y0 + (slot % 8) * QUIET_PITCH;
  lcdFill(x, y, QUIET_COL_W - 10, QUIET_PITCH - 2, 0, 0, 0);
  if (idx < 0) return;
  int b = idx / NUM_CH, c = idx % NUM_CH;
  char t[24];
  snprintf(t, sizeof(t), "%2d", slot + 1);
  lcdText(x, y, t, 2, C_RANK, C_BG);                       // position in the list
  snprintf(t, sizeof(t), "%c%d", BAND_NAME[b], c + 1);
  lcdText(x + 30, y, t, 2, C_ACC, C_BG);                   // band / channel
  snprintf(t, sizeof(t), "%u", FPV_MHZ[b][c]);
  lcdText(x + 62, y, t, 2, C_TEXT, C_BG);                  // frequency
  snprintf(t, sizeof(t), "%.1f dB, %d%% busy", actAvg(b, c), actBusyPct(b, c));
  lcdText(x + 30, y + 18, t, 1, C_DIM, C_BG);
}

void quietDraw(bool learning, uint32_t learnStart) {
  static bool lastLearning = false;
  bool ready = !learning && actLastMs != 0;
  int order[NUM_BANDS * NUM_CH];
  actRankOrder(order);
  for (int i = 0; i < QUIET_LIST_N; i++) {
    int idx = ready ? order[i] : -1;
    int tenths = -1, busy = -1;
    if (idx >= 0) {
      tenths = (int)lroundf(actAvg(idx / NUM_CH, idx % NUM_CH) * 10);
      busy = actBusyPct(idx / NUM_CH, idx % NUM_CH);
    }
    if (idx != quietShownIdx[i] || tenths != quietShownTenths[i] || busy != quietShownBusy[i]) {
      quietDrawEntry(i, idx);
      quietShownIdx[i] = idx; quietShownTenths[i] = tenths; quietShownBusy[i] = busy;
    }
    pollInput();
  }

  if (learning) {
    drawLearnProgress(FOOT_Y, (gridSweeps - learnStart) * 100 / GRID_LEARN_SWEEPS);
  } else if (lastLearning) {
    quietFooterDirty = true;                 // learning just finished
  }
  lastLearning = learning;
  static bool lastReady = false;
  if (ready != lastReady) { quietFooterDirty = true; lastReady = ready; }
  if (!learning && quietFooterDirty) {
    learnShownPct = -1; scanAnimY = -1;
    lcdFill(0, FOOT_Y, LCD_W, LCD_H - FOOT_Y, 0, 0, 0);
    if (ready) lcdText(4, FOOT_Y + 20, "1 = quietest", 1, C_DIM, C_BG);
    else       drawScanning(FOOT_Y + 2);
    drawSpanLabel();
    quietFooterDirty = false;
  }
}
