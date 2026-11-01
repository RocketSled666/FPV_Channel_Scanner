/*
 * FPV Channel Scanner
 * Revision: 35
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
 * Five pages, in KEY_R order:
 *   FPV CHANNEL ACTIVITY (grid) - all 48 FPV channels, square brightness =
 *               power above floor right now
 *   SPECTRUM  - continuous sweep across one band in 4 MHz steps
 *   FPV CHANNEL NOISE - same layout as the grid; each box = its signal above
 *               the floor averaged over all time since start or the last
 *               reset, shaded from the quietest channel (black) to the
 *               busiest (bright red); the quietest is outlined and named
 *   QUIETEST CHANNELS - the 16 quietest channels from the Channel Noise
 *               history, in order (1 = quietest), with average dB and busy %.
 *               Reached only by KEY_R from Channel Noise, once there is at
 *               least 60 s of history; before that a "Wait longer" pop-up
 *               shows and the page doesn't change.
 *   QUIETEST FOR HDZERO - the same list, limited to channels an HDZero VTX
 *               can use: R1-R8, E1, F1, F2, F4 and L1-L8 (low band unlocked).
 *               Reached only by KEY_R from Quietest Channels.
 *
 *   KEY_R: Activity -> Spectrum -> Noise -> Quietest -> HDZero -> Activity
 *   KEY_L: Activity -> Noise -> Spectrum -> Activity;
 *          HDZero -> Quietest -> Noise
 *
 * Buttons:
 *   KEY_L / KEY_R (GPIO2 / GPIO3) = previous / next page (order above)
 *   ENTER (short) on GRID     = cycle the span (dB)
 *   ENTER (short) on NOISE or either QUIETEST page = clear the noise history
 *   ENTER (short) on SPECTRUM = select the next band button under the graph
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
 *   Radio.cpp       tuning, measurement, RSSI fallback, timing
 *   Bands.cpp       band display order
 *   Screen Common.cpp  shared drawing, pop-up, page navigation
 *   Page Activity.cpp / Page Spectrum.cpp / Page Noise.cpp / Page Quietest.cpp
 *   font5x7.h       5x7 bitmap font
 *
 * Revision history: see CHANGELOG.md
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
  delay(200);
  Serial.println("\nFPV Channel Scanner (ESP32-C5)");

  pinMode(PIN_LCD_CS, OUTPUT); digitalWrite(PIN_LCD_CS, HIGH);
  pinMode(PIN_SD_CS,  OUTPUT); digitalWrite(PIN_SD_CS,  HIGH);
  pinMode(PIN_LCD_DC, OUTPUT);
  pinMode(PIN_KEY_L,     INPUT_PULLUP);
  pinMode(PIN_KEY_R,     INPUT_PULLUP);
  pinMode(PIN_BTN_ENTER, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  SPI.begin(PIN_LCD_SCK, PIN_LCD_MISO, PIN_LCD_MOSI, -1);

  lcdInit();
  exioBacklight(100);
  gridDrawStatic();
  lcdText(4, FOOT_Y + 2, "Starting radio", 2, C_DIM, C_BG);

  radioInit();
  gridResetFloor();
  for (int b = 0; b < NUM_BANDS; b++) specFloorValid[b] = false;
  specSetBand(specBand);

  uint32_t t0 = micros();
  float t = readFreqDb(5800, GRID_SAMPLES, SETTLE_US);
  Serial.printf("Self-test 5800 MHz: %.1f dB, %lu us/channel, timeouts=%lu\n",
                t, (unsigned long)(micros() - t0), (unsigned long)g_nfTimeouts);
  bootTimingTest();
  g_tTuneUs = g_tMeasUs = g_tDrawUs = 0; g_nTunes = g_nMeas = 0;
  gridInvalidate();
}

void loop() {
  if (mode == MODE_SPECTRUM) specSweep(); else gridSweep();
  pollInput();

  if (evToggle) {
    evToggle = false; evEnterShort = evEnterLong = false;
    ScreenMode next = pageAfter(mode, pageDir);
    if (next == MODE_QUIET && actTrackedS < QUIET_MIN_HISTORY_S) {
      showPopup("Wait longer");                      // not enough history yet
      Serial.printf("quietest: %lu of %d s of history, wait longer\n",
                    (unsigned long)actTrackedS, QUIET_MIN_HISTORY_S);
    } else {
      enterMode(next);
      Serial.printf("page -> %s\n", mode == MODE_GRID ? "channel activity" : mode == MODE_ACTIVITY ? "channel noise" :
                                mode == MODE_QUIET ? "quietest channels" :
                                mode == MODE_HDZERO ? "quietest for HDZero" : "spectrum");
    }
  }
  if (popupUntilMs && (int32_t)(millis() - popupUntilMs) >= 0) {
    popupUntilMs = 0;
    lcdFill(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, 0, 0, 0);   // clear it, gaps included
    actInvalidate();                         // redraw the boxes the pop-up covered
  }
  if (evEnterLong) {                        // hold: re-learn the noise floor
    evEnterLong = false;
    if (mode == MODE_GRID) { gridResetFloor(); gridInvalidate(); gridFooterDirty = true; drawTitleStatus(0, true); }
    else if (mode == MODE_ACTIVITY) { gridResetFloor(); actInvalidate(); actFooterDirty = true; drawTitleStatus(0, true); }
    else if (quietPage()) { gridResetFloor(); quietInvalidate(); quietFooterDirty = true; drawTitleStatus(0, true); }
    else { specFloorValid[specBand] = false; specNoSeedOnce = true; specSetBand(specBand); specDrawStatic(); }
  }
  if (evEnterShort) {
    evEnterShort = false;
    if (mode == MODE_GRID) {                // cycle brightness span
      spanDb = (spanDb >= 40) ? 15 : spanDb + 5;
      gridInvalidate(); gridFooterDirty = true;
    } else if (mode == MODE_ACTIVITY) {     // restart the activity sums
      actClear(); actInvalidate(); actFooterDirty = true;
    } else if (quietPage()) {               // same history as the noise page
      actClear(); quietInvalidate(); quietFooterDirty = true;
    } else {                                // next band button
      specSetBand(nextBandInButtonOrder(specBand));
      specDrawStatic();
    }
  }
  delay(1);   // let the Wi-Fi task and watchdog breathe
}
