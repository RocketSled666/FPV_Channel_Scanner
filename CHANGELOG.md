# Changelog

Revision numbers match the `Revision:` line in the source headers.

## Revision 41
- The pop-up shown when Quietest Channels isn't available yet now reads
  "Wait >60s" instead of "Wait longer" (the number follows
  QUIET_MIN_HISTORY_S).

## Revision 40
- The span (grid brightness / spectrum height) is saved in flash when it is
  changed and restored at power-on, instead of resetting to 15 dB.
- HDZero outlines: confirmed they are drawn only in HDZero mode.

## Revision 39
- The start-up HDZero answer is saved in flash (Preferences) and becomes the
  default next time: with no press during the countdown, the saved answer is
  used. The default box is marked and the countdown says what will happen.
  Saved only when the answer changes.

## Revision 38
- HDZero mode is now chosen from a start-up question, "Enable HDZero
  specifics?": KEY_R = Yes, KEY_L = No. With no press it starts normally (No)
  after a 5 second on-screen countdown. Replaces holding KEY_L at power-on.

## Revision 37
- HDZero features are now optional: hold KEY_L while powering on to enable
  HDZero mode (the Quietest for HDZero page, blue outlines on HDZero-usable
  channels, and a blue "HDZ" marker on the grid pages). Without it, KEY_R
  from Quietest Channels returns to Activity. A press just after power-on,
  during start-up, also counts.

## Revision 36
- On the Activity and Noise grids, channels an HDZero VTX can use (R1-R8,
  E1, F1, F2, F4, L1-L8) have a blue outline. The white highlight stays
  outermost when both apply; frequency labels moved up 1 px to clear it.

## Revision 35
- New "Quietest for HDZero" page after Quietest Channels: the same ranked
  list, limited to channels an HDZero VTX can use (R1-R8, E1, F1, F2, F4,
  and L1-L8 with low band unlocked), per the HDZero VTX documentation.
  Reached only with KEY_R from Quietest Channels; KEY_L goes back to it.

## Revision 34
- Split the single sketch into modules for easier maintenance: the .ino
  now holds only `setup()` and `loop()`; config, shared declarations, state,
  display, input, radio, bands, shared screen code and one file per page.
- No behavior changes.

## Revision 33
- Lowercase letters added to font5x7.h; all on-screen text uses normal capitalization ("dB", "MHz", "Scanning", ...). Band/channel codes such as "R4" stay in capitals. Band order re-checked on every page

## Revision 32
- Pop-up area is cleared to black before the grid is redrawn, so no yellow border is left in the gaps between boxes

## Revision 31
- Page order Activity -> Spectrum -> Noise -> Quietest. Quietest is reached only by KEY_R from Noise, and only after 60 s of noise history (otherwise a "WAIT LONGER" pop-up). KEY_L from Activity goes to Noise

## Revision 30
- New QUIETEST CHANNELS page after Channel Noise: the 16 quietest channels from the noise history, listed quietest first

## Revision 29
- Rank numbers 1.5x size (was 2x) and light grey instead of white

## Revision 28
- ACT_GAMMA 0.4 -> 0.55

## Revision 27
- Noise page ranks all 48 channels from quietest (1) to busiest (48) and shows the rank inside each box, above the frequency

## Revision 26
- Noise page shading uses a gain curve (gamma 0.4) so small differences between channels show as clearly brighter red; full-red spread 0.5 dB

## Revision 25
- Noise page keeps long-term statistics for every channel (average dB above floor, % of time busy, peak) since start / last reset; shading and the quietest pick use the long-term average. Footer shows how much history has been collected

## Revision 24
- Noise page: noisy channels rise in ~15 s but fade over ~60 s, so a busy channel stays red after the signal stops

## Revision 23
- Noise page tracks each channel's average signal over time and shades quietest (black) to busiest (bright red); no average subtraction. Page titles: "FPV Channel Activity" (grid), "FPV Channel Noise"

## Revision 22
- ACTIVITY averaging window shortened to 15 seconds

## Revision 21
- ACTIVITY highlights and names the quietest channel instead of the busiest; averaging window is 60 seconds (time-based, not sweeps)

## Revision 20
- ACTIVITY page: per-box moving sum of signal above the floor, minus the average over all boxes, shown black -> red. KEY_R / KEY_L step forward / back through GRID -> ACTIVITY -> SPECTRUM

## Revision 19
- Grid rows use the same band order as the spectrum buttons (L E R A B F)

## Revision 18
- "NO SIGNAL" replaced by "SCANNING" with animated . .. ... dots (only the dots are redrawn)

## Revision 17
- Peak-hold trace falls 3x faster (0.6 -> 1.8 px per sweep)

## Revision 16
- Spectrum band buttons ordered by increasing start frequency (L E R A B F); ENTER steps through them left to right

## Revision 15
- Spectrum bands use the grid's learned floor directly - no second learning pass. Re-learning the grid also refreshes the spectrum.

## Revision 14
- SPAN label on the spectrum page in the same spot as the grid page, and kept visible on both pages while learning; removed the "ENTER: BAND" hint from the spectrum page; USB serial can no longer slow the scanner when nothing on the PC is reading it (non-blocking writes, 4 KB buffer, periodic dumps skipped when the buffer is full)

## Revision 13
- Learning shows a 0-100% readout and progress bar in the footer

## Revision 12
- Spectrum graph height follows the grid's span setting (default 15 dB)

## Revision 11
- Measurement timeout 10 -> 5 ms; a stalled measurement falls back to the receiver RSSI register; per-frequency stall report on serial; spectrum shows each channel's dB under its number; '+' dropped

## Revision 10
- Spectrum floor seeded from the learned grid floor (interpolated), so the trace shows at once; LEARNING still shown while it refines

## Revision 9
- Speed: 1 measurement per point (each is a fixed ~3.2 ms hardware window), no discarded sample, 4 MHz spectrum steps

## Revision 8
- Revision number added to header

## Revision 7
- Learning cut to 4 sweeps; timing breakdown on serial

## Revision 6
- Spectrum scale fixed at 0-15 dB; include font5x7.h

## Revision 5
- KEY_L / KEY_R switch pages; ENTER selects band / span / re-learn

## Revision 4
- Spectrum sweep page with band buttons; accelerometer tap to switch

## Revision 3
- Files renamed with spaces

## Revision 2
- Fix IDE auto-prototype error (RGB type moved up, no default args)

## Revision 1
- Initial grid scanner
