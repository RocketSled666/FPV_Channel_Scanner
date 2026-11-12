/*
 * FPV Channel Scanner - Display.cpp
 * Revision: 102
 *
 * ILI9488 320x320 LCD driver (SPI, 18-bit color) on ESP-IDF's esp_lcd
 * panel IO, text drawing with the 5x7 font, and the CH32V003 IO expander
 * used for LCD reset and backlight.
 */

#include "Scanner.h"
#include "font5x7.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"

#define LCD_CHUNK_ROWS 16                          // full-width rows per pixel transfer
#define LCD_BUF_BYTES  (LCD_W * LCD_CHUNK_ROWS * 3)

static uint16_t exioOut = 0xFFFF;
static esp_lcd_panel_io_handle_t lcdIo = nullptr;
alignas(4) static uint8_t pixBuf[2][LCD_BUF_BYTES];  // two DMA buffers, used alternately
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

// Command plus parameters. Waits for any pixel transfer still in progress.
void lcdCmd(uint8_t c, const uint8_t *data, size_t n) {
  esp_lcd_panel_io_tx_param(lcdIo, c, n ? data : nullptr, n);
}

void lcdCmd0(uint8_t c) { lcdCmd(c, nullptr, 0); }

// SPI bus and panel IO as in Waveshare's BSP (esp_lcd, hardware CS and DC).
static bool lcdBusInit() {
  spi_bus_config_t bus = {};
  bus.sclk_io_num = PIN_LCD_SCK;
  bus.mosi_io_num = PIN_LCD_MOSI;
  bus.miso_io_num = PIN_LCD_MISO;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = LCD_BUF_BYTES;
  esp_err_t e = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  if (e != ESP_OK) { Serial.printf("lcd: spi_bus_initialize: %s\n", esp_err_to_name(e)); return false; }
  esp_lcd_panel_io_spi_config_t io = {};
  io.dc_gpio_num = PIN_LCD_DC;
  io.cs_gpio_num = PIN_LCD_CS;
  io.pclk_hz = LCD_SPI_HZ;
  io.lcd_cmd_bits = 8;
  io.lcd_param_bits = 8;
  io.spi_mode = 0;
  io.trans_queue_depth = 10;
  e = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &lcdIo);
  if (e != ESP_OK) { Serial.printf("lcd: new_panel_io_spi: %s\n", esp_err_to_name(e)); lcdIo = nullptr; return false; }
  return true;
}

void lcdInit() {
  if (!lcdIo && !lcdBusInit()) return;
  // Hardware reset through the IO expander
  uint8_t modeCfg[3] = { EXIO_REG_MODE, 0xF7, 0xFF };  // as Waveshare BSP
  exioWrite(modeCfg, 3);
  exioPin(EXIO_PIN_LCDRST, 0); delay(10);
  exioPin(EXIO_PIN_LCDRST, 1); delay(120);
  lcdCmd0(0x01); delay(120);        // software reset

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

// Sends a w x h window, rowFn writing each row's R, G, B bytes. Chunks
// alternate between the two buffers: each transfer waits for the one before
// it, so a buffer is never refilled while it is being sent.
typedef void (*RowFn)(uint8_t *dst, int row, int w, const void *ctx);
static void lcdStreamRows(int x, int y, int w, int h, RowFn rowFn, const void *ctx) {
  if (!lcdIo || w <= 0 || h <= 0) return;
  lcdWindow(x, y, w, h);
  const int rowBytes = w * 3;
  const int perChunk = LCD_BUF_BYTES / rowBytes;
  int sel = 0;
  for (int r = 0; r < h; ) {
    int n = min(perChunk, h - r);
    uint8_t *buf = pixBuf[sel];
    for (int k = 0; k < n; k++) rowFn(buf + k * rowBytes, r + k, w, ctx);
    esp_lcd_panel_io_tx_color(lcdIo, r == 0 ? 0x2C : 0x3C, buf, n * rowBytes);   // memory write / continue
    r += n; sel ^= 1;
  }
}

static void rowCopy(uint8_t *dst, int, int w, const void *ctx) { memcpy(dst, ctx, w * 3); }

// One row of pixels from a buffer of R, G, B bytes (3 per pixel).
void lcdDrawRgbRow(int x, int y, int w, const uint8_t *rgb) {
  lcdStreamRows(x, y, w, 1, rowCopy, rgb);
}

static void rowSolid(uint8_t *dst, int, int w, const void *ctx) {
  const RGB *c = (const RGB *)ctx;
  for (int i = 0; i < w; i++) { dst[i * 3] = c->r; dst[i * 3 + 1] = c->g; dst[i * 3 + 2] = c->b; }
}

void lcdFill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  RGB c = { r, g, b };
  lcdStreamRows(x, y, w, h, rowSolid, &c);
}

void lcdFillC(int x, int y, int w, int h, RGB c) { lcdFill(x, y, w, h, c.r, c.g, c.b); }

// One character cell: glyph g scaled to cw x ch pixels (nearest neighbour).
struct GlyphCtx { const uint8_t *g; int ch, rows; RGB fg, bg; };
static void rowGlyph(uint8_t *dst, int py, int cw, const void *ctx) {
  const GlyphCtx *k = (const GlyphCtx *)ctx;
  int row = py * 8 / k->ch;
  for (int px = 0; px < cw; px++) {
    int col = px * 6 / cw;
    bool on = col < 5 && row < k->rows && ((k->g[col] >> row) & 1);
    RGB p = on ? k->fg : k->bg;
    *dst++ = p.r; *dst++ = p.g; *dst++ = p.b;
  }
}

static void drawText(int x, int y, const char *s, int cw, int ch, int rows, RGB fg, RGB bg) {
  for (; *s; s++, x += cw) {
    char c = *s;
    if (c < 0x20 || c > 0x7E) c = '?';
    GlyphCtx k = { FONT5X7[c - 0x20], ch, rows, fg, bg };
    lcdStreamRows(x, y, cw, ch, rowGlyph, &k);
  }
}

// Draw text with a solid background (glyph box drawn as one window).
void lcdText(int x, int y, const char *s, int scale, RGB fg, RGB bg) {
  drawText(x, y, s, 6 * scale, 8 * scale, 8, fg, bg);
}

// Text at any character cell size (cw x ch pixels, normal font is 6 x 8),
// nearest-neighbour scaled, for sizes between the integer scales.
void lcdTextSized(int x, int y, const char *s, int cw, int ch, RGB fg, RGB bg) {
  drawText(x, y, s, cw, ch, 7, fg, bg);
}

// Text rotated 90 degrees anticlockwise (reads bottom to top), normal size:
// a box 8 px wide and 6 px per character tall, top-left corner at x, y.
struct VTextCtx { const char *s; int len; RGB fg, bg; };
static void rowVText(uint8_t *dst, int py, int w, const void *ctx) {
  const VTextCtx *k = (const VTextCtx *)ctx;
  int along = k->len * 6 - 1 - py;                   // position along the text
  char c = k->s[along / 6];
  if (c < 0x20 || c > 0x7E) c = '?';
  const uint8_t *g = FONT5X7[c - 0x20];
  int col = along % 6;
  for (int px = 0; px < w; px++) {
    bool on = col < 5 && px < 7 && ((g[col] >> px) & 1);
    RGB p = on ? k->fg : k->bg;
    *dst++ = p.r; *dst++ = p.g; *dst++ = p.b;
  }
}

void lcdTextV(int x, int y, const char *s, RGB fg, RGB bg) {
  VTextCtx k = { s, (int)strlen(s), fg, bg };
  lcdStreamRows(x, y, 8, 6 * k.len, rowVText, &k);
}

RGB scaleColor(RGB c, float k) {
  RGB o = { (uint8_t)(c.r * k), (uint8_t)(c.g * k), (uint8_t)(c.b * k) };
  return o;
}
