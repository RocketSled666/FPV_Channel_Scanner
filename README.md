# FPV Channel Scanner — Waveshare ESP32-C5-LCD-2.73

Scans the 48 analog FPV channels (bands A, B, E, F, Raceband, Lowband) with the
ESP32-C5's own 5 GHz radio, with no extra receiver hardware. Four pages show
live activity, a per-band spectrum, long-term noise per channel and a ranked
list of the quietest channels.

## Hardware
<p align="center"><img src="docs/images/Board%20Splash.png" width="360" alt="The scanner's start-up splash on the Waveshare ESP32-C5-LCD-2.73"></p>

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
| `Page Band Scan.cpp` | Band Scan page |
| `Page Noise.cpp` | FPV Channel Noise page and the long-term statistics |
| `Page Quietest.cpp` | Quietest Channels, Quietest for (video system) and IMD-Free Channels pages |
| `font5x7.h` | 5x7 bitmap font |

To change the splash picture, replace `Splash.PNG` (320 x 320) and run
`python "tools/Make Splash Header.py" "FPV Channel Scanner/Splash.PNG"
"FPV Channel Scanner/Splash Image.h"` (needs Pillow).

Each source file's header carries the revision number; see `CHANGELOG.md`
for what changed in each revision.

## Screens
Pages in KEY_R order: Activity -> Band Scan -> Spectrum -> Noise -> Quietest (or Quietest for the video system)
-> IMD-Free -> Activity. KEY_L goes back: Activity -> Noise -> Spectrum -> Band Scan -> Activity, and
IMD-Free -> Quietest (or Quietest for the video system) -> Noise. With a video system selected (see
below) its Quietest page replaces Quietest Channels; otherwise only Quietest Channels is shown. Quietest Channels is only reached with KEY_R from Channel Noise, and
only once there are 60 s of noise history; before that a "Wait NNs"
pop-up shows and the page stays put.

Every page shows its button hints on the bottom line (key names in white,
what they do in blue), in key order: "Left:"
and "Right:" name the page each key goes to, and "ENTER:" says what a short
press does there. Just above them, "Long press ENTER: ..." says what
a long press does (Re-learn floor on the scanner pages, Save / Discard in
Configuration).
- **FPV Channel Activity** (grid): all 48 channels; square brightness = power
  above the noise floor right now. In HDZero mode only channels an HDZero VTX
  can use are drawn; the others are left blank (same on
  the Noise grid, whose shading and ranks then cover only those channels).
- **Band Scan**: the whole band in one sweep, like the ArduVidRx band
  scanner. It measures every channel frequency plus fill-in points between
  them (one to three, depending on the gap) and 37 MHz past each end, 91
  points from 5325 to 5982 MHz, and draws them as bars in 10 MHz bins (in the same graph box as the
  Spectrum page, with the same 31-sweep waterfall under it and the MHz
  scale below that) on a
  5300-6000 MHz axis. With "Max MHz" set, the axis ends there instead and
  the bars widen to fill the graph. Bars at or above "Busy level" (6 dB by
  default, the same setting as the Noise and Quietest pages) are red when
  the bar's point is a channel frequency (a fill-in point between channels
  stays green), the rest green, both brighter the stronger they are. With a
  video system selected, its channels are measured at their own
  frequencies (replacing FPV channels within 6 MHz of them), only they turn
  red, and labels and the Peak line use their names (CH4, 153); the strongest bar is
  yellow (the footer colour) and the footer names it, e.g. "Peak: F1 5740
  MHz 22 dB". Every bar on a channel frequency at or above Busy level (the
  red bars, and the yellow one when it's on a channel) gets a label; peaks
  between channels don't. Labels are one line of text turned 90
  degrees (reading upward): the channel ("F1", the default), the frequency
  ("5740") or both ("F1 5740"), chosen by "Peak labels" on the Display
  page. Each label is
  centred on its bar. Adjacent labels step upward from the lowest bar to
  the highest: each goes just above its bar or above the label already
  there, with a dotted line down to its bar when lifted, so adjacent busy
  channels are all named and every label stays in line with its bar. When
  the top of the graph is full, the label goes in the first free spot
  below (over the bar). The graph height works like the spectrum's:
  the span (bars above it clip), or auto range with "Auto range" On. The
  floor comes from the shared floor (see below).
- **Spectrum**: continuous sweep of one band in 4 MHz steps (outer channels
  20 MHz beyond the band), with the point nearest each channel moved onto the
  channel's exact frequency, so every channel is measured directly. Channel numbers sit under faint marker lines; white
  dots are a falling peak hold. A 31-pixel waterfall in a white box under the
  graph shows the last 31 sweeps, newest on top (dark blue = weak, bright yellow =
  strong). Each channel's dB above the floor is printed
  under its number (blank below 3 dB, strongest in amber). The graph's
  height is the span (stronger signals are cut off at the top). With
  "Auto range" On (Auto Range page, default On; also used by Band Scan and the Activity
  grid's brightness scale; the footer then shows the current scale, e.g.
  "Span auto 70 dB (min 50)") it
  ranges itself instead: at
  least the span, and when a signal goes higher the top jumps to fit it
  (rounded up to 10 dB; the axis label shows it), then steps back down
  "Drop step" (default 10 dB) at a time once "Hold time" (default 5 s) has
  passed with no higher peak, never below the span (both on the Auto Range page). The waterfall always uses
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
- **Quietest for HDZero / DJI / Walksnail / OpenIPC**: the same list, limited
  to the selected video system's channels and showing the system's own
  channel names and frequencies (e.g. "CH3 5741" for DJI, "149 5745" for a
  Wi-Fi channel). Only with a video system selected, where it replaces
  Quietest Channels (reached with KEY_R from Channel Noise, after the same
  wait for history).

- **IMD-Free Channels**: four columns, for 3, 4, 5 and 6 pilots, each
  listing the channel set (band / channel only) with the least
  intermodulation (IMD) interference. Two transmitters on f1 and f2 make
  interference at 2*f1 - f2 and 2*f2 - f1; sets are scored like ET's
  IMDTabler (100 = no product within 35 MHz of a channel in use), and the
  score is shown under each column. Channels are at least "Min spacing"
  apart; among the best sets, the quietest in the noise history wins
  ("Quietest first"). While the sets are held, a long press of ENTER
  switches to "Best IMD first": the best IMD score always wins, even if
  that means a noisier channel, and noise only decides between equal
  scores. A channel in that set that is noisier than the one Quietest first
  would have used is shaded from yellow toward red (full red at "Busy
  level" above it). A second long press switches back; the choice lasts
  until then or a restart, and the footer shows which is in use. With
  a video system selected only its channels are used (named and spaced by
  the system's own frequencies), and the footer says which, e.g. "DJI O4
  FCC only: 7 ch, 5669-5876 MHz".

**Video system:** the "Video system" setting (Video System page) is
(All bands/chans: all 48 channels, the default), HDZero, DJI, Walksnail or
OpenIPC. Anything else enables that system's Quietest page, draws only its channels on
the grid pages, shows a blue marker at the top left of the grids, Band Scan
and Spectrum and next to the IMD-Free title (HDZ, DJI, WS, IPC), limits the
IMD-Free sets to its channels and the Spectrum band buttons to bands holding
them, and makes Band Scan name and colour only its channels. The scanner measures the 48 analog
frequencies, so a system channel that isn't on one uses the nearest
measured channel within 6 MHz (e.g. DJI O3 5669 MHz uses E3 5665).

| System | Channels (20 MHz) | Settings |
|---|---|---|
| HDZero | R1-R8, E1, F1, F2, F4, L1-L8 ([HDZero VTX docs](https://docs.hd-zero.com/vtx-general)) | HDZ low band, HDZ E1 and F1 |
| DJI V1 / Vista | stock CH3-5, CH8: 5735, 5770, 5805, 5839; FCC CH1-8 adds 5660, 5695, 5878, 5914 | DJI model, FCC channels |
| DJI O3 | stock 5769, 5805, 5840; FCC adds 5669, 5705, 5876, 5912 (CH1-7 in frequency order) | DJI model, FCC channels |
| DJI O4 | stock 5769, 5790, 5815; FCC adds 5669, 5705, 5741, 5876 (CH1-7 in frequency order) | DJI model, FCC channels |
| DJI O4 race | R1-R8 | DJI model |
| Walksnail Avatar | as DJI V1 (stock or FCC); race mode R1-R8 | FCC channels, WS race mode |
| OpenIPC (also OpenHD, Ruby) | 5 GHz Wi-Fi channels within 6 MHz of a measured channel: 116, 124, 128, 132, 136, 140, 144, 149-177 (5580-5885 MHz) | none |

DJI and Walksnail channel sets are from the
[Skwad FPV frequency guide](https://skwad.atxfpv.org/freq-guide.html); DJI
doesn't publish them, and the O3/O4 channel numbers here simply count up in
frequency order. 40 and 60 MHz DJI modes aren't covered.

## Configuration
At power-on a splash screen shows for 5 seconds while the radio starts. Press
KEY_L and KEY_R together during it to open the Configuration menu; otherwise
the page shown last before power-off or restart follows (the Activity grid the
first time, or always with "Last page" Off), on the same spectrum band. You can also open the menu at any time by holding
KEY_L and KEY_R together for 5 seconds on any page.

The menu opens on a Help page that lists what each button does on each
screen. After it come the settings, grouped by job: video system first,
then radio, scanning and channel selection, with the display pages
(display, controls) last, each page titled on a blue
bar ("Radio Page", "Display Page", ...), then a Save / Discard page. The "Configuration" title is red:

| Page | Settings |
|---|---|
| Video System | Video system (All bands/chans, HDZero, DJI, Walksnail, OpenIPC); then only the selected system's settings: HDZ low band, HDZ E1 and F1; DJI model (V1/Vista, O3, O4, O4 race); WS race mode; FCC channels (DJI / Walksnail, not with a race mode) |
| Radio | RSSI only, Tuning (Direct, Wi-Fi, Wi-Fi+chan), RX filter (Auto, or 11-23 MHz), Max MHz (Off, or 5800-5995: channels and spectrum points above it are not measured), measurement timeout (ms), 802.11p mode |
| Scanning | Samples per channel (grids), learn sweeps, spectrum step (MHz), spectrum samples per point |
| Channel Selection | Busy level (dB); minimum history before Quietest opens (s); L Band Limit (1-8): most L-band channels in the Quietest lists and in each IMD-free set (8 = no limit); IMD min spacing (MHz), IMD use L band, IMD refresh (s) |
| Display | Span (dB), Band Scan peak labels (Band, MHz, Both), Spectrum peak decay and waterfall log curve (0 = linear), Noise shading gamma and full-red spread (dB), Quietest / IMD moved-channel fade (ms, 0 = off) |
| Auto Range | Auto range On/Off (default On; Activity grid, Spectrum and Band Scan: the scale grows to fit peaks), Hold time (s with no higher peak before the scale drops, default 5) and Drop step (dB per drop, default 10; shown only with Auto range On) |
| Controls | Long press, Configuration hold, splash time, splash combo (ms); Last page (start on the last page shown, default On); pop-up time, scanning-dot step (ms), backlight (%) |

- **KEY_L / KEY_R**: move up / down the list (past the end of a page goes to
  the next or previous page).
- **ENTER**: flip an On/Off setting, or start editing a number. While
  editing, KEY_L / KEY_R step it down / up (hold to repeat) and ENTER sets it.
- **Long press ENTER**: jump straight to the Save / Discard page.
- **Save and restart** writes everything to flash; **Discard and restart**
  forgets the changes; **Reset to defaults** loads the defaults (then Save).

## Buttons
<p align="center"><img src="docs/images/Button%20Layout.png" width="720" alt="Front view of the board with arrows to the Left (KEY_L), ENTER (BOOT) and Right (KEY_R) buttons"></p>

The three buttons are on the side edges of the board, behind the screen:
Left (KEY_L) and ENTER (BOOT) on the left edge, Right (KEY_R) on the right
edge above PWR (power, not used by the scanner). USB-C is on the bottom edge.

- **KEY_R / KEY_L**: next / previous page (order above), when released.
- **KEY_L + KEY_R held 5 s**: open the Configuration menu from any page.
- **ENTER (short)** on the grid: cycle the span (25-50 dB in 5 dB steps, default 25). It sets
  the grid brightness scale and the spectrum's graph height (minimum with Auto range) and
  waterfall scale, and is remembered
  across power cycles.
- **ENTER (short)** on Channel Noise or a Quietest page: clear the noise history.
  (Re-learning the noise floor also clears it.)
- **ENTER (short)** on IMD-Free with no set in any column: **Help**, a box
  saying which Configuration settings to change to get sets (FCC channels,
  DJI model O4 race, WS race mode, HDZ low band, IMD min spacing ...);
  ENTER again closes it.
- **ENTER (short)** on IMD-Free: hold the channel sets (they stop being
  re-picked and "Hold" flashes under the table); ENTER again releases them.
  The hold is kept when you go to another page and come back.
- **ENTER (short)** on the spectrum: select the next band button. With a
  video system selected, bands with none of its channels are drawn darker
  grey and skipped.
- **ENTER (short)** on Band Scan: hold / release the scan (a red "Hold"
  flashes in place of the Peak line, as on the IMD-Free page). The hold is
  kept when you go to another page and come back (the held scan is shown
  again); a re-learn releases it.
- **Long press ENTER** on IMD-Free while held: switch between Quietest first
  and Best IMD first (see IMD-Free Channels above).
- **Long press ENTER (0.7 s)**: re-learn the noise floor (grid: all channels;
  spectrum: current band; Band Scan: its own points). Do it with your VTXs
  off if you can. The result replaces the shared floor at those
  frequencies.
- **Shared floor:** a floor learned on any page is kept by frequency and
  used by every other page when it is shown, so the first page to learn
  (usually the one shown at start-up) does it for all. A page learns on its
  own only on a long press of ENTER, or when more than an eighth of its
  frequencies have no floor within 40 MHz yet (e.g. the Spectrum on a band
  no page has measured, or the grid after only one Spectrum band was
  learned: L1-L8 still need one). Floors learned on the Spectrum and Band
  Scan (1 reading per point) sit a little lower than the grid's (3 per
  channel), so a grid seeded from them can show quiet channels 1-2 dB above
  the floor until it settles (it creeps up 0.01 dB a sweep).
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

## Not all ESP32-C5 boards are the same
The ESP32-C5 isn't a calibrated receiver, and boards differ, even two of the
same model bought together. Espressif only documents the C5's 5 GHz range up
to **5885 MHz**, so R8 (5917), E6 (5905), E7 (5925) and E8 (5945) are outside
it, and that is where most of the differences show up:

- **Lost sensitivity at the top of the band.** Tuned directly, one board lost
  1.5-4 dB above about 5745 MHz and another about 20 dB, and was nearly deaf by
  5900 MHz. Tuning through the Wi-Fi driver (the default) received a test
  tone at every point up to 5990 MHz on the same boards.
- **False signals at the top of the band.** One board read a steady -30 to
  -40 dBm block from about 5925 MHz up that a second board beside it didn't
  see. It looks like a busy channel that never goes away.
- **Different noise floors and stalls.** Floors and how often a reading
  stalls (with "RSSI only" Off) vary from board to board, mostly above
  5.9 GHz.

**Checking a board:** with every VTX off, re-learn the floor (long press
ENTER), then watch the Spectrum page for the E and R bands and Band Scan. A
flat raised block that never moves at the top end is a board problem, not a
signal. Then power one VTX on R8 or E6 and compare it with the same VTX on R1
from the same spot: a much weaker reading means the board is losing
sensitivity up there.

**Settings that help** (Configuration):

| Setting | Page | What to try |
|---|---|---|
| Tuning | Radio | Default Wi-Fi. If the top of the band is weak or noisy, compare Wi-Fi, Wi-Fi+chan and Direct with the same VTX in the same spot and keep the best. |
| Max MHz | Radio | Off by default. Set it just below where the board goes bad (e.g. 5915 drops R8, E7 and E8 but keeps E6). Channels above it aren't measured: they show "--", are left out of the Noise ranking, Quietest and IMD-free sets, and are greyed out on Spectrum and Band Scan. |
| RX filter | Radio | Default 16 MHz. Narrower (11-14) lowers the floor and separates neighbouring channels; Auto leaves the board's own calibration. |
| RSSI only | Radio | Default On (signal RSSI, no stalls on the boards tested). Off uses the noise-floor estimator (finer steps, but can stall above 5.9 GHz). |
| Meas timeout | Radio | Only with RSSI only Off: how long a reading may take before it counts as stalled. |
| 802.11p mode | Radio | Experimental; reported to help 5750-5990 MHz. Doubles measurement time. |
| Samples/channel | Scanning | More samples give steadier readings on a noisy board, at the cost of sweep speed. |
| Learn sweeps | Scanning | More sweeps give a steadier noise floor; always learn with VTXs off. |
| Busy level | Channel Selection | Raise it if a board's floor is jumpy and channels turn red with nothing on. |
| Video system | Video System | Picking your system limits every page to the channels it actually uses, which may avoid the top of the band altogether. |

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
simulated radio): every page, with each video system where it changes the
page: the splash, the scanner pages in Right-button order, then the
Configuration menu pages.

<table>
<tr>
<td align="center"><img src="docs/screenshots/01%20Splash.png" width="320" alt="Start-up splash (5 s)"><br>Start-up splash (5 s)</td>
<td align="center"><img src="docs/screenshots/02%20FPV%20Channel%20Activity.png" width="320" alt="FPV Channel Activity"><br>FPV Channel Activity</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/03%20FPV%20Channel%20Activity%20%28HDZero%29.png" width="320" alt="FPV Channel Activity, HDZero"><br>FPV Channel Activity, HDZero</td>
<td align="center"><img src="docs/screenshots/04%20FPV%20Channel%20Activity%20%28DJI%29.png" width="320" alt="FPV Channel Activity, DJI"><br>FPV Channel Activity, DJI</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/05%20FPV%20Channel%20Activity%20%28Walksnail%29.png" width="320" alt="FPV Channel Activity, Walksnail"><br>FPV Channel Activity, Walksnail</td>
<td align="center"><img src="docs/screenshots/06%20FPV%20Channel%20Activity%20%28OpenIPC%29.png" width="320" alt="FPV Channel Activity, OpenIPC"><br>FPV Channel Activity, OpenIPC</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/07%20Band%20Scan.png" width="320" alt="Band Scan (busy channels red, labelled)"><br>Band Scan (busy channels red, labelled)</td>
<td align="center"><img src="docs/screenshots/08%20Band%20Scan%20%28Hold%29.png" width="320" alt="Band Scan, held (ENTER)"><br>Band Scan, held (ENTER)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/09%20Band%20Scan%20%28HDZero%29.png" width="320" alt="Band Scan, HDZero"><br>Band Scan, HDZero</td>
<td align="center"><img src="docs/screenshots/10%20Band%20Scan%20%28DJI%29.png" width="320" alt="Band Scan, DJI O4 (its channel names)"><br>Band Scan, DJI O4 (its channel names)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/11%20Band%20Scan%20%28Walksnail%29.png" width="320" alt="Band Scan, Walksnail"><br>Band Scan, Walksnail</td>
<td align="center"><img src="docs/screenshots/12%20Band%20Scan%20%28OpenIPC%29.png" width="320" alt="Band Scan, OpenIPC (Wi-Fi channel numbers)"><br>Band Scan, OpenIPC (Wi-Fi channel numbers)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/13%20Spectrum%20%28Raceband%29.png" width="320" alt="Spectrum (Raceband) with waterfall"><br>Spectrum (Raceband) with waterfall</td>
<td align="center"><img src="docs/screenshots/14%20Spectrum%20%28HDZero%29.png" width="320" alt="Spectrum, HDZero (bands it can't use darker)"><br>Spectrum, HDZero (bands it can't use darker)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/15%20Spectrum%20%28DJI%29.png" width="320" alt="Spectrum, DJI"><br>Spectrum, DJI</td>
<td align="center"><img src="docs/screenshots/16%20Spectrum%20%28Walksnail%29.png" width="320" alt="Spectrum, Walksnail"><br>Spectrum, Walksnail</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/17%20Spectrum%20%28OpenIPC%29.png" width="320" alt="Spectrum, OpenIPC"><br>Spectrum, OpenIPC</td>
<td align="center"><img src="docs/screenshots/18%20FPV%20Channel%20Noise.png" width="320" alt="FPV Channel Noise"><br>FPV Channel Noise</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/19%20FPV%20Channel%20Noise%20%28HDZero%29.png" width="320" alt="FPV Channel Noise, HDZero"><br>FPV Channel Noise, HDZero</td>
<td align="center"><img src="docs/screenshots/20%20FPV%20Channel%20Noise%20%28DJI%29.png" width="320" alt="FPV Channel Noise, DJI"><br>FPV Channel Noise, DJI</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/21%20FPV%20Channel%20Noise%20%28Walksnail%29.png" width="320" alt="FPV Channel Noise, Walksnail"><br>FPV Channel Noise, Walksnail</td>
<td align="center"><img src="docs/screenshots/22%20FPV%20Channel%20Noise%20%28OpenIPC%29.png" width="320" alt="FPV Channel Noise, OpenIPC"><br>FPV Channel Noise, OpenIPC</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/23%20FPV%20Channel%20Noise%20with%20Wait%20popup.png" width="320" alt="Wait pop-up (Quietest not ready)"><br>Wait pop-up (Quietest not ready)</td>
<td align="center"><img src="docs/screenshots/24%20Quietest%20Channels.png" width="320" alt="Quietest Channels"><br>Quietest Channels</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/25%20Quietest%20for%20HDZero.png" width="320" alt="Quietest for HDZero"><br>Quietest for HDZero</td>
<td align="center"><img src="docs/screenshots/26%20Quietest%20for%20DJI.png" width="320" alt="Quietest for DJI"><br>Quietest for DJI</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/27%20Quietest%20for%20Walksnail.png" width="320" alt="Quietest for Walksnail"><br>Quietest for Walksnail</td>
<td align="center"><img src="docs/screenshots/28%20Quietest%20for%20OpenIPC.png" width="320" alt="Quietest for OpenIPC"><br>Quietest for OpenIPC</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/29%20IMD-Free%20Channels.png" width="320" alt="IMD-Free Channels"><br>IMD-Free Channels</td>
<td align="center"><img src="docs/screenshots/30%20IMD-Free%20Channels%20%28Hold%2C%20Best%20IMD%20first%29.png" width="320" alt="IMD-Free Channels, held, Best IMD first (noisier channels shaded red)"><br>IMD-Free Channels, held, Best IMD first (noisier channels shaded red)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/31%20IMD-Free%20Channels%20%28HDZero%29.png" width="320" alt="IMD-Free Channels, HDZero"><br>IMD-Free Channels, HDZero</td>
<td align="center"><img src="docs/screenshots/32%20IMD-Free%20Channels%20%28DJI%29.png" width="320" alt="IMD-Free Channels, DJI (FCC channels)"><br>IMD-Free Channels, DJI (FCC channels)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/33%20IMD-Free%20Channels%20%28DJI%20stock%2C%20no%20set%29.png" width="320" alt="IMD-Free Channels, DJI O4 stock: no set, ENTER = Help"><br>IMD-Free Channels, DJI O4 stock: no set, ENTER = Help</td>
<td align="center"><img src="docs/screenshots/34%20IMD-Free%20Channels%20%28no%20set%20Help%29.png" width="320" alt="IMD-Free no-set Help box"><br>IMD-Free no-set Help box</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/35%20IMD-Free%20Channels%20%28Walksnail%29.png" width="320" alt="IMD-Free Channels, Walksnail"><br>IMD-Free Channels, Walksnail</td>
<td align="center"><img src="docs/screenshots/36%20IMD-Free%20Channels%20%28OpenIPC%29.png" width="320" alt="IMD-Free Channels, OpenIPC"><br>IMD-Free Channels, OpenIPC</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/37%20Configuration%20-%20Help.png" width="320" alt="Configuration: Help (opens first)"><br>Configuration: Help (opens first)</td>
<td align="center"><img src="docs/screenshots/38%20Configuration%20-%20Video%20System.png" width="320" alt="Configuration: Video System (All bands/chans)"><br>Configuration: Video System (All bands/chans)</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/39%20Configuration%20-%20Video%20System%20%28HDZero%29.png" width="320" alt="Configuration: Video System, HDZero"><br>Configuration: Video System, HDZero</td>
<td align="center"><img src="docs/screenshots/40%20Configuration%20-%20Video%20System%20%28DJI%29.png" width="320" alt="Configuration: Video System, DJI"><br>Configuration: Video System, DJI</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/41%20Configuration%20-%20Video%20System%20%28DJI%20O4%20race%29.png" width="320" alt="Configuration: Video System, DJI O4 race"><br>Configuration: Video System, DJI O4 race</td>
<td align="center"><img src="docs/screenshots/42%20Configuration%20-%20Video%20System%20%28Walksnail%29.png" width="320" alt="Configuration: Video System, Walksnail"><br>Configuration: Video System, Walksnail</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/43%20Configuration%20-%20Video%20System%20%28OpenIPC%29.png" width="320" alt="Configuration: Video System, OpenIPC"><br>Configuration: Video System, OpenIPC</td>
<td align="center"><img src="docs/screenshots/44%20Configuration%20-%20Radio.png" width="320" alt="Configuration: Radio"><br>Configuration: Radio</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/45%20Configuration%20-%20Scanning.png" width="320" alt="Configuration: Scanning"><br>Configuration: Scanning</td>
<td align="center"><img src="docs/screenshots/46%20Configuration%20-%20Channel%20Selection.png" width="320" alt="Configuration: Channel Selection"><br>Configuration: Channel Selection</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/47%20Configuration%20-%20Display.png" width="320" alt="Configuration: Display"><br>Configuration: Display</td>
<td align="center"><img src="docs/screenshots/48%20Configuration%20-%20Auto%20Range.png" width="320" alt="Configuration: Auto Range"><br>Configuration: Auto Range</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/49%20Configuration%20-%20Controls.png" width="320" alt="Configuration: Controls"><br>Configuration: Controls</td>
<td align="center"><img src="docs/screenshots/50%20Configuration%20-%20Save%20-%20Discard.png" width="320" alt="Configuration: Save / Discard"><br>Configuration: Save / Discard</td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/51%20Configuration%20-%20editing%20a%20value.png" width="320" alt="Configuration: editing a value"><br>Configuration: editing a value</td>
</tr>
</table>
