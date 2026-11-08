# FPV Channel Scanner — Waveshare ESP32-C5-LCD-2.73

Scans the 48 analog FPV channels (bands A, B, E, F, Raceband, Lowband) with the
ESP32-C5's own 5 GHz radio, with no extra receiver hardware. Four pages show
live activity, a per-band spectrum, long-term noise per channel and a ranked
list of the quietest channels.

## Hardware
- [Waveshare ESP32-C5-LCD-2.73](https://docs.waveshare.com/ESP32-C5-LCD-2.73):
  ESP32-C5, 2.73" 320x320 ILI9488 SPI LCD, three buttons (KEY_L, KEY_R,
  BOOT/ENTER). Nothing else is needed.

## Building
This is a standard Arduino sketch and builds in any Arduino-compatible
environment (Arduino IDE, arduino-cli, or an IDE plug-in that uses the Arduino
build system).

The sketch is in the `FPV Channel Scanner/` folder. Its name must match the
`.ino`, so keep it as is.

Requires the **esp32 by Espressif** board package, version **3.3.x**. No extra
libraries.

Board settings:
- Board: **ESP32C5 Dev Module**
- USB CDC On Boot: **Enabled** (serial readout over USB-C)
- PSRAM: **Disabled** (not used; avoids a harmless boot message)
- Everything else at defaults.

Open `FPV Channel Scanner/FPV Channel Scanner.ino`, select the board and
options above, then build and upload. The `.ino` holds only `setup()` and
`loop()`; everything else is in the `.cpp` / `.h` files next to it, which are
compiled automatically.

## Source layout
All in `FPV Channel Scanner/`:

| File | Contents |
|---|---|
| `FPV Channel Scanner.ino` | `setup()` and `loop()` |
| `Config.h` | Pins, tuning constants, shared types, frequency table, colors, radio register access |
| `Scanner.h` | Shared declarations: all global variables and every function |
| `State.cpp` | Global state shared between pages |
| `Display.cpp` | ILI9488 driver, text drawing, IO expander (LCD reset / backlight) |
| `Input.cpp` | Buttons |
| `Radio.cpp` | Tuning, noise-floor measurement, stall handling, start-up, timing report |
| `Bands.cpp` | Band display order (lowest frequency first) |
| `Settings.cpp` | Settings list, limits and defaults; flash load / save |
| `Config Menu.cpp` | Configuration menu |
| `Splash.cpp` | Start-up splash screen |
| `Splash Image.h` | Splash picture data (generated from `Splash.PNG`) |
| `Splash.PNG` | Splash picture, 320 x 320 |
| `Screen Common.cpp` | Drawing shared by pages, pop-up, page navigation |
| `Page Activity.cpp` | FPV Channel Activity page and the channel sweep |
| `Page Spectrum.cpp` | Spectrum page |
| `Page Noise.cpp` | FPV Channel Noise page and the long-term statistics |
| `Page Quietest.cpp` | Quietest Channels, Quietest for HDZero and IMD-Free Channels pages |
| `font5x7.h` | 5x7 bitmap font |

To change the splash picture, replace `Splash.PNG` (320 x 320) and run
`python "tools/Make Splash Header.py" "FPV Channel Scanner/Splash.PNG"
"FPV Channel Scanner/Splash Image.h"` (needs Pillow).

Each source file's header carries the revision number; see `CHANGELOG.md`
for what changed in each revision.

## Screens
Pages in KEY_R order: Activity -> Spectrum -> Noise -> Quietest (or Quietest for HDZero)
-> IMD-Free -> Activity. KEY_L goes back: Activity -> Noise -> Spectrum -> Activity, and
IMD-Free -> Quietest (or Quietest for HDZero) -> Noise. In HDZero mode (see below) the Quietest for
HDZero page replaces Quietest Channels; otherwise only Quietest Channels is shown. Quietest Channels is only reached with KEY_R from Channel Noise, and
only once there are 60 s of noise history; before that a "Wait NNs"
pop-up shows and the page stays put.
- **FPV Channel Activity** (grid): all 48 channels; square brightness = power
  above the noise floor right now. In HDZero mode only channels an HDZero VTX
  can use are drawn; the others are left blank (same on
  the Noise grid, whose shading and ranks then cover only those channels).
- **Spectrum**: continuous sweep of one band in 4 MHz steps (outer channels
  20 MHz beyond the band), with the point nearest each channel moved onto the
  channel's exact frequency, so every channel is measured directly. Channel numbers sit under faint marker lines; white
  dots are a falling peak hold. A 31-pixel waterfall in a white box under the
  graph shows the last 31 sweeps, newest on top (dark blue = weak, bright yellow =
  strong). Each channel's dB above the floor is printed
  under its number (blank below 3 dB, strongest in amber). The graph's
  height is the span (stronger signals are cut off at the top). With
  "Auto range" On (Display page, default Off) it ranges itself instead: at
  least the span, and when a signal goes higher the top jumps to fit it
  (rounded up to 10 dB; the axis label shows it), then steps back down
  10 dB at a time after 5 s without needing it. The waterfall always uses
  the span, so its colours mean the same from row to row (strong signals
  just saturate to yellow). Band buttons
  L E R A B F are a row above the graph, as wide as it (selected band in green).
- **FPV Channel Noise**: same layout. Every channel keeps its own long-term
  statistics from start-up (or the last reset): average dB above the floor,
  % of time busy (6 dB or more above the floor by default, "Busy level") and peak. Nothing fades, so
  the longer it runs, the more clearly quiet channels stand out. Boxes are
  shaded from the quietest channel (black) to the busiest (bright red), and
  numbered by rank above the frequency: 1 = quietest, 48 = busiest. The
  quietest is outlined in amber and named in the footer with its average and
  busy %, e.g. "L5 5510 0.3 dB 2%". The footer also shows how much history
  has been collected. The serial dump lists all three statistics per channel.
- **Quietest Channels**: the 16 quietest channels from the Channel Noise
  history, listed quietest first (left column 1-8, right column 9-16), each
  with band/channel, frequency, average dB above the floor and busy %. It
  keeps measuring, so the list stays live.
- **Quietest for HDZero**: the same list, limited to channels an HDZero VTX
  can be set to: R1-R8, E1, F1, F2, F4, and L1-L8 once low band is unlocked
  on the VTX ([HDZero VTX docs](https://docs.hd-zero.com/vtx-general)).
  HDZero mode only, where it replaces Quietest Channels (reached with KEY_R
  from Channel Noise, after the same wait for history).

- **IMD-Free Channels**: four columns, for 3, 4, 5 and 6 pilots, each
  listing the channel set (band / channel only) with the least
  intermodulation (IMD) interference. Two transmitters on f1 and f2 make
  interference at 2*f1 - f2 and 2*f2 - f1; sets are scored like ET's
  IMDTabler (100 = no product within 35 MHz of a channel in use), and the
  score is shown under each column. Channels are at least "Min spacing"
  apart; among the best sets, the quietest in the noise history wins. In
  HDZero mode only HDZero channels are used, and the footer says
  "HDZero only".
**HDZero mode:** turned on with the "HDZero features" setting in the
Configuration menu. It enables the Quietest for HDZero page, draws only
HDZero channels on the grid pages, shows a blue "HDZ" marker at the top left of the grid pages, and
limits the IMD-Free page to HDZero channels. The HDZero lists can leave out L1-L8 and E1/F1 (settings on the same page).

## Configuration
At power-on a splash screen shows for 5 seconds while the radio starts. Press
KEY_L and KEY_R together during it to open the Configuration menu; otherwise
the Activity grid follows. You can also open the menu at any time by holding
KEY_L and KEY_R together for 5 seconds on any page.

The menu opens on a Help page that lists what each button does on each
screen. After it come the settings, grouped by job (radio, scanning,
channel selection, HDZero, display, controls), each page titled on a blue
bar ("Radio Page", "Display Page", ...), then a Save / Discard page. The "Configuration" title is red:

| Page | Settings |
|---|---|
| Radio | RSSI only, Tuning (Direct, Wi-Fi, Wi-Fi+chan), RX filter (Auto, or 11-23 MHz), Max MHz (Off, or 5800-5995: channels and spectrum points above it are not measured), measurement timeout (ms), 802.11p mode |
| Scanning | Samples per channel (grids), learn sweeps, spectrum step (MHz), spectrum samples per point |
| Channel Selection | Busy level (dB); minimum history before Quietest opens (s); L Band Limit (1-8): most L-band channels in the Quietest lists and in each IMD-free set (8 = no limit); IMD min spacing (MHz), IMD use L band, IMD refresh (s) |
| HDZero | HDZero features, low band L1-L8, E1 and F1 |
| Display | Span (dB), Spectrum auto range, shading gamma, full-red spread (dB), peak decay, waterfall log curve (0 = linear), moved-channel fade (ms, 0 = off), backlight (%) |
| Controls | Long press, Configuration hold, splash time, splash combo, pop-up time, scanning-dot step (ms) |

- **KEY_L / KEY_R**: move up / down the list (past the end of a page goes to
  the next or previous page).
- **ENTER**: flip an On/Off setting, or start editing a number. While
  editing, KEY_L / KEY_R step it down / up (hold to repeat) and ENTER sets it.
- **Hold ENTER**: jump straight to the Save / Discard page.
- **Save and restart** writes everything to flash; **Discard and restart**
  forgets the changes; **Reset to defaults** loads the defaults (then Save).

## Buttons
- **KEY_R / KEY_L**: next / previous page (order above), when released.
- **KEY_L + KEY_R held 5 s**: open the Configuration menu from any page.
- **ENTER (short)** on the grid: cycle the span (15-40 dB, default 15). It sets
  the grid brightness scale and the spectrum's graph height (minimum with Auto range) and
  waterfall scale, and is remembered
  across power cycles.
- **ENTER (short)** on Channel Noise: clear the history (does nothing on the Quietest pages).
- **ENTER (short)** on IMD-Free: hold the channel sets (they stop being
  re-picked and "Hold" flashes under the table); ENTER again, or going to
  another page, releases them.
  and start over.
  (Re-learning the noise floor also clears it.)
- **ENTER (short)** on the spectrum: select the next band button. In HDZero
  mode, bands with no HDZero channel (A, B, and L or E when those are off in
  the HDZero settings) are drawn darker grey and skipped.
- **ENTER (hold 0.7 s)**: re-learn the noise floor (grid: all channels;
  spectrum: current band). Do it with your VTXs off if you can.
- Serial monitor (115200) prints readings once per second.

## How it works
- Wi-Fi is started in STA mode, 5 GHz only, promiscuous, just to power up and
  calibrate the 5 GHz receive chain. It never connects or transmits.
- `phy_set_chanfreq(MHz, 0)` (internal libphy function) tunes the synthesizer to
  any frequency. Values above 14 are treated as MHz, so frequencies that aren't
  Wi-Fi channels (all of L band, most of R/B) work.
- The baseband's noise-floor estimator (registers 0x600A7018 / 0x600A7050)
  measures in-band power over the 20 MHz receive bandwidth. It's read raw,
  without the -98...-88 dB clamp Espressif's own test code applies.

## Caveats
- Measurement uses libphy's signal RSSI by default ("RSSI only" On, see
  below). With it Off, the baseband noise-floor estimator is used instead;
  if a frequency's measurement stalls (seems to happen when a signal keeps
  the receiver busy, and on some boards on every sweep above 5.9 GHz), the
  last good reading at that frequency is held, and kept out of the floor
  and the noise history. A channel with more than 60% of recent
  readings held shows "--" on the grids (until it drops below 30%) and is
  left out of the Noise ranking, Quietest lists and IMD-free sets. Espressif
  documents the C5's 5 GHz range only up to 5885 MHz, so R8 and E6-E8 are
  outside it; the experimental "802.11p mode" setting is reported to help
  5750-5990 MHz (it doubles measurement time, so the timeout is fixed at
  12 ms while it is on). "Meas timeout" sets how long to wait
  before a noise-floor reading counts as stalled. How often this happens
  varies a lot from board to board.
- "RSSI only" (default On) measures with libphy's signal RSSI
  (phy_get_sigrssi(), dBm) instead of the noise-floor estimator; on the C5
  the two can't run together. Each sample is the median of 5 reads 250 us
  apart (about 1 ms). It reads live on every channel, R8 and E6-E8
  included, with no stalls on the boards tested, at about 4 grid sweeps/s.
  Resolution is 1 dB, against 0.25 dB for the estimator, so Samples/channel
  defaults to 3; switching RSSI only Off in the menu sets it to 1, and
  switching it On sets it back to 3 (you can still change it after). A "rssi only" serial line
  shows reads out of range, the dBm range and how often a sample repeated
  the previous one (near 100% would mean the value isn't updating).
- Some boards read junk at the top of the band: one test board showed a
  steady -30 to -40 dBm block from about 5925 MHz up that a second board
  beside it didn't see (beyond the C5's documented 5885 MHz). On such a
  board set "Max MHz" (Radio page), e.g. 5915: channels above it show
  "--" and are left out of the Noise ranking, Quietest and IMD-free sets,
  and the spectrum greys out everything above it. They aren't measured, so
  sweeps get a little faster too.
- "Tuning" (Radio page, default Wi-Fi) picks how the
  receiver is tuned. Direct is libphy phy_set_chanfreq(MHz), as before.
  Wi-Fi parks the Wi-Fi driver on channel 140 and tunes with libphy
  phy_change_channel(MHz, 1, 0, 0); Wi-Fi+chan also re-sets channel 140
  before every retune. ESPsoup (an ESP32-C5 SDR firmware) measured that
  path receiving a test tone at every point up to 5990 MHz, while its
  direct path lost 1.5-4 dB on one board and about 20 dB on another above
  about 5745 MHz and was nearly deaf at 5900 MHz. Compare the boards with
  each setting (same VTX, same spot); the boot timing test shows what it
  costs per retune.
- "RX filter" (Radio page, default 16 MHz) sets the analog channel filter
  width: Auto leaves it as calibrated, 11-23 MHz forces the filter
  capacitor code (BBTOP block 0x67, registers 6/7) after every retune.
  Narrower separates neighbouring channels better and lowers the floor,
  but cuts some of a wide signal (analog video is about 15-20 MHz wide).
  Widths are approximate (ESP-SDR / ESPsoup noise measurements, PHY
  channel mode 0, which Direct tuning uses); with Wi-Fi tuning they may
  differ. The boot log shows the calibrated values and the code used.
- The spectrum tunes in 4 MHz steps (2-6 MHz around channels), off the normal
  5 MHz Wi-Fi grid. If the synthesizer can't land on those exactly, the trace
  will look stepped; set `SPEC_STEP_MHZ` to 5.
- These PHY calls and registers are undocumented, found by disassembling
  Espressif's libphy.a. A future core update could move or change them.
- Readings are relative dB, not calibrated dBm.
- With a 20 MHz receive bandwidth, channels closer than ~20 MHz (e.g. A8 5725 /
  B1 5733 / R3 5732) will light up together. That's the radio, not a bug.
- RF calibration outside the Wi-Fi channel plan (L band, E7/E8 above 5925 MHz)
  is interpolated by the PHY, so sensitivity there may be lower.

## License
MIT - see [LICENSE](LICENSE).

## Screenshots
Simulated screenshots (the firmware's own drawing code run on a PC with a
simulated radio): the splash, the scanner pages in Right-button order, then
the Configuration menu pages.

<table>
<tr>
<td align="center"><img src="docs/screenshots/01%20Splash.png" width="320" alt="Start-up splash (5 s)"><br>Start-up splash (5 s)</td>
<td align="center"><img src="docs/screenshots/02%20FPV%20Channel%20Activity.png" width="320" alt="FPV Channel Activity"><br>FPV Channel Activity</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/03%20FPV%20Channel%20Activity%20%28HDZero%20mode%29.png" width="320" alt="FPV Channel Activity, HDZero mode"><br>FPV Channel Activity, HDZero mode</td>
<td align="center"><img src="docs/screenshots/04%20Spectrum%20%28Raceband%29.png" width="320" alt="Spectrum (Raceband) with waterfall"><br>Spectrum (Raceband) with waterfall</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/05%20FPV%20Channel%20Noise.png" width="320" alt="FPV Channel Noise"><br>FPV Channel Noise</td>
<td align="center"><img src="docs/screenshots/06%20FPV%20Channel%20Noise%20%28HDZero%20mode%29.png" width="320" alt="FPV Channel Noise, HDZero mode"><br>FPV Channel Noise, HDZero mode</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/07%20FPV%20Channel%20Noise%20with%20Wait%20popup.png" width="320" alt="Wait pop-up (Quietest not ready)"><br>Wait pop-up (Quietest not ready)</td>
<td align="center"><img src="docs/screenshots/08%20Quietest%20Channels.png" width="320" alt="Quietest Channels"><br>Quietest Channels</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/09%20Quietest%20for%20HDZero.png" width="320" alt="Quietest for HDZero"><br>Quietest for HDZero</td>
<td align="center"><img src="docs/screenshots/10%20IMD-Free%20Channels.png" width="320" alt="IMD-Free Channels"><br>IMD-Free Channels</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/11%20IMD-Free%20Channels%20%28HDZero%20mode%29.png" width="320" alt="IMD-Free Channels, HDZero mode"><br>IMD-Free Channels, HDZero mode</td>
<td align="center"><img src="docs/screenshots/12%20Configuration%20-%20Help.png" width="320" alt="Configuration: Help (opens first)"><br>Configuration: Help (opens first)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/13%20Configuration%20-%20Radio.png" width="320" alt="Configuration: Radio"><br>Configuration: Radio</td>
<td align="center"><img src="docs/screenshots/14%20Configuration%20-%20Scanning.png" width="320" alt="Configuration: Scanning"><br>Configuration: Scanning</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/15%20Configuration%20-%20Channel%20Selection.png" width="320" alt="Configuration: Channel Selection"><br>Configuration: Channel Selection</td>
<td align="center"><img src="docs/screenshots/16%20Configuration%20-%20HDZero.png" width="320" alt="Configuration: HDZero"><br>Configuration: HDZero</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/17%20Configuration%20-%20Display.png" width="320" alt="Configuration: Display"><br>Configuration: Display</td>
<td align="center"><img src="docs/screenshots/18%20Configuration%20-%20Controls.png" width="320" alt="Configuration: Controls"><br>Configuration: Controls</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/19%20Configuration%20-%20Save%20-%20Discard.png" width="320" alt="Configuration: Save / Discard"><br>Configuration: Save / Discard</td>
<td align="center"><img src="docs/screenshots/20%20Configuration%20-%20editing%20a%20value.png" width="320" alt="Configuration: editing a value"><br>Configuration: editing a value</td>
</tr>
</table>
