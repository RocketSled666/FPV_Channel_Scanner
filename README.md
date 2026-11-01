# FPV Channel Scanner — Waveshare ESP32-C5-LCD-2.73

Scans the 48 analog FPV channels (bands A, B, E, F, Raceband, Lowband) with the
ESP32-C5's own 5 GHz radio and shows them as a 6x8 grid. Each square's
brightness is the channel's power above its learned noise floor. The strongest
channel gets a white border and is named in the footer.

## Building
The sketch is in the `FPV Channel Scanner/` folder (the folder name must match
the `.ino`).

## Arduino IDE setup
- Boards Manager: **esp32 by Espressif, 3.3.x** (compiled against 3.3.12)
- Board: **ESP32C5 Dev Module**
- USB CDC On Boot: **Enabled** (to see the serial readout over USB-C)
- Everything else can stay at defaults. No extra libraries needed.

## Screens
Pages in KEY_R order: Activity -> Spectrum -> Noise -> Quietest -> Activity.
KEY_L goes back: Activity -> Noise -> Spectrum -> Activity, and Quietest ->
Noise. Quietest Channels is only reached with KEY_R from Channel Noise, and
only once there are 60 s of noise history; before that a "Wait longer"
pop-up shows and the page stays put.
- **FPV Channel Activity** (grid): all 48 channels; square brightness = power
  above the noise floor right now.
- **Spectrum**: continuous sweep of one band in 4 MHz steps (outer channels
  20 MHz beyond the band). Channel numbers sit under faint marker lines; white
  dots are a falling peak hold. Each channel's dB above the floor is printed
  under its number (blank below 3 dB, strongest in amber). Band buttons
  L E R A B F sit under the graph.
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

## Buttons
- **KEY_R / KEY_L**: next / previous page (order above).
- **ENTER (short)** on the grid: cycle the span (15-40 dB, default 15). It sets
  the grid brightness scale and the spectrum graph height.
- **ENTER (short)** on Channel Noise or Quietest Channels: clear the history
  and start over.
  (Re-learning the noise floor also clears it.)
- **ENTER (short)** on the spectrum: select the next band button.
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
- The spectrum tunes in 4 MHz steps, off the normal 5 MHz Wi-Fi grid. If the
  synthesizer can't land on those exactly, the trace will look stepped; set
  `SPEC_STEP_MHZ` to 5.
- These PHY calls and registers are undocumented, found by disassembling
  Espressif's libphy.a. A future core update could move or change them.
- Readings are relative dB, not calibrated dBm.
- With a 20 MHz receive bandwidth, channels closer than ~20 MHz (e.g. A8 5725 /
  B1 5733 / R3 5732) will light up together. That's the radio, not a bug.
- RF calibration outside the Wi-Fi channel plan (L band, E7/E8 above 5925 MHz)
  is interpolated by the PHY, so sensitivity there may be lower.
