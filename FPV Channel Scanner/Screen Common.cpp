/*
 * FPV Channel Scanner - Screen Common.cpp
 * Revision: 103
 *
 * Drawing shared by several pages (grid frame, Scanning dots, span /
 * history label, learning progress, title status), the pop-up, and
 * page navigation.
 */

#include "Scanner.h"

// Title, channel numbers and band letters - shared by GRID and ACTIVITY.
// Blue video system tag ("HDZ", "DJI", "WS", "IPC") at the top left, under
// the title: left of the column numbers on the grids, the band buttons on the
// Spectrum and the graph's top on Band Scan. Nothing for All bands/chans.
void drawSysMarker() {
  if (sysOn()) lcdText(4, HDR_Y + 4, sysTag(), 1, C_HDZ, C_BG);
}

void drawGridFrame(const char *title, RGB titleColor) {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, title, 2, titleColor, C_BG);
  drawPageHints();                                 // button hints, bottom line
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
  drawSysMarker();
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

// Bottom right of the footer: the span on the pages that use it (Activity,
// Band Scan, Spectrum), the noise history time on Noise / Quietest / IMD-Free.
// With Auto range On: "Span auto 70 dB (min 50)" while the scale is above
// the span setting, "Span auto 50 dB" at it.
void drawSpanLabel() {
  char s[64];
  if (mode == MODE_ACTIVITY || quietPage()) {
    // D:HH:MM:SS; fixed width (up to 9 days) so no leftovers when it changes
    unsigned long t = (unsigned long)actTrackedS;
    snprintf(s, sizeof(s), "History %lu:%02lu:%02lu:%02lu", t / 86400, t / 3600 % 24, t / 60 % 60, t % 60);
  } else {
    int now = mode == MODE_GRID ? gridRangeNow() : mode == MODE_SPECTRUM ? specRangeNow() : bsRangeNow();
    if (now < cfg.span) now = cfg.span;
    char t[40];
    if (!cfg.specAutoRange) snprintf(t, sizeof(t), "Span %d dB", cfg.span);
    else if (now > cfg.span) snprintf(t, sizeof(t), "Span auto %d dB (min %d)", now, cfg.span);
    else snprintf(t, sizeof(t), "Span auto %d dB", now);
    snprintf(s, sizeof(s), "%24s", t);             // fixed width: no leftovers when it shortens
  }
  lcdText(LCD_W - 6 * (int)strlen(s) - 4, FOOT_Y + 20, s, 1, C_DIM, C_BG);
}

// "LEARNING nn%" plus a progress bar, drawn in a page's footer area.
// Only the parts that change are redrawn.
void drawLearnProgress(int footY, int pct) {
  pct = constrain(pct, 0, 100);
  if (learnShownY != footY || learnShownPct < 0) {
    lcdFill(0, footY, LCD_W, LEGEND_Y - footY, 0, 0, 0);
    lcdFillC(4, footY + 20, LEARN_BAR_W + 4, 8, C_DIM);       // bar outline
    lcdFill(6, footY + 22, LEARN_BAR_W, 4, 0, 0, 0);
    drawSpanLabel();
    learnShownPct = -1; scanAnimY = -1;
  }
  if (pct == learnShownPct) return;
  char s[16];
  snprintf(s, sizeof(s), "Learning %3d%%", pct);
  lcdText(4, footY + 2, s, 2, C_ACC, C_BG);
  int w = LEARN_BAR_W * pct / 100;
  lcdFillC(6, footY + 22, w, 4, C_ACC);
  lcdFill(6 + w, footY + 22, LEARN_BAR_W - w, 4, 0, 0, 0);
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
  if (highlight) { lcdFillC(x, y, CELL_W, CELL_H, C_ACC); in = 2; }   // same amber as the footer text
  lcdFillC(x + in, y + in, CELL_W - 2 * in, CELL_H - 2 * in, fill);
}

// Grid box for a channel this board can't measure (chanNoData): dark,
// "--" in the middle, frequency in dim grey.
void drawNoDataBox(int x, int y, int b, int c) {
  const RGB fill = { 18, 18, 18 };
  drawGridBox(x, y, fill, false);
  lcdText(x + (CELL_W - 24) / 2, y + 8, "--", 2, RGB{ 90, 90, 90 }, fill);
  char f[6];
  snprintf(f, sizeof(f), "%u", FPV_MHZ[b][c]);
  lcdText(x + (CELL_W - 24) / 2, y + CELL_H - 12, f, 1, RGB{ 70, 70, 70 }, fill);
}

// Page reached from page m by KEY_R (dir +1) or KEY_L (dir -1).
ScreenMode pageAfter(ScreenMode m, int dir) {
  if (dir > 0) {
    switch (m) {
      case MODE_GRID:     return MODE_BANDSCAN;
      case MODE_BANDSCAN: return MODE_SPECTRUM;
      case MODE_SPECTRUM: return MODE_ACTIVITY;
      case MODE_ACTIVITY: return sysOn() ? MODE_HDZERO : MODE_QUIET;   // video system: its list only
      case MODE_QUIET:    return MODE_IMD;
      case MODE_HDZERO:   return MODE_IMD;
      default:            return MODE_GRID;       // from IMD-Free
    }
  }
  switch (m) {
    case MODE_GRID:     return MODE_ACTIVITY;     // skips both quietest lists
    case MODE_BANDSCAN: return MODE_GRID;
    case MODE_SPECTRUM: return MODE_BANDSCAN;
    case MODE_ACTIVITY: return MODE_SPECTRUM;
    case MODE_IMD:      return sysOn() ? MODE_HDZERO : MODE_QUIET;
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
  // Coming back to the grid pages from another page: take the shared floor
  // (it may have been refined there); among the grid pages it carries over.
  static bool gridLive = false;
  const bool grid = m == MODE_GRID || m == MODE_ACTIVITY || m == MODE_QUIET || m == MODE_HDZERO || m == MODE_IMD;
  if (grid && !gridLive) gridSeed();
  gridLive = grid;
  mode = m;
  popupUntilMs = 0;
  if (mode == MODE_GRID) {
    gridDrawStatic();
  } else if (mode == MODE_ACTIVITY) {
    actDrawStatic();
  } else if (quietPage()) {
    quietDrawStatic();
  } else if (mode == MODE_BANDSCAN) {
    bscanEnter();
  } else {
    specSetBand(specBand);
    specDrawStatic();
  }
  saveLastPage();
}

// Button hints on the bottom line of every page, in key order: "Left:" at
// the left edge, "ENTER:" centred between, "Right:" against the right edge.
// Key names in white, actions in blue.
// "Long press ENTER: <what>", centred just above the button hints; key in
// white, action in blue like the hints.
void drawLongPressLegend(const char *what) {
  static const char KEY[] = "Long press ENTER: ";
  int x = (LCD_W - 6 * (int)(strlen(KEY) + strlen(what))) / 2;
  lcdFill(0, LEGEND_Y, LCD_W, 8, 0, 0, 0);
  lcdText(x, LEGEND_Y, KEY, 1, C_TEXT, C_BG);
  lcdText(x + 6 * (int)strlen(KEY), LEGEND_Y, what, 1, C_KEYACT, C_BG);
}

void drawHintLine(const char *l, const char *e, const char *r) {
  lcdFill(0, HINT_Y, LCD_W, LCD_H - HINT_Y, 0, 0, 0);
  int leftEnd = 4 + 6 * (6 + (int)strlen(l));
  int xr = LCD_W - 4 - 6 * (7 + (int)strlen(r));
  int xe = (leftEnd + xr - 6 * (7 + (int)strlen(e))) / 2;
  lcdText(4,  HINT_Y, "Left:", 1, C_TEXT, C_BG);   lcdText(4 + 36,  HINT_Y, l, 1, C_KEYACT, C_BG);
  lcdText(xe, HINT_Y, "ENTER:", 1, C_TEXT, C_BG);  lcdText(xe + 42, HINT_Y, e, 1, C_KEYACT, C_BG);
  lcdText(xr, HINT_Y, "Right:", 1, C_TEXT, C_BG);  lcdText(xr + 42, HINT_Y, r, 1, C_KEYACT, C_BG);
}

static const char *pageShortName(ScreenMode m) {
  switch (m) {
    case MODE_GRID:     return "Activity";
    case MODE_BANDSCAN: return "Band Scan";
    case MODE_SPECTRUM: return "Spectrum";
    case MODE_ACTIVITY: return "Noise";
    case MODE_QUIET:    return "Quietest";
    case MODE_HDZERO:   return "Quietest";
    default:            return "IMD-Free";
  }
}

extern bool bsPaused, imdHold;
// Scanner pages: Left / Right name the page they go to; ENTER says what a
// short press does here.
void drawPageHints() {
  const char *e;
  switch (mode) {
    case MODE_GRID:     e = "Span"; break;
    case MODE_SPECTRUM: e = "Next band"; break;
    case MODE_BANDSCAN: e = bsPaused ? "Release" : "Hold"; break;
    case MODE_IMD: {                                  // no sets: ENTER explains why
      extern bool imdHelpOpen; extern bool imdNoSets();
      e = imdHelpOpen ? "Close" : imdNoSets() ? "Help" : imdHold ? "Release" : "Hold"; break; }
    default:            e = "Clear"; break;           // Noise and both Quietest lists
  }
  drawHintLine(pageShortName(pageAfter(mode, -1)), e, pageShortName(pageAfter(mode, +1)));
  // IMD-Free while held: long press switches the ranking; elsewhere it re-learns the floor
  extern bool imdBestFirst;
  drawLongPressLegend(mode == MODE_IMD && imdHold ? (imdBestFirst ? "Quietest first" : "Best IMD first")
                                                  : "Re-learn floor");
}
