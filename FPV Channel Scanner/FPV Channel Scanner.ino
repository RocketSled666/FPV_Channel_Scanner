/*
 * FPV Channel Scanner
 * Revision: 95
 * Board : Waveshare ESP32-C5-LCD-2.73 (ESP32-C5, ILI9488 320x320 SPI LCD)
 * Core  : Arduino-ESP32 3.3.x  (Tools > Board > "ESP32C5 Dev Module")
 *
 * Uses the ESP32-C5's own 5 GHz Wi-Fi radio as an energy detector:
 *   1. Wi-Fi is started only to power up and calibrate the 5 GHz RF path.
 *   2. The RF synthesizer is tuned directly to a frequency in MHz with the PHY
 *      library's internal phy_set_chanfreq() (the same call the Wi-Fi driver
 *      uses; values > 14 are taken as MHz).
 *   3. The baseband's hardware noise-floor estimator measures the in-band
 *      power (20 MHz bandwidth) - no Wi-Fi packets are needed, so analog FPV
 *      video carriers show up.
 *
 * phy_set_chanfreq() and the baseband registers are UNDOCUMENTED and were
 * found by disassembling Espressif's libphy.a for the ESP32-C5. They may
 * change between core versions. Readings are relative, not calibrated dBm.
 *
 * Six pages, plus a seventh in HDZero mode, in KEY_R order:
 *   FPV CHANNEL ACTIVITY (grid) - all 48 FPV channels, square brightness =
 *               power above floor right now
 *   SPECTRUM  - continuous sweep across one band in 4 MHz steps, with the
 *               point nearest each channel moved onto its exact frequency; under
 *               the graph a 31-pixel waterfall shows the last 31 sweeps
 *               (newest on top), dark blue = weak, bright yellow = strong
 *   BAND SCAN - the whole band in one sweep, like the ArduVidRx band
 *               scanner: every channel frequency plus fill-in points, as
 *               bars in 10 MHz bins; peaks labelled with their MHz, the
 *               strongest bar green and named in the footer
 *   FPV CHANNEL NOISE - same layout as the grid; each box = its signal above
 *               the floor averaged over all time since start or the last
 *               reset, shaded from the quietest channel (black) to the
 *               busiest (bright red); the quietest is outlined and named
 *   QUIETEST CHANNELS - the 16 quietest channels from the Channel Noise
 *               history, in order (1 = quietest), with average dB and busy %.
 *               Reached only by KEY_R from Channel Noise, once there is at
 *               least 60 s of history; before that a "Wait NNs" pop-up
 *               (the seconds still to go)
 *               shows and the page doesn't change. Not shown in HDZero
 *               mode (Quietest for HDZero takes its place).
 *   QUIETEST FOR HDZERO - the same list, limited to channels an HDZero VTX
 *               can use: R1-R8, E1, F1, F2, F4 and L1-L8 (low band unlocked).
 *               Only in HDZero mode, where it replaces Quietest Channels:
 *               reached by KEY_R from Channel Noise (same 60 s wait).
 *   IMD-FREE CHANNELS - for 3, 4, 5 and 6 pilots, the channel set (band /
 *               channel only) with the least intermodulation interference:
 *               two VTXs on f1, f2 make interference at 2*f1-f2 and 2*f2-f1,
 *               scored like ET's IMDTabler (100 = clean). Channels are at
 *               least "Min spacing" apart; among the best sets the quietest
 *               (noise history) wins. HDZero mode: HDZero channels only,
 *               and "HDZero only" in the footer. Reached by KEY_R from
 *               Quietest for HDZero (HDZero mode) or Quietest Channels.
 *
 * Start-up: a splash screen (Splash.PNG) shows for 5 s with a blue hint,
 *   "Press forward+backward buttons now for Settings screen, or hold for
 *   >5s at any time"; the radio
 *   starts underneath it. Pressing KEY_L + KEY_R together during the splash
 *   opens the Configuration menu; otherwise the Activity grid follows.
 *
 * Configuration: hold KEY_L + KEY_R for 5 s on any page (or press both
 *   during the splash) to open the Configuration menu. It opens on a Help
 *   page listing what each button does on each screen, then lists every
 *   setting, one page per screen,
 *   titled like that screen on a blue bar (FPV Channel Activity, Spectrum,
 *   FPV Channel Noise, Quietest Channels, Quietest for HDZero) plus General,
 *   then a Save / Discard page; both restart the device. In the
 *   menu: KEY_L / KEY_R move up / down, ENTER flips an On/Off setting or
 *   edits a number (then KEY_L / KEY_R step it, hold to repeat, ENTER sets
 *   it); holding ENTER jumps to the Save / Discard page. A User Interface
 *   page holds the timings (long press, Configuration hold, splash, pop-up,
 *   scanning dots, IMD refresh, moved-channel fade). All settings are kept
 *   in flash. See Settings.cpp for the list.
 *
 *   HDZero mode (the "HDZero features" setting) adds the Quietest for
 *   HDZero page, draws only HDZero-usable channels on both grid pages
 *   (the rest are left blank), shows a blue "HDZ" marker at the top left
 *   of the grids, and limits the IMD-Free page
 *   to HDZero channels. In HDZero mode, Quietest for HDZero replaces
 *   Quietest Channels (the normal list isn't shown).
 *
 *   KEY_R: Activity -> Spectrum -> Band Scan -> Noise -> Quietest -> IMD-Free
 *          -> Activity (HDZero mode: ... Noise -> Quietest for HDZero -> IMD-Free ...)
 *   KEY_L: Activity -> Noise -> Band Scan -> Spectrum -> Activity;
 *          IMD-Free -> Quietest (or Quietest for HDZero) -> Noise
 *
 * Buttons:
 *   KEY_L / KEY_R (GPIO2 / GPIO3) = previous / next page (order above),
 *                                   on release
 *   KEY_L + KEY_R held 5 s    = open the Configuration menu from any page
 *   ENTER (short) on GRID     = cycle the span (dB); saved across power cycles
 *   ENTER (short) on NOISE    = clear the noise history
 *   ENTER (short) on IMD-FREE = hold / release the channel sets (no
 *                             re-picking while held; "Hold" flashes)
 *   ENTER (short) on either QUIETEST page does nothing
 *   ENTER (short) on SPECTRUM = select the next band button above the graph
 *                             (HDZero mode: bands HDZero can't use are skipped)
 *   ENTER (short) on BAND SCAN = pause / resume the scan
 *   ENTER (hold 0.7 s)        = re-learn the noise floor (do it with VTXs off)
 *
 * Arduino IDE note: only setup() and loop() live in this .ino, so the IDE's
 * auto-generated prototypes have nothing to trip over. Types are in Config.h,
 * every other function is declared in Scanner.h, and none uses default
 * arguments.
 *
 * Source files:
 *   Config.h        pins, constants, types, frequency table, colors
 *   Scanner.h       shared declarations (globals + every function)
 *   State.cpp       global state
 *   Display.cpp     LCD driver, text, IO expander
 *   Input.cpp       buttons
 *   Radio.cpp       tuning, measurement, stall handling, timing
 *   Bands.cpp       band display order
 *   Settings.cpp    settings: list, limits, defaults, flash load / save
 *   Config Menu.cpp Configuration menu
 *   Splash.cpp      start-up splash screen
 *   Splash Image.h  splash picture data, made from Splash.PNG by
 *                   tools/Make Splash Header.py
 *   Screen Common.cpp  shared drawing, pop-up, page navigation
 *   Page Activity.cpp / Page Spectrum.cpp / Page Band Scan.cpp /
 *   Page Noise.cpp / Page Quietest.cpp
 *   font5x7.h       5x7 bitmap font
 *
 * Revision history: see CHANGELOG.md
 *   36  Blue outline on HDZero-usable channels on both grid pages
 *   37  HDZero features only when KEY_L is held at power-on (HDZero mode)
 *   38  HDZero mode chosen from a start-up question with a 5 s countdown
 *   39  Start-up answer saved in flash and used as the default next time
 *   40  Span saved in flash and restored at power-on; HDZero outlines
 *       confirmed to depend only on HDZero mode
 *   41  Pop-up text "Wait longer" changed to "Wait >60s"
 *   42  Spectrum measures every channel at its exact frequency (nearest
 *       sweep point snapped onto it); no extra points, same sweep speed
 *   43  Configuration menu at start-up replaces the HDZero question; all
 *       tunable values are settings kept in flash
 *   44  Holding KEY_L + KEY_R for 5 s opens the Configuration menu from any
 *       page; page keys now act on release so the hold doesn't change page
 *   45  Start-up Configuration prompt removed (two-key hold only); menu
 *       pages titled like the screens they configure, on a blue bar
 *   46  5 s start-up splash screen from Splash.PNG with a blue hint; both
 *       page keys pressed during it open the Configuration menu
 *   47  Splash hint adds "or hold for >5s at any time"; Configuration menu
 *       opens on a Help page listing the buttons for each screen
 *   48  Menu hint line: "Prev Page" / "Next Page" on a page's top / last
 *       line, "Modify Value" on settings; Help page KEY_L = "Save Changes"
 *   49  Hints say "Page Back" / "Page Fwd" instead of "Prev Page" / "Next Page"
 *   50  Menu and Help page name the keys "Left:", "Right:" and "ENTER:"
 *   51  Hints say "Back" / "Forward" instead of "Page Back" / "Page Fwd"
 *   52  Spectrum graph 14 px shorter; 10 px waterfall of the last 10 sweeps
 *       under it (dark blue = weak, bright yellow = strong)
 *   53  Band buttons 5 px lower, 15 px round circles; waterfall 15 px
 *   54  Band circles moved to the top, under the title; graph, waterfall
 *       and channel labels moved down; waterfall 25 px
 *   55  White box around the waterfall, 31 px; band circles 19 px, selected
 *       band green, others greyed out
 *   56  Band buttons square again, spread to line up with the graph box
 *       edges (6 x 43 px, 6 px gaps)
 *   57  IMD-Free Channels page after Quietest Channels: best channel sets
 *       for 3-6 pilots; settings "Min spacing" and "Use L band"
 *   58  Help page: each button's action shown in green
 *   59  Configuration hint line: each key's action shown in green
 *   60  History clock as D:HH:MM:SS; on the Quietest and IMD-Free pages a
 *       channel that changes position shows red, fading back over 3 s
 *   61  Green page titles on FPV Channel Activity, Quietest Channels,
 *       IMD-Free Channels and Quietest for HDZero
 *   62  ENTER (short) does nothing on the Quietest and IMD-Free pages
 *   63  Quietest for HDZero now comes before IMD-Free; IMD-Free footer says
 *       "HDZero only" in HDZero mode
 *   64  HDZero mode shows only Quietest for HDZero, not Quietest Channels
 *   65  Save / Discard page shows the revision (FW_REVISION in Config.h)
 *       and the build date and time
 *   66  Build line moved to the bottom of the Save / Discard page, darker
 *       grey, two blank lines below the footer
 *   67  User Interface Configuration page for the UI timings (were compile-
 *       time constants); long ENTER press in Configuration = Save / Discard
 *   68  New splash picture (Splash.PNG replaced, Splash Image.h regenerated)
 *   69  The Quietest pop-up shows the time still to wait ("Wait 23s")
 *   70  "L Band Limit" setting: most L-band channels in the Quietest lists
 *       and in each IMD-free set
 *   71  L Band Limit range 1-8 (0 removed: "Use L band" / "Low band L1-L8"
 *       already turn L band off)
 *   72  HDZero mode: the Activity and Noise grids leave channels HDZero can't
 *       tune blank (no box, no letter for an empty band); Noise shading,
 *       ranks and quietest use only the drawn channels
 *   73  Blue HDZero outlines removed from the grid boxes
 *   74  HDZero mode: spectrum band buttons without an HDZero channel are
 *       darker grey and skipped by ENTER
 *   75  ENTER on IMD-Free holds the sets (flashing "Hold"); ENTER again or
 *       another page releases them
 *   76  "Hold" in red; "Hold" and the ENTER hint moved down 6 / 4 px
 *   77  Releasing the IMD hold re-picks and redraws the sets at once (not
 *       after the next grid sweep); hold / release indicators also at once
 *   78  Waterfall colours on a log curve, set by "Waterfall log" (Spectrum
 *       settings; 0 = linear, default 4) so weak signals are brighter
 *   79  Waterfall floor colour darker blue; Waterfall log default 8
 *   80  A stalled measurement now holds that frequency's last good reading
 *       (kept out of floors and noise history) instead of the RSSI register;
 *       "Meas timeout" setting (default 8 ms, was a fixed 5 ms)
 *   81  Calibrated RSSI stands in for stalled readings (offset learned from
 *       good readings, garbage RSSI ignored); channels with no live data
 *       show "--" and are left out of the Noise ranking, Quietest and IMD
 *   82  phy_get_rssi() stand-in removed (it freezes between Wi-Fi packets);
 *       optional "Sig RSSI fill" (libphy signal-RSSI mode) and "802.11p
 *       mode" settings; "no data" with hysteresis (on > 60%, off < 30%)
 *   83  Measurement timeout fixed at 12 ms while 802.11p mode is on (the
 *       saved "Meas timeout" value returns when it is off); Sig RSSI fill
 *       help now says to leave it Off
 *   84  "Sig RSSI fill" replaced by "RSSI only": measure every reading with
 *       libphy signal RSSI (median of 5 reads) instead of the noise-floor
 *       estimator; "rssi only" serial statistics
 *   85  RSSI only On by default; Samples/channel default 3, set to 1 / 3
 *       when RSSI only is toggled Off / On; samples help shows ms per reading
 *   86  Spectrum graph auto range (fits the highest peak, 10 dB steps, shrinks
 *       after 5 s); waterfall stays on the span
 *   87  "Max MHz" setting: frequencies above it aren't measured (grids "--",
 *       spectrum greyed), for boards that read junk at the top of the band
 *   88  Fixes: IMD-Free table repainted after the Wait pop-up; floor clamp
 *       uses learned floors only; no overflow with no live channel; LCD
 *       driven through ESP-IDF's esp_lcd panel IO with a software reset,
 *       so the screen comes back after a flash or USB reset
 *   89  "Tuning" setting (Direct / Wi-Fi / Wi-Fi+chan: libphy
 *       phy_change_channel path) and "RX filter" width setting (Auto, 11-23 MHz)
 *   90  Busy level default 6 dB; grid highlight outline amber like the footer
 *   91  Defaults: Tuning Wi-Fi, RX filter 16 MHz
 *   92  Config pages grouped by job (Radio, Scanning, Channel Selection,
 *       HDZero, Display, Controls); red "Configuration" title
 *   93  Config page titles end in "Page"
 *   94  Spectrum height = span again; auto range is a setting (default Off)
 *   95  Band Scan page after Spectrum: whole-band bar graph like the
 *       ArduVidRx band scanner, peak labels, ENTER pauses / resumes,
 *       axis ends at "Max MHz"
 */

#include "Scanner.h"

void setup() {
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  // Native USB serial: never wait on the PC. Without this, an open-but-unread
  // port (e.g. right after an IDE upload) stalls each print for up to 2 s.
  Serial.setTxBufferSize(4096);   // must be set before begin()
  Serial.setTxTimeoutMs(0);
#endif
  Serial.begin(115200);
  buildButtonOrder();          // band order for grid rows and spectrum buttons
  loadSettings();              // all settings saved in flash (defaults if none)
  delay(200);
  Serial.println("\nFPV Channel Scanner (ESP32-C5)");

  pinMode(PIN_SD_CS,  OUTPUT); digitalWrite(PIN_SD_CS,  HIGH);
  pinMode(PIN_KEY_L,     INPUT_PULLUP);
  pinMode(PIN_KEY_R,     INPUT_PULLUP);
  pinMode(PIN_BTN_ENTER, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

  lcdInit();
  showSplash();                               // picture + "Press forward+backward..." hint
  exioBacklight(cfg.backlight);               // backlight on once the splash is drawn
  uint32_t splashStart = millis();
  Serial.print("Settings:");                  // everything from the Configuration menu
  for (int i = 0; i < NUM_SETTINGS; i++) {
    char v[16];
    formatSetting(i, v, sizeof(v));
    Serial.printf(" %s=%s", SETTINGS[i].key, v);
  }
  Serial.println();
  radioInit();                                // radio starts while the splash shows
  gridResetFloor();
  for (int b = 0; b < NUM_BANDS; b++) specFloorValid[b] = false;
  specSetBand(specBand);

  uint32_t t0 = micros();
  float t = readFreqDb(5800, GRID_SAMPLES, SETTLE_US);
  Serial.printf("Self-test 5800 MHz: %.1f dB, %lu us/channel, timeouts=%lu\n",
                t, (unsigned long)(micros() - t0), (unsigned long)g_nfTimeouts);
  bootTimingTest();
  g_tTuneUs = g_tMeasUs = g_tDrawUs = 0; g_nTunes = g_nMeas = 0;

  // Splash stays up for SPLASH_MS in total; KEY_L + KEY_R pressed together
  // during it opens the Configuration (settings) menu, which ends in a restart.
  if (splashWait(splashStart)) runConfigMenu();
  gridDrawStatic();
  gridInvalidate();
}

void loop() {
  if (mode == MODE_SPECTRUM) specSweep();
  else if (mode == MODE_BANDSCAN) bscanSweep();
  else gridSweep();
  pollInput();

  if (evConfig) {                           // KEY_L + KEY_R held: Configuration menu
    evConfig = false;
    Serial.println("config: KEY_L + KEY_R held, opening Configuration menu");
    runConfigMenu();                        // ends in a restart
  }
  if (evToggle) {
    evToggle = false; evEnterShort = evEnterLong = false;
    ScreenMode next = pageAfter(mode, pageDir);
    if ((next == MODE_QUIET || next == MODE_HDZERO) && actTrackedS < QUIET_MIN_HISTORY_S) {
      char msg[16];                                  // not enough history yet:
      int left = (int)ceilf(QUIET_MIN_HISTORY_S - actTrackedS);   // seconds still to wait
      if (left < 1) left = 1;
      snprintf(msg, sizeof(msg), "Wait %ds", left);
      showPopup(msg);
      Serial.printf("quietest: %lu of %d s of history, wait longer\n",
                    (unsigned long)actTrackedS, QUIET_MIN_HISTORY_S);
    } else {
      enterMode(next);
      Serial.printf("page -> %s\n", mode == MODE_GRID ? "channel activity" : mode == MODE_ACTIVITY ? "channel noise" :
                                mode == MODE_QUIET ? "quietest channels" :
                                mode == MODE_IMD ? "IMD-free channels" :
                                mode == MODE_HDZERO ? "quietest for HDZero" :
                                mode == MODE_BANDSCAN ? "band scan" : "spectrum");
    }
  }
  if (popupUntilMs && (int32_t)(millis() - popupUntilMs) >= 0) {
    popupUntilMs = 0;
    lcdFill(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, 0, 0, 0);   // clear it, gaps included
    if (mode == MODE_IMD) imdRepaint();      // redraw what the pop-up covered
    else actInvalidate();
  }
  if (evEnterLong) {                        // hold: re-learn the noise floor
    evEnterLong = false;
    if (mode == MODE_GRID) { gridResetFloor(); gridInvalidate(); gridFooterDirty = true; drawTitleStatus(0, true); }
    else if (mode == MODE_ACTIVITY) { gridResetFloor(); actInvalidate(); actFooterDirty = true; drawTitleStatus(0, true); }
    else if (quietPage()) { gridResetFloor(); quietInvalidate(); quietFooterDirty = true; drawTitleStatus(0, true); }
    else if (mode == MODE_BANDSCAN) bscanRelearn();
    else { specFloorValid[specBand] = false; specNoSeedOnce = true; specSetBand(specBand); specDrawStatic(); }
  }
  if (evEnterShort) {
    evEnterShort = false;
    if (mode == MODE_GRID) {                // cycle brightness span
      cfg.span = (cfg.span >= 40) ? 15 : cfg.span + 5;
      saveSpan();                           // remembered across power cycles
      gridInvalidate(); gridFooterDirty = true;
    } else if (mode == MODE_ACTIVITY) {     // restart the activity sums
      actClear(); actInvalidate(); actFooterDirty = true;
    } else if (quietPage()) {               // IMD-Free: hold / release; Quietest pages: nothing
      if (mode == MODE_IMD) imdToggleHold();
    } else if (mode == MODE_BANDSCAN) {     // pause / resume the scan
      bscanTogglePause();
    } else {                                // next band button
      specSetBand(nextBandInButtonOrder(specBand));
      specDrawStatic();
    }
  }
  delay(1);   // let the Wi-Fi task and watchdog breathe
}
