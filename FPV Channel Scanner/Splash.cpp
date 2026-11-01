/*
 * FPV Channel Scanner - Splash.cpp
 * Revision: 46
 *
 * Start-up splash screen: the picture from Splash.PNG (stored in
 * "Splash Image.h", made by tools/Make Splash Header.py), with a blue hint
 * at the bottom. It stays up for SPLASH_MS; the radio starts underneath it.
 * Pressing KEY_L and KEY_R together while it shows opens the Configuration
 * (settings) menu.
 */

#include "Scanner.h"
#include "Splash Image.h"

// Draw the picture row by row, turning palette indexes into R, G, B bytes.
void showSplash() {
  static uint8_t row[SPLASH_W * 3];
  for (int y = 0; y < SPLASH_H; y++) {
    const uint8_t *src = &SPLASH_PIXELS[y * SPLASH_W];
    for (int x = 0; x < SPLASH_W; x++) {
      const uint8_t *c = SPLASH_PALETTE[src[x]];
      row[x * 3] = c[0]; row[x * 3 + 1] = c[1]; row[x * 3 + 2] = c[2];
    }
    lcdDrawRgbRow(0, y, SPLASH_W, row);
  }
  // Hint at the bottom (the picture is black there). Too wide for one line
  // at this font size, so it is split over two centred lines.
  const char *l1 = "Press forward+backward buttons now";
  const char *l2 = "for Settings screen";
  lcdText((LCD_W - 6 * (int)strlen(l1)) / 2, LCD_H - 22, l1, 1, C_HDZ, C_BG);
  lcdText((LCD_W - 6 * (int)strlen(l2)) / 2, LCD_H - 11, l2, 1, C_HDZ, C_BG);
}

// Keep the splash up until SPLASH_MS after startMs. Returns true as soon as
// KEY_L and KEY_R are both held (for SPLASH_COMBO_MS, to ignore bounce).
bool splashWait(uint32_t startMs) {
  uint32_t bothSince = 0;
  while (millis() - startMs < SPLASH_MS) {
    bool both = digitalRead(PIN_KEY_L) == LOW && digitalRead(PIN_KEY_R) == LOW;
    if (both) {
      if (!bothSince) bothSince = millis();
      if (millis() - bothSince >= SPLASH_COMBO_MS) return true;
    } else {
      bothSince = 0;
    }
    delay(10);
  }
  return false;
}
