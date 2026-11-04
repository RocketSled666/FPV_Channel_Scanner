/*
 * FPV Channel Scanner - Display.cpp
 * Revision: 87
 *
 * ILI9488 320x320 LCD driver (SPI, 18-bit color), text drawing with the
 * 5x7 font, and the CH32V003 IO expander used for LCD reset and backlight.
 */

#include "Scanner.h"
#include "font5x7.h"

static uint16_t exioOut = 0xFFFF;
static SPISettings lcdSpi(LCD_SPI_HZ, MSBFIRST, SPI_MODE0);
static uint8_t lineBuf[LCD_W * 3];
#define LCMD(c, ...) do { static const uint8_t _d[] = { __VA_ARGS__ }; lcdCmd(c, _d, sizeof(_d)); } while (0)

void exioWrite(const uint8_t *d, size_t n) {
  Wire.beginTransmission(EXIO_ADDR);
  Wire.write(d, n);
  Wire.endTransmission();
}

void exioPin(uint8_t pin, bool level) {
  if (level) exioOut |= (1u << pin); else exioOut &= ~(1u << pin);
  uint8_t d[3] = { EXIO_REG_OUT, (uint8_t)exioOut, (uint8_t)(exioOut >> 8) };
  exioWrite(d, 3);
}

void exioBacklight(uint8_t pct) {      // same scaling as Waveshare BSP
  if (pct > 97) pct = 97;
  uint8_t d[2] = { EXIO_REG_PWM, (uint8_t)(pct * 255 / 100) };
  exioWrite(d, 2);
}

void lcdCmd(uint8_t c, const uint8_t *data, size_t n) {
  SPI.beginTransaction(lcdSpi);
  digitalWrite(PIN_LCD_CS, LOW);
  digitalWrite(PIN_LCD_DC, LOW);
  SPI.transfer(c);
  if (n) { digitalWrite(PIN_LCD_DC, HIGH); SPI.writeBytes(data, n); }
  digitalWrite(PIN_LCD_CS, HIGH);
  SPI.endTransaction();
}

void lcdCmd0(uint8_t c) { lcdCmd(c, nullptr, 0); }

void lcdInit() {
  // Hardware reset through the IO expander
  uint8_t modeCfg[3] = { EXIO_REG_MODE, 0xF7, 0xFF };  // as Waveshare BSP
  exioWrite(modeCfg, 3);
  exioPin(EXIO_PIN_LCDRST, 0); delay(10);
  exioPin(EXIO_PIN_LCDRST, 1); delay(120);

  // Init sequence taken from Waveshare's ILI9488 driver for this panel
  LCMD(0xC0, 0x10, 0x12);
  LCMD(0xC1, 0x66);
  LCMD(0xC5, 0x00, 0x45, 0x80);
  LCMD(0x36, 0x48);                 // MADCTL: MX | BGR
  LCMD(0x3A, 0x66);                 // 18-bit color
  LCMD(0xB0, 0x80);
  LCMD(0xB1, 0xA0);
  LCMD(0xB4, 0x02);
  LCMD(0xB6, 0x00, 0x02, 0x3B);
  LCMD(0xE0, 0x00,0x08,0x19,0x05,0x16,0x09,0x35,0x47,0x4A,0x04,0x0D,0x0C,0x30,0x33,0x0F);
  LCMD(0xE1, 0x00,0x08,0x19,0x05,0x16,0x09,0x35,0x47,0x4A,0x04,0x0D,0x0C,0x30,0x33,0x0F);
  LCMD(0xE9, 0x00);
  LCMD(0xF7, 0xA9, 0x51, 0x2C, 0x82);
  lcdCmd0(0x21);                    // inversion on (panel needs it)
  lcdCmd0(0x11); delay(120);        // sleep out
  lcdCmd0(0x29); delay(20);         // display on
}

void lcdWindow(int x, int y, int w, int h) {
  uint8_t ca[4] = { (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)((x + w - 1) >> 8), (uint8_t)(x + w - 1) };
  uint8_t ra[4] = { (uint8_t)(y >> 8), (uint8_t)y, (uint8_t)((y + h - 1) >> 8), (uint8_t)(y + h - 1) };
  lcdCmd(0x2A, ca, 4);
  lcdCmd(0x2B, ra, 4);
}

// One row of pixels from a buffer of R, G, B bytes (3 per pixel).
void lcdDrawRgbRow(int x, int y, int w, const uint8_t *rgb) {
  if (w <= 0) return;
  lcdWindow(x, y, w, 1);
  SPI.beginTransaction(lcdSpi);
  digitalWrite(PIN_LCD_CS, LOW);
  digitalWrite(PIN_LCD_DC, LOW);
  SPI.transfer(0x2C);
  digitalWrite(PIN_LCD_DC, HIGH);
  SPI.writeBytes(rgb, w * 3);
  digitalWrite(PIN_LCD_CS, HIGH);
  SPI.endTransaction();
}

void lcdFill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  if (w <= 0 || h <= 0) return;
  lcdWindow(x, y, w, h);
  for (int i = 0; i < w; i++) { lineBuf[i*3] = r; lineBuf[i*3+1] = g; lineBuf[i*3+2] = b; }
  SPI.beginTransaction(lcdSpi);
  digitalWrite(PIN_LCD_CS, LOW);
  digitalWrite(PIN_LCD_DC, LOW);
  SPI.transfer(0x2C);
  digitalWrite(PIN_LCD_DC, HIGH);
  for (int j = 0; j < h; j++) SPI.writeBytes(lineBuf, w * 3);
  digitalWrite(PIN_LCD_CS, HIGH);
  SPI.endTransaction();
}

void lcdFillC(int x, int y, int w, int h, RGB c) { lcdFill(x, y, w, h, c.r, c.g, c.b); }

// Draw text with a solid background (glyph box drawn as one window).
void lcdText(int x, int y, const char *s, int scale, RGB fg, RGB bg) {
  for (; *s; s++, x += 6 * scale) {
    char c = *s;
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *g = FONT5X7[c - 0x20];
    lcdWindow(x, y, 6 * scale, 8 * scale);
    SPI.beginTransaction(lcdSpi);
    digitalWrite(PIN_LCD_CS, LOW);
    digitalWrite(PIN_LCD_DC, LOW);
    SPI.transfer(0x2C);
    digitalWrite(PIN_LCD_DC, HIGH);
    for (int row = 0; row < 8; row++) {
      int n = 0;
      for (int col = 0; col < 6; col++) {
        bool on = col < 5 && ((g[col] >> row) & 1);
        RGB p = on ? fg : bg;
        for (int k = 0; k < scale; k++) { lineBuf[n++] = p.r; lineBuf[n++] = p.g; lineBuf[n++] = p.b; }
      }
      for (int k = 0; k < scale; k++) SPI.writeBytes(lineBuf, n);
    }
    digitalWrite(PIN_LCD_CS, HIGH);
    SPI.endTransaction();
  }
}

// Text at any character cell size (cw x ch pixels, normal font is 6 x 8),
// nearest-neighbour scaled, for sizes between the integer scales.
void lcdTextSized(int x, int y, const char *s, int cw, int ch, RGB fg, RGB bg) {
  for (; *s; s++, x += cw) {
    char c = *s;
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *g = FONT5X7[c - 0x20];
    lcdWindow(x, y, cw, ch);
    SPI.beginTransaction(lcdSpi);
    digitalWrite(PIN_LCD_CS, LOW);
    digitalWrite(PIN_LCD_DC, LOW);
    SPI.transfer(0x2C);
    digitalWrite(PIN_LCD_DC, HIGH);
    for (int py = 0; py < ch; py++) {
      int row = py * 8 / ch, n = 0;
      for (int px = 0; px < cw; px++) {
        int col = px * 6 / cw;
        bool on = col < 5 && row < 7 && ((g[col] >> row) & 1);
        RGB p = on ? fg : bg;
        lineBuf[n++] = p.r; lineBuf[n++] = p.g; lineBuf[n++] = p.b;
      }
      SPI.writeBytes(lineBuf, n);
    }
    digitalWrite(PIN_LCD_CS, HIGH);
    SPI.endTransaction();
  }
}

RGB scaleColor(RGB c, float k) {
  RGB o = { (uint8_t)(c.r * k), (uint8_t)(c.g * k), (uint8_t)(c.b * k) };
  return o;
}
