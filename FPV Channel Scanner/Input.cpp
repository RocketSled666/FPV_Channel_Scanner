/*
 * FPV Channel Scanner - Input.cpp
 * Revision: 34
 *
 * KEY_L / KEY_R / ENTER button polling. Called often, including from inside
 * the radio sweeps, so presses are caught while a sweep is running.
 */

#include "Scanner.h"

// Called often, including from inside the radio sweeps, so presses are caught
// even while a sweep is running.
void pollButton(ButtonState &b, uint32_t now, bool isEnter) {
  bool d = digitalRead(b.pin) == LOW;
  if (d && !b.down) {                       // pressed
    b.down = true; b.downSinceMs = now; b.longFired = false;
    if (!isEnter) {                         // KEY_L / KEY_R switch page on press
      evToggle = true;
      pageDir = (b.pin == PIN_KEY_L) ? -1 : 1;
    }
  } else if (!d && b.down) {                // released
    b.down = false;
    if (isEnter && !b.longFired && now - b.downSinceMs >= 30) evEnterShort = true;
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
  updateScanDots();
}
