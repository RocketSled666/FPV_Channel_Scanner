/*
 * FPV Channel Scanner - Screen Common.cpp
 * Revision: 79
 *
 * Drawing shared by several pages (grid frame, Scanning dots, span /
 * history label, learning progress, title status), the pop-up, and
 * page navigation.
 */

#include "Scanner.h"

// Title, channel numbers and band letters - shared by GRID and ACTIVITY.
void drawGridFrame(const char *title, RGB titleColor) {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, title, 2, titleColor, C_BG);
  for (int c = 0; c < NUM_CH; c++) {
    char s[2] = { (char)('1' + c), 0 };
    lcdText(GRID_X + c * (CELL_W + CELL_GAP) + (CELL_W - 12) / 2, HDR_Y, s, 2, C_DIM, C_BG);
  }
  for (int r = 0; r < NUM_BANDS; r++) {
    bool any = false;                          // HDZero mode: no letter for an empty band
    for (int c = 0; c < NUM_CH; c++) if (gridShown(btnOrder[r], c)) any = true;
    if (!any) continue;
    char s[2] = { BAND_NAME[btnOrder[r]], 0 };
    lcdText(8, GRID_Y + r * (CELL_H + CELL_GAP) + (CELL_H - 24) / 2, s, 3, C_TEXT, C_BG);
  }
  if (cfg.hdzero) lcdText(4, HDR_Y + 4, "HDZ", 1, C_HDZ, C_BG);   // mode indicator
  learnShownPct = -1; scanAnimY = -1;
}

// "Scanning" followed by 1, 2, 3 dots, repeating. The word is drawn once;
// updateScanDots() redraws only the three dot positions.
void updateScanDots() {
  if (scanAnimY < 0) return;
  uint32_t now = millis();
  if (now - scanDotsMs < SCAN_DOT_MS) return;
  scanDotsMs = now;
  scanDots = scanDots % 3 + 1;
  char d[4] = { ' ', ' ', ' ', 0 };
  for (int i = 0; i < scanDots; i++) d[i] = '.';
  lcdText(4 + 8 * 12, scanAnimY, d, 2, C_DIM, C_BG);   // right after "Scanning"
}

void drawScanning(int y) {
  lcdText(4, y, "Scanning", 2, C_DIM, C_BG);
  scanAnimY = y;
  scanDots = 0;
  scanDotsMs = 0;          // show the first dot right away
  updateScanDots();
}

// "SPAN nndB" at the bottom right; same screen position on both pages.
void drawSpanLabel() {
  char s[64];
  if (mode == MODE_ACTIVITY || quietPage()) {
    // D:HH:MM:SS; fixed width (up to 9 days) so no leftovers when it changes
    unsigned long t = (unsigned long)actTrackedS;
    snprintf(s, sizeof(s), "History %lu:%02lu:%02lu:%02lu", t / 86400, t / 3600 % 24, t / 60 % 60, t % 60);
  }
  else snprintf(s, sizeof(s), "Span %d dB", cfg.span);
  lcdText(LCD_W - 6 * (int)strlen(s) - 4, FOOT_Y + 20, s, 1, C_DIM, C_BG);
}

// "LEARNING nn%" plus a progress bar, drawn in a page's footer area.
// Only the parts that change are redrawn.
void drawLearnProgress(int footY, int pct) {
  pct = constrain(pct, 0, 100);
  if (learnShownY != footY || learnShownPct < 0) {
    lcdFill(0, footY, LCD_W, LCD_H - footY, 0, 0, 0);
    lcdFillC(4, footY + 20, LEARN_BAR_W + 4, 10, C_DIM);      // bar outline
    lcdFill(6, footY + 22, LEARN_BAR_W, 6, 0, 0, 0);
    drawSpanLabel();
    learnShownPct = -1; scanAnimY = -1;
  }
  if (pct == learnShownPct) return;
  char s[16];
  snprintf(s, sizeof(s), "Learning %3d%%", pct);
  lcdText(4, footY + 2, s, 2, C_ACC, C_BG);
  int w = LEARN_BAR_W * pct / 100;
  lcdFillC(6, footY + 22, w, 6, C_ACC);
  lcdFill(6 + w, footY + 22, LEARN_BAR_W - w, 6, 0, 0, 0);
  learnShownPct = pct; learnShownY = footY;
}

void drawTitleStatus(float sweepsPerSec, bool learning) {
  char s[24];
  if (learning) snprintf(s, sizeof(s), "Learning  ");
  else          snprintf(s, sizeof(s), "%4.1f sw/s", sweepsPerSec);
  lcdText(LCD_W - 6 * 10 - 4, 8, s, 1, learning ? C_ACC : C_DIM, C_BG);
}

// One grid box: optional white highlight (outermost), optional blue HDZero
// outline inside it, then the fill. Shared by the Activity and Noise grids.
void drawGridBox(int x, int y, RGB fill, bool highlight) {
  int in = 0;
  if (highlight) { lcdFillC(x, y, CELL_W, CELL_H, C_PEAKH); in = 2; }
  lcdFillC(x + in, y + in, CELL_W - 2 * in, CELL_H - 2 * in, fill);
}

// Page reached from page m by KEY_R (dir +1) or KEY_L (dir -1).
ScreenMode pageAfter(ScreenMode m, int dir) {
  if (dir > 0) {
    switch (m) {
      case MODE_GRID:     return MODE_SPECTRUM;
      case MODE_SPECTRUM: return MODE_ACTIVITY;
      case MODE_ACTIVITY: return cfg.hdzero ? MODE_HDZERO : MODE_QUIET;   // HDZero mode: HDZero list only
      case MODE_QUIET:    return MODE_IMD;
      case MODE_HDZERO:   return MODE_IMD;
      default:            return MODE_GRID;       // from IMD-Free
    }
  }
  switch (m) {
    case MODE_GRID:     return MODE_ACTIVITY;     // skips both quietest lists
    case MODE_SPECTRUM: return MODE_GRID;
    case MODE_ACTIVITY: return MODE_SPECTRUM;
    case MODE_IMD:      return cfg.hdzero ? MODE_HDZERO : MODE_QUIET;
    default:            return MODE_ACTIVITY;     // from either Quietest list
  }
}

// Small centred message box; removed by loop() after POPUP_MS.
void showPopup(const char *msg) {
  const int w = POPUP_W, h = POPUP_H, x = POPUP_X, y = POPUP_Y;
  lcdFillC(x, y, w, h, C_ACC);                       // border
  lcdFill(x + 3, y + 3, w - 6, h - 6, 30, 30, 30);
  int tw = 12 * (int)strlen(msg);
  lcdText((LCD_W - tw) / 2, y + (h - 16) / 2, msg, 2, C_TEXT, RGB{ 30, 30, 30 });
  popupUntilMs = millis() + POPUP_MS;
  if (popupUntilMs == 0) popupUntilMs = 1;
}

void enterMode(ScreenMode m) {
  mode = m;
  popupUntilMs = 0;
  if (mode == MODE_GRID) {
    gridDrawStatic();
  } else if (mode == MODE_ACTIVITY) {
    actDrawStatic();
  } else if (quietPage()) {
    quietDrawStatic();
  } else {
    specSetBand(specBand);
    specDrawStatic();
  }
}
