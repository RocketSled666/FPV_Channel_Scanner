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
| `Radio.cpp` | Tuning, noise-floor measurement, RSSI fallback, start-up, timing report |
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
  under its number (blank below 3 dB, strongest in amber). Band buttons
  L E R A B F are a row above the graph, as wide as it (selected band in green).
- **FPV Channel Noise**: same layout. Every channel keeps its own long-term
  statistics from start-up (or the last reset): average dB above the floor,
  % of time busy (3 dB or more above the floor) and peak. Nothing fades, so
  the longer it runs, the more clearly quiet channels stand out. Boxes are
  shaded from the quietest channel (black) to the busiest (bright red), and
  numbered by rank above the frequency: 1 = quietest, 48 = busiest. The
  quietest is outlined in white and named in the footer with its average and
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
screen. After it come one page per screen, titled like that screen on a blue
bar, then a Save / Discard page:

| Page | Settings |
|---|---|
| FPV Channel Activity | Span (dB), learn sweeps, samples per channel |
| Spectrum | Step (MHz), samples per point, peak decay, waterfall log curve (0 = linear) |
| FPV Channel Noise | Busy level (dB), shading gamma, full-red spread (dB) |
| Quietest Channels | Minimum history before the list opens (s); L Band Limit (1-8): most L-band channels in the Quietest lists and in each IMD-free set (8 = no limit) |
| Quietest for HDZero | HDZero features, low band L1-L8, E1 and F1 |
| IMD-Free Channels | Min spacing (MHz), use L band |
| User Interface | Long press, Configuration hold, splash time, splash combo, pop-up time, scanning-dot step (ms); IMD refresh (s); moved-channel fade (ms, 0 = off) |
| General | Backlight (%) |

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
  the grid brightness scale and the spectrum graph height, and is remembered
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
- If a frequency's measurement stalls (seems to happen when a signal keeps
  the receiver busy), the receiver's RSSI register is used instead. Serial
  prints a "stalls" line listing those frequencies and the fallback values.
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
simulated radio), in the order the software shows them: the splash, the
Configuration menu pages, then the scanner pages in Right-button order.

<table>
<tr>
<td align="center"><img src="docs/screenshots/01%20Splash.png" width="320" alt="Start-up splash (5 s)"><br>Start-up splash (5 s)</td>
<td align="center"><img src="docs/screenshots/02%20Configuration%20-%20Help.png" width="320" alt="Configuration: Help (opens first)"><br>Configuration: Help (opens first)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/03%20Configuration%20-%20FPV%20Channel%20Activity.png" width="320" alt="Configuration: FPV Channel Activity"><br>Configuration: FPV Channel Activity</td>
<td align="center"><img src="docs/screenshots/04%20Configuration%20-%20Spectrum.png" width="320" alt="Configuration: Spectrum"><br>Configuration: Spectrum</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/05%20Configuration%20-%20FPV%20Channel%20Noise.png" width="320" alt="Configuration: FPV Channel Noise"><br>Configuration: FPV Channel Noise</td>
<td align="center"><img src="docs/screenshots/06%20Configuration%20-%20Quietest%20Channels.png" width="320" alt="Configuration: Quietest Channels"><br>Configuration: Quietest Channels</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/07%20Configuration%20-%20Quietest%20for%20HDZero.png" width="320" alt="Configuration: Quietest for HDZero"><br>Configuration: Quietest for HDZero</td>
<td align="center"><img src="docs/screenshots/08%20Configuration%20-%20IMD-Free%20Channels.png" width="320" alt="Configuration: IMD-Free Channels"><br>Configuration: IMD-Free Channels</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/09%20Configuration%20-%20User%20Interface.png" width="320" alt="Configuration: User Interface"><br>Configuration: User Interface</td>
<td align="center"><img src="docs/screenshots/10%20Configuration%20-%20General.png" width="320" alt="Configuration: General"><br>Configuration: General</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/11%20Configuration%20-%20Save%20-%20Discard.png" width="320" alt="Configuration: Save / Discard"><br>Configuration: Save / Discard</td>
<td align="center"><img src="docs/screenshots/12%20Configuration%20-%20editing%20a%20value.png" width="320" alt="Configuration: editing a value"><br>Configuration: editing a value</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/13%20FPV%20Channel%20Activity.png" width="320" alt="FPV Channel Activity"><br>FPV Channel Activity</td>
<td align="center"><img src="docs/screenshots/14%20FPV%20Channel%20Activity%20%28HDZero%20mode%29.png" width="320" alt="FPV Channel Activity, HDZero mode"><br>FPV Channel Activity, HDZero mode</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/15%20Spectrum%20%28Raceband%29.png" width="320" alt="Spectrum (Raceband) with waterfall"><br>Spectrum (Raceband) with waterfall</td>
<td align="center"><img src="docs/screenshots/16%20FPV%20Channel%20Noise.png" width="320" alt="FPV Channel Noise"><br>FPV Channel Noise</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/17%20FPV%20Channel%20Noise%20%28HDZero%20mode%29.png" width="320" alt="FPV Channel Noise, HDZero mode"><br>FPV Channel Noise, HDZero mode</td>
<td align="center"><img src="docs/screenshots/18%20FPV%20Channel%20Noise%20with%20Wait%20popup.png" width="320" alt="Wait pop-up (Quietest not ready)"><br>Wait pop-up (Quietest not ready)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/19%20Quietest%20Channels.png" width="320" alt="Quietest Channels"><br>Quietest Channels</td>
<td align="center"><img src="docs/screenshots/20%20Quietest%20for%20HDZero.png" width="320" alt="Quietest for HDZero"><br>Quietest for HDZero</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/21%20IMD-Free%20Channels.png" width="320" alt="IMD-Free Channels"><br>IMD-Free Channels</td>
<td align="center"><img src="docs/screenshots/22%20IMD-Free%20Channels%20%28HDZero%20mode%29.png" width="320" alt="IMD-Free Channels, HDZero mode"><br>IMD-Free Channels, HDZero mode</td>
</tr>
</table>
