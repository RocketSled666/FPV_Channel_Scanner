/*
 * FPV Channel Scanner - Input.cpp
 * Revision: 79
 *
 * KEY_L / KEY_R / ENTER button polling. Called often, including from inside
 * the radio sweeps, so presses are caught while a sweep is running.
 */

#include "Scanner.h"

// Called often, including from inside the radio sweeps, so presses are caught
// even while a sweep is running.
//
// KEY_L / KEY_R change page when released, so that holding both together
// (KEY_L + KEY_R for CONFIG_HOLD_MS opens the Configuration menu) never
// changes page: once both have been down at the same time, neither release
// counts until both are up again.
static bool keyCombo = false;              // both page keys were down together

void pollButton(ButtonState &b, uint32_t now, bool isEnter) {
  bool d = digitalRead(b.pin) == LOW;
  if (d && !b.down) {                       // pressed
    b.down = true; b.downSinceMs = now; b.longFired = false;
  } else if (!d && b.down) {                // released
    b.down = false;
    if (isEnter && !b.longFired && now - b.downSinceMs >= 30) evEnterShort = true;
    if (!isEnter && !keyCombo && now - b.downSinceMs >= 30) {   // page change
      evToggle = true;
      pageDir = (b.pin == PIN_KEY_L) ? -1 : 1;
    }
  } else if (d && isEnter && !b.longFired && now - b.downSinceMs >= LONG_PRESS_MS) {
    b.longFired = true; evEnterLong = true;
  }
}

void pollInput() {
  uint32_t now = millis();
  if (now - lastInputPollMs < 4) return;
  lastInputPollMs = now;
  pollButton(btnKeyL,  now, false);
  pollButton(btnKeyR,  now, false);
  pollButton(btnEnter, now, true);

  // KEY_L + KEY_R held together: open the Configuration menu after CONFIG_HOLD_MS
  static uint32_t comboSinceMs = 0;
  static bool comboFired = false;
  if (btnKeyL.down && btnKeyR.down) {
    if (!keyCombo) { keyCombo = true; comboSinceMs = now; comboFired = false; }
    if (!comboFired && now - comboSinceMs >= CONFIG_HOLD_MS) { comboFired = true; evConfig = true; }
  } else if (!btnKeyL.down && !btnKeyR.down) {
    keyCombo = false;                       // both up: page keys work normally again
  }
  updateScanDots();
}
