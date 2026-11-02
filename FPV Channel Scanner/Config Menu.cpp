/*
 * FPV Channel Scanner - Config Menu.cpp
 * Revision: 56
 *
 * Configuration menu, opened by holding KEY_L + KEY_R for CONFIG_HOLD_MS on
 * any page.
 *
 * Menu: a Help page first (what each button does on each screen), then one
 * page per screen of the scanner, titled like that screen (FPV Channel
 * Activity, Spectrum, FPV Channel Noise, Quietest Channels, Quietest for
 * HDZero) plus General, listing its settings, then a last page to Save or
 * Discard. The page title is shown on a blue bar. All pages form one list
 * (on the Help page, KEY_R or ENTER goes to the first settings page and
 * KEY_L to "Save and restart" on the Save / Discard page):
 *   KEY_L / KEY_R  move up / down (moving past the end of a page goes to the
 *                  next or previous page)
 *   ENTER          on an On/Off setting: flip it
 *                  on a number: start editing it; then KEY_L / KEY_R step it
 *                  down / up (hold to repeat) and ENTER sets it
 *                  on Save / Discard / Defaults: do it
 * Save writes the settings to flash and restarts; Discard restarts without
 * saving. Defaults loads every default value (still needs Save).
 */

#include "Scanner.h"
#include <string.h>

#define MENU_ROW_Y0     62      // first list row
#define MENU_ROW_PITCH  30
#define MENU_HELP_Y     262     // one-line description of the selected item
#define MENU_HINT_Y     284     // button hints
static const RGB C_SEL = { 55, 40, 0 };   // selected row background

// Actions on the last page, after the settings
enum { ACT_SAVE = 0, ACT_DISCARD = 1, ACT_DEFAULTS = 2, NUM_ACTIONS = 3 };
static const char *ACTION_NAME[NUM_ACTIONS] = { "Save and restart", "Discard and restart", "Reset to defaults" };
static const char *ACTION_HELP[NUM_ACTIONS] = { "Write the settings to flash and restart",
                                                "Forget changes made here and restart",
                                                "Load every default value (then Save)" };
static const char *FINISH_PAGE = "Save / Discard";
static const char *HELP_PAGE   = "Help";

// ---------------------------------------------------------------- buttons
// Debounced press detection with auto-repeat while held.
struct MenuKey { uint8_t pin; bool down; uint32_t tChange, tNext; };

static bool menuKeyPressed(MenuKey &k, bool repeat) {
  bool d = digitalRead(k.pin) == LOW;
  uint32_t now = millis();
  if (d != k.down) {
    if (now - k.tChange < 30) return false;       // ignore contact bounce
    k.tChange = now; k.down = d;
    if (d) { k.tNext = now + 450; return true; }  // fresh press
    return false;
  }
  if (d && repeat && (int32_t)(now - k.tNext) >= 0) { k.tNext = now + 90; return true; }
  return false;
}

static void waitAllReleased() {
  uint32_t t0 = millis();
  while ((digitalRead(PIN_KEY_L) == LOW || digitalRead(PIN_KEY_R) == LOW ||
          digitalRead(PIN_BTN_ENTER) == LOW) && millis() - t0 < 3000)
    delay(10);
  delay(30);
}

// ---------------------------------------------------------------- drawing helpers
static void drawCentered(int y, const char *s, int scale, RGB fg) {
  int w = 6 * scale * (int)strlen(s);
  lcdText((LCD_W - w) / 2, y, s, scale, fg, C_BG);
}

// ---------------------------------------------------------------- menu drawing
// Menu items: 0 = the Help page, then every setting, then the actions.
static int  menuTotal()          { return 1 + NUM_SETTINGS + NUM_ACTIONS; }
static bool isHelp(int item)     { return item == 0; }
static bool isAction(int item)   { return item >= 1 + NUM_SETTINGS; }
static int  setIdx(int item)     { return item - 1; }                  // index into SETTINGS[]
static int  actIdx(int item)     { return item - 1 - NUM_SETTINGS; }   // ACT_SAVE...
static const char *itemPage(int item) {
  return isHelp(item) ? HELP_PAGE : isAction(item) ? FINISH_PAGE : SETTINGS[setIdx(item)].page;
}

// First item on the same page as item, and how many items that page has.
static void pageRange(int item, int &first, int &count) {
  const char *p = itemPage(item);
  first = item;
  while (first > 0 && strcmp(itemPage(first - 1), p) == 0) first--;
  count = 0;
  while (first + count < menuTotal() && strcmp(itemPage(first + count), p) == 0) count++;
}

static int pageNumber(int item, int &pages) {
  int num = 0; pages = 0;
  for (int i = 0; i < menuTotal(); i++) {
    if (i == 0 || strcmp(itemPage(i), itemPage(i - 1)) != 0) {
      pages++;
      if (i <= item) num = pages;
    }
  }
  return num;
}

static bool settingsChanged(const Settings &saved) {
  return memcmp(&saved, &cfg, sizeof(Settings)) != 0;
}

// Top line: "Configuration" with "unsaved" and the page counter on the right.
// Second line: the title of the screen being configured, on a blue bar.
static void drawMenuHeader(int item, const Settings &saved) {
  lcdFill(170, 0, LCD_W - 170, 24, 0, 0, 0);
  int pages, num = pageNumber(item, pages);
  char s[24];
  snprintf(s, sizeof(s), "%s%d/%d", settingsChanged(saved) ? "unsaved  " : "", num, pages);
  lcdText(LCD_W - 6 * (int)strlen(s) - 4, 8, s, 1, settingsChanged(saved) ? C_ACC : C_DIM, C_BG);
  lcdFillC(0, 26, LCD_W, 24, C_HDZ);
  lcdText(8, 30, itemPage(item), 2, RGB{ 255, 255, 255 }, C_HDZ);
}

static void drawMenuRow(int item, int row, bool selected, bool editing) {
  int y = MENU_ROW_Y0 + row * MENU_ROW_PITCH;
  RGB bg = selected ? C_SEL : C_BG;
  lcdFillC(4, y - 5, LCD_W - 8, MENU_ROW_PITCH - 4, bg);
  const char *name = isAction(item) ? ACTION_NAME[actIdx(item)] : SETTINGS[setIdx(item)].name;
  lcdText(10, y, name, 2, selected ? C_ACC : C_TEXT, bg);
  if (isAction(item)) return;
  char v[16];
  formatSetting(setIdx(item), v, sizeof(v));
  int w = 12 * (int)strlen(v);
  int x = LCD_W - 10 - w;
  if (editing) {                                   // value being edited: blue box
    lcdFillC(x - 4, y - 3, w + 8, 20, C_HDZ);
    lcdText(x, y, v, 2, RGB{ 0, 0, 0 }, C_HDZ);
  } else {
    lcdText(x, y, v, 2, selected ? C_TEXT : C_DIM, bg);
  }
}

static void drawMenuFooter(int item, bool editing) {
  lcdFill(0, MENU_HELP_Y - 2, LCD_W, LCD_H - MENU_HELP_Y + 2, 0, 0, 0);
  const char *help = isHelp(item)   ? "Settings follow on the next pages"
                   : isAction(item) ? ACTION_HELP[actIdx(item)] : SETTINGS[setIdx(item)].help;
  lcdText(4, MENU_HELP_Y, help, 1, C_DIM, C_BG);
  // Button hints, in three groups: "Left:" at the left edge, "ENTER:" against
  // the right edge and "Right:" centred between them. Left / Right say "Back" /
  // "Forward" when the selection is on the top / last line, since the next
  // press changes page.
  const char *l, *r, *e;
  if (editing)            { l = "-"; r = "+"; e = "Set"; }
  else if (isHelp(item))  { l = "Save Changes"; r = "Forward"; e = "Forward"; }
  else {
    int first, count;
    pageRange(item, first, count);
    l = item == first             ? "Back"    : "Up";
    r = item == first + count - 1 ? "Forward" : "Down";
    e = isAction(item) ? "Select" : "Modify Value";
  }
  char hl[24], hr[24], he[24];
  snprintf(hl, sizeof(hl), "Left: %s", l);
  snprintf(hr, sizeof(hr), "Right: %s", r);
  snprintf(he, sizeof(he), "ENTER: %s", e);
  int leftEnd = 4 + 6 * (int)strlen(hl);
  int xe = LCD_W - 4 - 6 * (int)strlen(he);
  int xr = (leftEnd + xe - 6 * (int)strlen(hr)) / 2;           // centred in the gap
  lcdText(4, MENU_HINT_Y, hl, 1, C_TEXT, C_BG);
  lcdText(xr, MENU_HINT_Y, hr, 1, C_TEXT, C_BG);
  lcdText(xe, MENU_HINT_Y, he, 1, C_TEXT, C_BG);
  lcdText(4, MENU_HINT_Y + 14, "Changes apply after Save and restart", 1, C_DIM, C_BG);
}

// Help page: what every button does, screen by screen (as in the .ino header).
// Headings in yellow; each line has the button(s) on the left, in white, and
// what they do on the right, in grey.
#define HELP_Y0     58
#define HELP_PITCH  11
#define HELP_COL2   118     // x of the description column (one space after 16 characters)

static void helpHeading(int &y, const char *s) {
  y += 4;
  lcdText(4, y, s, 1, C_ACC, C_BG);
  y += HELP_PITCH;
}
static void helpLine(int &y, const char *keys, const char *what) {
  lcdText(10, y, keys, 1, C_TEXT, C_BG);
  lcdText(HELP_COL2, y, what, 1, C_DIM, C_BG);
  y += HELP_PITCH;
}

static void drawHelpPage() {
  int y = HELP_Y0;
  char s[56];
  helpHeading(y, "All screens");
  helpLine(y, "Left / Right:",    "previous / next page");
  helpLine(y, "Hold Left+Right:", "5 s: open Configuration menu");
  helpLine(y, "Hold ENTER 0.7s:", "re-learn noise floor (VTXs off)");
  helpHeading(y, "ENTER (short press)");
  helpLine(y, "Activity:",        "cycle the span (dB), saved");
  helpLine(y, "Spectrum:",        "select the next band");
  helpLine(y, "Noise / Quietest:", "clear the noise history");
  helpHeading(y, "Page order");
  lcdText(10, y, cfg.hdzero ? "Right: Activity > Spectrum > Noise > Quietest > HDZ"
                            : "Right: Activity > Spectrum > Noise > Quietest",
          1, C_TEXT, C_BG);
  y += HELP_PITCH;
  lcdText(10, y, "Left:  goes back the same way", 1, C_TEXT, C_BG);
  y += HELP_PITCH;
  snprintf(s, sizeof(s), "Quietest opens after %d s of noise history", cfg.quietMinS);
  lcdText(10, y, s, 1, C_DIM, C_BG);
  y += HELP_PITCH;
  helpHeading(y, "Configuration");
  helpLine(y, "Left / Right:",    "up / down; - / + when editing");
  helpLine(y, "ENTER:",           "edit, set, on/off or select");
}

static void drawMenuPage(int item, bool editing, const Settings &saved) {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, "Configuration", 2, C_ACC, C_BG);
  drawMenuHeader(item, saved);
  if (isHelp(item)) { drawHelpPage(); drawMenuFooter(item, false); return; }
  int first, count;
  pageRange(item, first, count);
  for (int r = 0; r < count; r++) drawMenuRow(first + r, r, first + r == item, editing && first + r == item);
  drawMenuFooter(item, editing);
}

static void showMessageAndRestart(const char *line1) {
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  drawCentered(130, line1, 2, C_ACC);
  drawCentered(160, "Restarting...", 2, C_TEXT);
  Serial.printf("config: %s, restarting\n", line1);
  delay(900);
  ESP.restart();
}

// ---------------------------------------------------------------- menu loop
// Never returns: ends with Save or Discard, both of which restart.
void runConfigMenu() {
  if (digitalRead(PIN_KEY_L) == LOW || digitalRead(PIN_KEY_R) == LOW || digitalRead(PIN_BTN_ENTER) == LOW) {
    lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);    // opened by holding keys: wait for release
    drawCentered(140, "Configuration", 3, C_ACC);
    drawCentered(180, "Release the buttons", 2, C_TEXT);
    waitAllReleased();
  }
  const Settings saved = cfg;              // what's in flash, for "unsaved" and Discard
  // Start from the keys' current state, so a key still held isn't a press.
  MenuKey kL = { PIN_KEY_L,     digitalRead(PIN_KEY_L) == LOW,     millis(), 0 };
  MenuKey kR = { PIN_KEY_R,     digitalRead(PIN_KEY_R) == LOW,     millis(), 0 };
  MenuKey kE = { PIN_BTN_ENTER, digitalRead(PIN_BTN_ENTER) == LOW, millis(), 0 };
  int item = 0;                            // opens on the Help page
  bool editing = false;
  drawMenuPage(item, editing, saved);

  for (;;) {
    bool up   = menuKeyPressed(kL, true);
    bool down = menuKeyPressed(kR, true);
    bool enter = menuKeyPressed(kE, false);
    int first, count;
    pageRange(item, first, count);

    if (editing) {
      if (up || down) {
        stepSetting(setIdx(item), down ? +1 : -1);       // KEY_R = +, KEY_L = -
        drawMenuRow(item, item - first, true, true);
        drawMenuHeader(item, saved);
      }
      if (enter) {
        editing = false;
        drawMenuRow(item, item - first, true, false);
        drawMenuFooter(item, false);
      }
    } else if (up || down) {
      int next = (item + (down ? 1 : -1) + menuTotal()) % menuTotal();
      if (isHelp(item) && up) next = 1 + NUM_SETTINGS + ACT_SAVE;   // Help: KEY_L -> "Save and restart"
      if (strcmp(itemPage(next), itemPage(item)) != 0) {
        item = next;
        drawMenuPage(item, false, saved);         // moved onto another page
      } else {
        drawMenuRow(item, item - first, false, false);
        item = next;
        drawMenuRow(item, item - first, true, false);
        drawMenuFooter(item, false);
      }
    } else if (enter) {
      if (isHelp(item)) {
        item = 1;                                 // ENTER on Help: first settings page
        drawMenuPage(item, false, saved);
      } else if (isAction(item)) {
        int a = actIdx(item);
        if (a == ACT_SAVE)    { saveSettings(); showMessageAndRestart("Settings saved"); }
        if (a == ACT_DISCARD) { showMessageAndRestart("Changes discarded"); }
        if (a == ACT_DEFAULTS) {
          resetSettingsToDefaults();
          drawMenuPage(item, false, saved);
          lcdFill(0, MENU_HELP_Y - 2, LCD_W, 12, 0, 0, 0);
          lcdText(4, MENU_HELP_Y, "Defaults loaded - Save to keep them", 1, C_ACC, C_BG);
        }
      } else if (SETTINGS[setIdx(item)].isToggle) {
        int *v = SETTINGS[setIdx(item)].value;
        *v = *v ? 0 : 1;
        drawMenuRow(item, item - first, true, false);
        drawMenuHeader(item, saved);
      } else {
        editing = true;
        drawMenuRow(item, item - first, true, true);
        drawMenuFooter(item, true);
      }
    }
    delay(10);
  }
}
