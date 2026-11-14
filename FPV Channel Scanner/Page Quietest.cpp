/*
 * FPV Channel Scanner - Page Quietest.cpp
 * Revision: 103
 *
 * Quietest Channels and Quietest for HDZero pages: the QUIET_LIST_N
 * quietest channels from the Channel Noise history, in two columns of 8:
 * left column 1-8, right column 9-16. The HDZero page lists only channels
 * an HDZero VTX can use (hdzeroOk() in Settings.cpp). Measurement keeps running
 * on these pages, so the history and the lists stay live.
 *
 * IMD-Free Channels page: for 3, 4, 5 and 6 pilots, the set of channels with
 * the least intermodulation (IMD) interference, one column each, by band and
 * channel only. Two transmitters on f1 and f2 make interference at 2*f1 - f2
 * and 2*f2 - f1; a set is scored like ET's IMDTabler: every such product
 * within IMD_NEAR_MHZ of a channel in the set costs (35 - distance)^2, and
 * the score is 100 - total / 5 / channels (100 = clean). Channels in a set
 * are at least cfg.imdMinSep MHz apart. Among the best-scoring sets, the one
 * on the quietest channels (noise history) wins, so a busy channel is
 * avoided ("Quietest first"). Long press ENTER while the sets are held
 * switches to "Best IMD first": the best IMD score always wins and noise only
 * breaks ties; a chosen channel noisier than the Quietest-first pick for its
 * column is shaded toward red. In HDZero mode only channels an HDZero VTX can use are picked.
 */

#include "Scanner.h"

// True on either quietest-list page or the IMD-free page (all three use the
// noise history and are drawn from the grid sweep).
bool quietPage() {
  return mode == MODE_QUIET || mode == MODE_HDZERO || mode == MODE_IMD;
}

// ---------------------------------------------------------------- moved-channel fade
// A channel that has just changed position is drawn red, fading back to its
// normal color over MOVE_FADE_MS. moveMs = when it moved, 0 = not fading.
static RGB moveColor(uint32_t moveMs, RGB normal) {
  if (!moveMs) return normal;
  uint32_t age = millis() - moveMs;
  if (age >= MOVE_FADE_MS) return normal;
  float k = age / (float)MOVE_FADE_MS;                 // 0 = just moved (red) .. 1 = normal
  return RGB{ (uint8_t)lroundf(C_RED.r + (normal.r - C_RED.r) * k),
              (uint8_t)lroundf(C_RED.g + (normal.g - C_RED.g) * k),
              (uint8_t)lroundf(C_RED.b + (normal.b - C_RED.b) * k) };
}
static uint32_t nowNonZero() { uint32_t t = millis(); return t ? t : 1; }

// ---------------------------------------------------------------- IMD-free sets
static int      imdNCand = 0;
static int16_t  imdCandIdx[NUM_BANDS * NUM_CH];   // band*NUM_CH + channel, sorted by frequency
static uint16_t imdCandMhz[NUM_BANDS * NUM_CH];
static float    imdCandNoise[NUM_BANDS * NUM_CH]; // average dB above floor (0 without history)

static int16_t  imdSet[IMD_COLS][IMD_MAX_N];      // chosen candidates' idx, -1 = none
static int      imdScore[IMD_COLS];               // IMD score of each set, -999 = no set
static int16_t  imdShownSet[IMD_COLS][IMD_MAX_N];
static int      imdShownScore[IMD_COLS];
static uint32_t imdComputedMs = 0;
static uint32_t imdMoveMs[IMD_COLS][IMD_MAX_N];   // moved-channel fade start per cell
static bool     imdDirty = true;                  // recompute before the next draw
static uint8_t  imdTint[IMD_COLS][IMD_MAX_N];     // Best IMD first: how much noisier than the quietest pick (0-255)
static uint8_t  imdShownTint[IMD_COLS][IMD_MAX_N];
bool imdBestFirst = false;                        // long press ENTER while held: Best IMD first / Quietest first

// Candidate channels: one per frequency (F8 and R7 are both 5880 MHz, for
// example). The name used for a shared frequency follows R, F, E, A, B, L.
static void imdBuildCandidates(bool useNoise) {
  static const int PREF[NUM_BANDS] = { 4, 3, 2, 0, 1, 5 };   // R F E A B L
  imdNCand = 0;
  for (int p = 0; p < NUM_BANDS; p++) {
    int b = PREF[p];
    for (int c = 0; c < NUM_CH; c++) {
      if (chanNoData(b, c)) continue;                          // this board can't measure it
      if (sysOn()) { if (!sysOk(b, c)) continue; }      // video system: its channels only
      else if (BAND_NAME[b] == 'L' && !cfg.imdLowBand) continue;
      uint16_t f = sysOn() ? sysAt(b, c)->mhz : FPV_MHZ[b][c];   // the system's own frequency
      bool dup = false;
      for (int i = 0; i < imdNCand; i++) if (imdCandMhz[i] == f) { dup = true; break; }
      if (dup) continue;
      // insert sorted by frequency
      int j = imdNCand++;
      while (j > 0 && imdCandMhz[j - 1] > f) {
        imdCandMhz[j] = imdCandMhz[j - 1]; imdCandIdx[j] = imdCandIdx[j - 1]; imdCandNoise[j] = imdCandNoise[j - 1]; j--;
      }
      imdCandMhz[j] = f; imdCandIdx[j] = b * NUM_CH + c;
      float a = useNoise ? actAvg(b, c) : 0;
      imdCandNoise[j] = a > 0 ? a : 0;
    }
  }
}

// IMDTabler penalty total for k frequencies: every 2*f[r] - f[c] product
// (5100-6099 MHz) closer than IMD_NEAR_MHZ to a frequency in the set adds
// (IMD_NEAR_MHZ - distance)^2. Adding channels never lowers it.
static int imdPenalty(const uint16_t *f, int k) {
  int tot = 0;
  for (int r = 0; r < k; r++)
    for (int c = 0; c < k; c++) {
      if (r == c) continue;
      int p = 2 * (int)f[r] - (int)f[c];
      if (p < 5100 || p > 6099) continue;
      int d = 100000;
      for (int x = 0; x < k; x++) { int e = abs(p - (int)f[x]); if (e < d) d = e; }
      if (d < IMD_NEAR_MHZ) tot += (IMD_NEAR_MHZ - d) * (IMD_NEAR_MHZ - d);
    }
  return tot;
}

static int imdRatingFrom(int penalty, int n) { return 100 - penalty / 5 / n; }

// Depth-first search over candidates (in frequency order, at least
// cfg.imdMinSep apart). Ranking: IMD score, minus 2 points per dB of average
// noise on the chosen channels, plus a tiny bonus for a wider smallest gap.
// A branch is dropped as soon as it can't beat the best set found so far.
struct ImdSearch {
  int n;
  bool bestImd;                 // rank by IMD score first, noise only between equal scores
  int pick[IMD_MAX_N];
  uint16_t f[IMD_MAX_N];
  float bestValue;
  int bestPick[IMD_MAX_N];
  int bestRating;
  uint32_t nodes;
};

static void imdSearch(ImdSearch &S, int k, int start, float noise, int minGap, int lUsed) {
  if (++S.nodes % 512 == 0) pollInput();            // keep the buttons responsive
  if (k >= 3) {
    // Score, noise and smallest gap can only get worse as channels are
    // added, so this is the best any set on this branch can reach.
    int rating = imdRatingFrom(imdPenalty(S.f, k), S.n);
    float value = (S.bestImd ? rating * 1000.0f : (float)rating) - 2.0f * noise + minGap * 0.001f;
    if (value <= S.bestValue) return;
    if (k == S.n) {
      {
        S.bestValue = value; S.bestRating = rating;
        for (int i = 0; i < S.n; i++) S.bestPick[i] = S.pick[i];
      }
      return;
    }
  }
  for (int i = start; i <= imdNCand - (S.n - k); i++) {
    int gap = k ? imdCandMhz[i] - S.f[k - 1] : 100000;
    if (gap < cfg.imdMinSep) continue;
    bool isL = BAND_NAME[imdCandIdx[i] / NUM_CH] == 'L';
    if (isL && lUsed >= cfg.lBandLimit) continue;           // "L Band Limit" setting
    S.pick[k] = i; S.f[k] = imdCandMhz[i];
    imdSearch(S, k + 1, i + 1, noise + imdCandNoise[i], gap < minGap ? gap : minGap, lUsed + (isL ? 1 : 0));
  }
}

static void imdCompute() {
  bool useNoise = actLastMs != 0;
  imdBuildCandidates(useNoise);
  uint32_t t0 = millis(), nodes = 0;
  for (int col = 0; col < IMD_COLS; col++) {
    ImdSearch S;
    S.n = IMD_MIN_N + col; S.bestValue = -1e9f; S.bestRating = -999; S.nodes = 0; S.bestImd = imdBestFirst;
    if (S.n <= imdNCand) imdSearch(S, 0, 0, 0, 100000, 0);
    nodes += S.nodes;
    imdScore[col] = S.bestRating;
    for (int i = 0; i < IMD_MAX_N; i++) {
      imdSet[col][i] = (S.bestRating > -999 && i < S.n) ? imdCandIdx[S.bestPick[i]] : -1;
      imdTint[col][i] = 0;
    }
    // Best IMD first: the set Quietest first would pick for this column sets
    // the reference; a chosen channel noisier than its noisiest channel is
    // tinted toward red, full red at Busy level above it.
    if (imdBestFirst && useNoise && S.bestRating > -999) {
      ImdSearch Q;
      Q.n = S.n; Q.bestValue = -1e9f; Q.bestRating = -999; Q.nodes = 0; Q.bestImd = false;
      imdSearch(Q, 0, 0, 0, 100000, 0);
      nodes += Q.nodes;
      float ref = 0;
      for (int i = 0; i < Q.n && Q.bestRating > -999; i++) ref = max(ref, imdCandNoise[Q.bestPick[i]]);
      for (int i = 0; i < S.n; i++) {
        float k = (imdCandNoise[S.bestPick[i]] - ref) / max(1.0f, (float)cfg.busyDb);
        if (k > 0.02f) imdTint[col][i] = (uint8_t)lroundf(constrain(0.25f + 0.75f * k, 0.0f, 1.0f) * 255);
      }
    }
  }
  imdComputedMs = millis(); imdDirty = false;
  if (serialHasRoom(200))
    Serial.printf("imd: %d candidates, %lu nodes, %lu ms\n", imdNCand, (unsigned long)nodes,
                  (unsigned long)(imdComputedMs - t0));
}

static void imdInvalidate() {
  for (int col = 0; col < IMD_COLS; col++) {
    imdShownScore[col] = -1000;
    for (int i = 0; i < IMD_MAX_N; i++) { imdShownSet[col][i] = -2; imdMoveMs[col][i] = 0; imdShownTint[col][i] = 0; }
  }
  imdDirty = true;
}

// ENTER on the IMD-Free page: "hold" freezes the sets (no re-picking) until
// ENTER is pressed again; the hold is kept when another page is shown. While
// held, "Hold" flashes under the table.
bool imdHold = false;                // read by drawPageHints
static int  imdHoldShown = -1, imdHintShown = -1;   // what's on screen, -1 = redraw
bool imdHelpOpen = false;            // "no set" help box shown (ENTER: Help / Close)

// True when no column has a set (e.g. DJI or Walksnail stock channels, all
// closer together than "IMD min spacing"): ENTER then opens the help box
// instead of holding.
bool imdNoSets() {
  if (imdComputedMs == 0) return false;
  for (int col = 0; col < IMD_COLS; col++) if (imdScore[col] > -999) return false;
  return true;
}

static void imdDrawColumn(int col);
static void imdDrawHold();

// Takes effect at once, not at the end of the next grid sweep: on release
// the sets are re-picked from the noise history (which kept collecting
// while held) and redrawn right away; "Hold" and the hint update too.
void imdToggleHold() {
  imdHold = !imdHold;
  imdHoldShown = imdHintShown = -1;
  if (!imdHold) imdCompute();                        // released: re-pick now
  for (int col = 0; col < IMD_COLS; col++) imdDrawColumn(col);
  imdDrawHold();
}

// ENTER held while held: switch between Quietest first and Best IMD first
// and re-pick now from the history so far (still held).
void imdToggleBestFirst() {
  imdBestFirst = !imdBestFirst;
  imdCompute();
  for (int col = 0; col < IMD_COLS; col++)          // redraw in full: a change here isn't a "moved" channel
    for (int i = 0; i < IMD_MAX_N; i++) { imdShownSet[col][i] = -2; imdMoveMs[col][i] = 0; }
  imdHintShown = -1;
  for (int col = 0; col < IMD_COLS; col++) imdDrawColumn(col);
  imdDrawHold();
  quietFooterDirty = true;
  Serial.printf("imd: %s first\n", imdBestFirst ? "best IMD" : "quietest");
}

static void imdDrawHold() {
  int on = imdHold && (millis() / 500) % 2 == 0;     // flashes about once a second
  if (on != imdHoldShown) {
    lcdFill(0, IMD_HOLD_Y, LCD_W, 16, 0, 0, 0);
    if (on) lcdText((LCD_W - 4 * 12) / 2, IMD_HOLD_Y, "Hold", 2, C_RED, C_BG);    // red
    imdHoldShown = on;
  }
  const int hint = (int)imdHold | (int)imdNoSets() << 1 | (int)imdHelpOpen << 2;
  if (hint != imdHintShown) {                         // button hints: Hold / Release / Help / Close
    drawPageHints();

    imdHintShown = hint;
  }
}

// ---------------------------------------------------------------- "no set" help box
#define IMD_HELP_X 6
#define IMD_HELP_Y 28
#define IMD_HELP_W (LCD_W - 12)
#define IMD_HELP_H 134   // title, 4 lines and up to 3 fixes

// What to change in Configuration to get sets, for the settings in use.
static void imdDrawHelp() {
  const RGB bg = { 30, 30, 30 };
  lcdFillC(IMD_HELP_X, IMD_HELP_Y, IMD_HELP_W, IMD_HELP_H, C_ACC);          // border, as the pop-up
  lcdFill(IMD_HELP_X + 3, IMD_HELP_Y + 3, IMD_HELP_W - 6, IMD_HELP_H - 6, bg.r, bg.g, bg.b);
  int y = IMD_HELP_Y + 8;
  char t[64];
  auto ctr = [](const char *s, int size) { return (LCD_W - 6 * size * (int)strlen(s)) / 2; };   // centred x
  lcdText(ctr("No IMD-free set", 2), y, "No IMD-free set", 2, C_ACC, bg); y += 22;
  if (sysOn()) sysChannelsText(t, sizeof(t));
  else snprintf(t, sizeof(t), "Channels: all bands%s", cfg.imdLowBand ? "" : " except L");
  lcdText(ctr(t, 1), y, t, 1, C_TEXT, bg); y += 12;
  snprintf(t, sizeof(t), "A set needs 3+ channels %d MHz apart.", cfg.imdMinSep);
  lcdText(ctr(t, 1), y, t, 1, C_TEXT, bg); y += 18;
  snprintf(t, sizeof(t), "Configuration (hold Left+Right %d s):", (int)(CONFIG_HOLD_MS / 1000));
  lcdText(ctr(t, 1), y, t, 1, C_TEXT, bg); y += 14;
  auto fix = [&](const char *page, const char *what) {          // "Video System: FCC channels On"
    int x = (LCD_W - 6 * ((int)strlen(page) + 1 + (int)strlen(what))) / 2;
    lcdText(x, y, page, 1, C_ACC, bg);
    lcdText(x + 6 * ((int)strlen(page) + 1), y, what, 1, C_KEYACT, bg);
    y += 12;
  };
  if (cfg.vidSys == VS_DJI) {
    if (!cfg.fccChannels && cfg.djiModel != DJI_O4_RACE) fix("Video System:", "FCC channels On");
    if (cfg.djiModel == DJI_O4) fix("Video System:", "DJI model O4 race");
  } else if (cfg.vidSys == VS_WALKSNAIL) {
    if (!cfg.fccChannels && !cfg.wsRace) fix("Video System:", "FCC channels On");
    if (!cfg.wsRace) fix("Video System:", "WS race mode On");
  } else if (cfg.vidSys == VS_HDZERO) {
    if (!cfg.hdzLowBand) fix("Video System:", "HDZ low band On");
    if (!cfg.hdzE1F1) fix("Video System:", "HDZ E1 and F1 On");
  } else if (!sysOn() && !cfg.imdLowBand) {
    fix("Channel Selection:", "IMD use L band On");
  }
  snprintf(t, sizeof(t), "IMD min spacing lower");
  fix("Channel Selection:", t);
  y += 6;
  lcdText(ctr("then Save and restart.", 1), y, "then Save and restart.", 1, C_TEXT, bg);
}

static void imdDrawStatic();

// ENTER (short) on the IMD-Free page: close the help box; with no sets, open
// it; otherwise hold / release the sets.
void imdEnterShort() {
  if (imdHelpOpen) {
    imdDrawStatic();                                  // closes it: table, footer and hints back
    for (int col = 0; col < IMD_COLS; col++) imdDrawColumn(col);
    imdDrawHold();
  } else if (imdNoSets()) {
    imdHelpOpen = true;
    imdDrawHelp();
    imdDrawHold();                                    // hint: "ENTER: Close"
  } else {
    imdToggleHold();
  }
}

static void imdDrawStatic() {
  imdHelpOpen = false;
  imdHoldShown = imdHintShown = -1;
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  lcdText(4, 4, "IMD-Free Channels", 2, C_GOOD, C_BG);     // green title
  // video system tag right of the title (the grids' spot is taken by "3 pilots")
  if (sysOn()) lcdText(4 + 17 * 12 + 8, 8, sysTag(), 1, C_HDZ, C_BG);
  for (int col = 0; col < IMD_COLS; col++) {
    char t[12];
    snprintf(t, sizeof(t), "%d pilots", IMD_MIN_N + col);
    int x = col * IMD_COL_W;
    lcdText(x + (IMD_COL_W - 6 * (int)strlen(t)) / 2, IMD_HEAD_Y, t, 1, C_TEXT, C_BG);
    if (col) lcdFillC(x, IMD_HEAD_Y - 4, 1, IMD_SCORE_Y + 12 - (IMD_HEAD_Y - 4), C_MARK);   // divider
  }
  lcdFillC(0, IMD_Y0 - 5, LCD_W, 1, C_MARK);
  imdInvalidate();
  quietFooterDirty = true;
  learnShownPct = -1; scanAnimY = -1;
}

// Redraws the column dividers and every row and score, keeping the sets and
// the hold as they are. Used when the pop-up is taken down.
void imdRepaint() {
  for (int col = 1; col < IMD_COLS; col++)
    lcdFillC(col * IMD_COL_W, IMD_HEAD_Y - 4, 1, IMD_SCORE_Y + 12 - (IMD_HEAD_Y - 4), C_MARK);
  for (int col = 0; col < IMD_COLS; col++) {
    imdShownScore[col] = -1000;
    for (int i = 0; i < IMD_MAX_N; i++) { imdShownSet[col][i] = -2; imdMoveMs[col][i] = 0; }
    imdDrawColumn(col);
  }
}

// Redraw one column if its set or score changed, or a moved channel in it is
// still fading. A row whose channel changed (not the first fill) starts red.
// Yellow, shaded toward red by a Best IMD first tint (0-255).
static RGB imdTintColor(uint8_t t) {
  float k = t / 255.0f;
  return RGB{ (uint8_t)lroundf(C_ACC.r + (C_RED.r - C_ACC.r) * k),
              (uint8_t)lroundf(C_ACC.g + (C_RED.g - C_ACC.g) * k),
              (uint8_t)lroundf(C_ACC.b + (C_RED.b - C_ACC.b) * k) };
}

static void imdDrawColumn(int col) {
  bool same = imdScore[col] == imdShownScore[col], fading = false;
  for (int i = 0; i < IMD_MAX_N; i++) {
    int now = imdSet[col][i], was = imdShownSet[col][i];
    if (imdTint[col][i] != imdShownTint[col][i]) same = false;
    if (now != was) {
      same = false;
      if (now >= 0 && was >= 0) imdMoveMs[col][i] = nowNonZero();   // moved
      else imdMoveMs[col][i] = 0;
    }
    if (imdMoveMs[col][i]) {
      fading = true;                                  // redraw until the fade ends
      if (millis() - imdMoveMs[col][i] >= MOVE_FADE_MS) imdMoveMs[col][i] = 0;
    }
  }
  if (same && !fading) return;
  int x = col * IMD_COL_W + 1;
  for (int i = 0; i < IMD_MAX_N; i++) {
    int idx = imdSet[col][i];
    int y = IMD_Y0 + i * IMD_PITCH;
    if (idx != imdShownSet[col][i] || imdMoveMs[col][i] || fading || imdTint[col][i] != imdShownTint[col][i])
      lcdFill(x, y, IMD_COL_W - 1, 24, 0, 0, 0);
    imdShownSet[col][i] = idx; imdShownTint[col][i] = imdTint[col][i];
    if (idx < 0) continue;
    char t[8];
    const SysChan *sc = sysOn() ? sysAt(idx / NUM_CH, idx % NUM_CH) : nullptr;
    if (sc) snprintf(t, sizeof(t), "%s", sc->name);       // system channel name ("CH3", "149")
    else    snprintf(t, sizeof(t), "%c%d", BAND_NAME[idx / NUM_CH], idx % NUM_CH + 1);
    lcdText(x + (IMD_COL_W - 1 - 18 * (int)strlen(t)) / 2, y, t, 3, moveColor(imdMoveMs[col][i], imdTintColor(imdTint[col][i])), C_BG);
  }
  if (imdScore[col] == imdShownScore[col]) return;
  lcdFill(x, IMD_SCORE_Y, IMD_COL_W - 1, 8, 0, 0, 0);
  imdShownScore[col] = imdScore[col];
  char t[20];
  if (imdScore[col] <= -999) snprintf(t, sizeof(t), "no set");
  else                       snprintf(t, sizeof(t), "IMD %d", imdScore[col]);
  RGB c = imdScore[col] >= 100 ? C_GOOD : imdScore[col] >= 80 ? C_ACC : C_RED;
  lcdText(x + (IMD_COL_W - 1 - 6 * (int)strlen(t)) / 2, IMD_SCORE_Y, t, 1, c, C_BG);
}

static void imdDraw(bool learning, uint32_t learnStart) {
  static bool lastLearning = false, lastNoise = false;
  bool noise = actLastMs != 0;
  // Re-pick unless held (but always once, so there is something to show).
  if (imdComputedMs == 0 ||
      (!imdHold && (imdDirty || noise != lastNoise || millis() - imdComputedMs >= IMD_REFRESH_MS))) {
    imdCompute();
    if (noise != lastNoise) quietFooterDirty = true;
    lastNoise = noise;
  }
  if (!imdHelpOpen) {                                 // the help box covers the table
    for (int col = 0; col < IMD_COLS; col++) imdDrawColumn(col);
  }
  imdDrawHold();

  if (learning) {
    drawLearnProgress(FOOT_Y, (gridSweeps - learnStart) * 100 / GRID_LEARN_SWEEPS);
  } else if (lastLearning) {
    quietFooterDirty = true;
  }
  lastLearning = learning;
  if (!learning && quietFooterDirty) {
    learnShownPct = -1; scanAnimY = -1;
    lcdFill(0, FOOT_Y, LCD_W, LEGEND_Y - FOOT_Y, 0, 0, 0);
    char t[64];
    if (sysOn()) sysChannelsText(t, sizeof(t));             // "HDZero only: R1-8, ..."
    else snprintf(t, sizeof(t), "Min spacing %d MHz%s", cfg.imdMinSep, cfg.imdLowBand ? ", L band on" : "");
    lcdText(4, FOOT_Y + 6, t, 1, C_DIM, C_BG);
    lcdText(4, FOOT_Y + 20, !noise ? "IMD only (no history)" : imdBestFirst ? "Best IMD first" : "Quietest sets first", 1, C_DIM, C_BG);
    drawSpanLabel();
    quietFooterDirty = false;
  }
}

// ---------------------------------------------------------------- quietest lists
static uint32_t quietMoveMs[QUIET_LIST_N];   // moved-channel fade start per slot

void quietInvalidate() {
  imdInvalidate();
  for (int i = 0; i < QUIET_LIST_N; i++) quietMoveMs[i] = 0;
  for (int i = 0; i < QUIET_LIST_N; i++) { quietShownIdx[i] = -2; quietShownTenths[i] = -1; quietShownBusy[i] = -1; }
}

void quietDrawStatic() {
  if (mode == MODE_IMD) { imdDrawStatic(); return; }
  lcdFill(0, 0, LCD_W, LCD_H, 0, 0, 0);
  drawPageHints();                                   // button hints, bottom line
  { char t[32];
    if (mode == MODE_HDZERO) snprintf(t, sizeof(t), "Quietest for %s", sysName());
    else snprintf(t, sizeof(t), "Quietest Channels");
    lcdText(4, 4, t, 2, C_GOOD, C_BG); }                   // green title
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
  // On the video system's page: the system's channel name and frequency
  const SysChan *sc = mode == MODE_HDZERO ? sysAt(b, c) : nullptr;
  if (sc) snprintf(t, sizeof(t), "%s", sc->name);
  else    snprintf(t, sizeof(t), "%c%d", BAND_NAME[b], c + 1);
  lcdText(x + 30, y, t, 2, moveColor(quietMoveMs[slot], C_ACC), C_BG);   // band / channel (red when just moved)
  int fx = max(x + 62, x + 30 + 12 * (int)strlen(t) + 6);   // after a 3-letter name
  snprintf(t, sizeof(t), "%u", sc ? sc->mhz : FPV_MHZ[b][c]);
  lcdText(fx, y, t, 2, C_TEXT, C_BG);                      // frequency
  snprintf(t, sizeof(t), "%.1f dB, %d%% busy", actAvg(b, c), actBusyPct(b, c));
  lcdText(x + 30, y + 18, t, 1, C_DIM, C_BG);
}

void quietDraw(bool learning, uint32_t learnStart) {
  if (mode == MODE_IMD) { imdDraw(learning, learnStart); return; }
  static bool lastLearning = false;
  bool ready = !learning && actLastMs != 0;
  int order[NUM_BANDS * NUM_CH];
  actRankOrder(order);
  // On the video system's page, keep only the channels standing in for its channels.
  // Both lists hold at most cfg.lBandLimit L-band channels ("L Band Limit").
  int list[NUM_BANDS * NUM_CH], nList = 0, nL = 0;
  for (int i = 0; i < NUM_BANDS * NUM_CH; i++) {
    int k = order[i];
    if (mode == MODE_HDZERO && !sysOk(k / NUM_CH, k % NUM_CH)) continue;
    if (chanNoData(k / NUM_CH, k % NUM_CH)) continue;          // this board can't measure it
    if (BAND_NAME[k / NUM_CH] == 'L') { if (nL >= cfg.lBandLimit) continue; nL++; }
    list[nList++] = k;
  }
  for (int i = 0; i < QUIET_LIST_N; i++) {
    int idx = (ready && i < nList) ? list[i] : -1;
    int tenths = -1, busy = -1;
    if (idx >= 0) {
      tenths = (int)lroundf(actAvg(idx / NUM_CH, idx % NUM_CH) * 10);
      busy = actBusyPct(idx / NUM_CH, idx % NUM_CH);
    }
    // A different channel in this slot (not the first fill) = it moved.
    if (idx != quietShownIdx[i] && idx >= 0 && quietShownIdx[i] >= 0) quietMoveMs[i] = nowNonZero();
    bool fading = quietMoveMs[i] != 0;
    if (fading && millis() - quietMoveMs[i] >= MOVE_FADE_MS) quietMoveMs[i] = 0;   // last, normal-colour redraw
    if (fading || idx != quietShownIdx[i] || tenths != quietShownTenths[i] || busy != quietShownBusy[i]) {
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
    lcdFill(0, FOOT_Y, LCD_W, LEGEND_Y - FOOT_Y, 0, 0, 0);
    if (ready && mode == MODE_HDZERO)
      { char t[64]; sysChannelsText(t, sizeof(t)); lcdText(4, FOOT_Y + 6, t, 1, C_DIM, C_BG); }
    if (ready) lcdText(4, FOOT_Y + 20, "1 = quietest", 1, C_DIM, C_BG);
    else       drawScanning(FOOT_Y + 2);
    drawSpanLabel();
    quietFooterDirty = false;
  }
}
