/*
 * FPV Channel Scanner - Screen Common.cpp
 * Revision: 41
 *
 * Drawing shared by several pages (grid frame, Scanning dots, span /
 * history label, learning progress, title status), the pop-up, and
 * page navigation.
 */

#include "Scanner.h"
#include <Preferences.h>

// Title, channel numbers and band letters - shared by GRID and ACTIVITY.
void drawGridFrame(const char *title, RGB titleColor) {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, title, 2, titleColor, C_BG);
  for (int c = 0; c < NUM_CH; c++) {
    char s[2] = { (char)('1' + c), 0 };
    lcdText(GRID_X + c * (CELL_W + CELL_GAP) + (CELL_W - 12) / 2, HDR_Y, s, 2, C_DIM, C_BG);
  }
  for (int r = 0; r < NUM_BANDS; r++) {
    char s[2] = { BAND_NAME[btnOrder[r]], 0 };
    lcdText(8, GRID_Y + r * (CELL_H + CELL_GAP) + (CELL_H - 24) / 2, s, 3, C_TEXT, C_BG);
  }
  if (hdzeroMode) lcdText(4, HDR_Y + 4, "HDZ", 1, C_HDZ, C_BG);   // mode indicator
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
  char s[20];
  if (mode == MODE_ACTIVITY || quietPage()) {
    uint32_t t = (uint32_t)actTrackedS;    // fixed width so no leftovers when it changes
    if (t < 3600) snprintf(s, sizeof(s), "History %2lum%02lus", (unsigned long)(t / 60), (unsigned long)(t % 60));
    else          snprintf(s, sizeof(s), "History %2luh%02lum", (unsigned long)(t / 3600), (unsigned long)(t / 60 % 60));
  }
  else snprintf(s, sizeof(s), "Span %d dB", spanDb);
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
void drawGridBox(int x, int y, RGB fill, bool highlight, bool hdzero) {
  int in = 0;
  if (highlight) { lcdFillC(x, y, CELL_W, CELL_H, C_PEAKH); in = 2; }
  if (hdzero)    { lcdFillC(x + in, y + in, CELL_W - 2 * in, CELL_H - 2 * in, C_HDZ); in += 2; }
  lcdFillC(x + in, y + in, CELL_W - 2 * in, CELL_H - 2 * in, fill);
}

// One answer box on the start-up question: border, label and key name.
static void drawAskBox(int x, int y, const char *label, const char *key, RGB edge, bool chosen) {
  const int w = 120, h = 70;
  RGB fill = chosen ? edge : RGB{ 20, 20, 20 };
  RGB text = chosen ? RGB{ 0, 0, 0 } : C_TEXT;
  lcdFillC(x, y, w, h, edge);
  lcdFillC(x + 3, y + 3, w - 6, h - 6, fill);
  int lw = 18 * (int)strlen(label);
  lcdText(x + (w - lw) / 2, y + 14, label, 3, text, fill);
  int kw = 6 * (int)strlen(key);
  lcdText(x + (w - kw) / 2, y + h - 18, key, 1, chosen ? text : C_DIM, fill);
}

// Span (grid brightness / spectrum height) is kept in flash so it survives
// power cycles. Only 15..40 dB in 5 dB steps is accepted; anything else
// (including nothing saved yet) falls back to 15 dB.
void loadSpan() {
  Preferences prefs;
  prefs.begin("fpvscan", false);
  int v = prefs.getInt("span", 15);
  prefs.end();
  spanDb = (v >= 15 && v <= 40 && v % 5 == 0) ? v : 15;
}

void saveSpan() {
  Preferences prefs;
  prefs.begin("fpvscan", false);
  if (prefs.getInt("span", -1) != spanDb) prefs.putInt("span", spanDb);
  prefs.end();
}

// Start-up question: "Enable HDZero specifics?" KEY_R = Yes, KEY_L = No.
// The last answer is saved in flash (Preferences); if nothing is pressed
// within HDZ_ASK_MS the saved answer is used (No the very first time). A
// press is saved only if it differs from what's stored, to spare the flash.
// Returns true for Yes. Waits for the button to be released so the press
// isn't also taken as a page change.
bool askHdzeroMode() {
  Preferences prefs;
  prefs.begin("fpvscan", false);
  bool saved = prefs.getBool("hdzero", false);

  const int bxL = 30, bxR = LCD_W - 30 - 120, by = 140;
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, "FPV Channel Scanner", 2, C_ACC, C_BG);
  const char *q = "Enable HDZero specifics?";
  lcdText((LCD_W - 12 * (int)strlen(q)) / 2, 90, q, 2, C_TEXT, C_BG);
  drawAskBox(bxL, by, "No",  saved ? "KEY_L" : "KEY_L - default", C_DIM, false);
  drawAskBox(bxR, by, "Yes", saved ? "KEY_R - default" : "KEY_R", C_HDZ, false);

  bool prevL = digitalRead(PIN_KEY_L) == LOW;   // a button already held
  bool prevR = digitalRead(PIN_KEY_R) == LOW;   // doesn't count until re-pressed
  int answer = -1, shown = -1;                  // -1 none, 0 No, 1 Yes
  uint32_t start = millis();
  while (answer < 0) {
    uint32_t elapsed = millis() - start;
    if (elapsed >= HDZ_ASK_MS) break;
    int secs = (int)((HDZ_ASK_MS - elapsed + 999) / 1000);   // 5, 4, 3, 2, 1
    if (secs != shown) {
      char s[32];
      snprintf(s, sizeof(s), saved ? "Starting HDZero in %d s" : "Starting normally in %d s", secs);
      lcdText((LCD_W - 12 * (int)strlen(s)) / 2, 240, s, 2, C_DIM, C_BG);
      shown = secs;
    }
    bool l = digitalRead(PIN_KEY_L) == LOW;
    bool r = digitalRead(PIN_KEY_R) == LOW;
    if (r && !prevR) answer = 1;
    else if (l && !prevL) answer = 0;
    prevL = l; prevR = r;
    delay(10);
  }
  bool yes = (answer < 0) ? saved : (answer == 1);
  if (answer >= 0 && yes != saved) prefs.putBool("hdzero", yes);   // remember it
  prefs.end();
  if (answer >= 0) {                            // show which one was picked
    if (yes) drawAskBox(bxR, by, "Yes", "KEY_R", C_HDZ, true);
    else     drawAskBox(bxL, by, "No",  "KEY_L", C_DIM, true);
    delay(400);
  }
  uint32_t t0 = millis();                       // wait (up to 3 s) for release
  while ((digitalRead(PIN_KEY_L) == LOW || digitalRead(PIN_KEY_R) == LOW) && millis() - t0 < 3000)
    delay(10);
  return yes;
}

// Page reached from page m by KEY_R (dir +1) or KEY_L (dir -1).
ScreenMode pageAfter(ScreenMode m, int dir) {
  if (dir > 0) {
    switch (m) {
      case MODE_GRID:     return MODE_SPECTRUM;
      case MODE_SPECTRUM: return MODE_ACTIVITY;
      case MODE_ACTIVITY: return MODE_QUIET;
      case MODE_QUIET:    return hdzeroMode ? MODE_HDZERO : MODE_GRID;
      default:            return MODE_GRID;       // from Quietest for HDZero
    }
  }
  switch (m) {
    case MODE_GRID:     return MODE_ACTIVITY;     // skips both quietest lists
    case MODE_SPECTRUM: return MODE_GRID;
    case MODE_ACTIVITY: return MODE_SPECTRUM;
    case MODE_HDZERO:   return MODE_QUIET;
    default:            return MODE_ACTIVITY;     // from Quietest
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
