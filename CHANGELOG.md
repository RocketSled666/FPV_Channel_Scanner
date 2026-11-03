# Changelog

Revision numbers match the `Revision:` line in the source headers.

## Revision 74
- HDZero mode, Spectrum page: band buttons for bands with no HDZero channel
  (A and B, plus L when "Low band L1-L8" is off and E when "E1 and F1" is
  off) are drawn in a darker grey and ENTER skips them. If the spectrum was
  on such a band, it moves to the next enabled one.

## Revision 73
- The blue HDZero outline is no longer drawn around grid boxes. In HDZero
  mode the grids already show only HDZero channels, so it added nothing.
  The white highlight (strongest / quietest channel) is unchanged.

## Revision 72
- HDZero mode, FPV Channel Activity and FPV Channel Noise grids: channels an
  HDZero VTX can't tune (following the HDZero settings for L1-L8 and E1/F1)
  are left blank, and a band with no usable channels (A, B) loses its
  letter too. On Activity the strongest-signal highlight and footer only
  consider drawn channels; on Noise the black-to-red shading, the rank
  numbers (1 = quietest drawn channel) and the outlined quietest channel do
  too. Measurement still covers all 48 channels.

## Revision 71
- L Band Limit now ranges 1-8; 0 is gone, since turning L band off is what
  "Use L band" (IMD-Free) and "Low band L1-L8" (HDZero) are for. A saved 0
  loads as 1.

## Revision 70
- New "L Band Limit" setting (0-8, default 8 = no limit) on the Quietest
  Channels Configuration page: the most L-band channels allowed in the
  Quietest Channels and Quietest for HDZero lists (further L channels are
  skipped and the next quietest non-L channels fill the list) and in each
  IMD-Free set (3, 4, 5 and 6 pilots each separately).

## Revision 69
- The pop-up shown when Quietest can't open yet now gives the time still to
  wait, e.g. "Wait 23s", instead of "Wait >60s".

## Revision 68
- New splash screen picture: Splash.PNG replaced with the new 320 x 320
  artwork and "Splash Image.h" regenerated from it with
  tools/Make Splash Header.py.

## Revision 67
- New "User Interface" Configuration page (between IMD-Free Channels and
  General) for timings that were compile-time constants, all kept in flash:
  Long press (700 ms), Config hold (5000 ms), Splash time (5000 ms, 0 = no
  splash), Splash combo (80 ms), Pop-up time (1500 ms), Scan dot step
  (400 ms), IMD refresh (10 s) and Move fade (3000 ms, 0 = off). The page has
  8 settings, so its rows are closer together (24 px instead of 30).
- The splash hint and the Help page now show the configured hold times
  ("hold for >5s", "Hold ENTER 0.7s") instead of fixed text.
- In the Configuration menu, holding ENTER for the long-press time jumps to
  the Save / Discard page (with "Save and restart" selected). A short ENTER
  press now acts on release. Help page lists "Hold ENTER: go to Save /
  Discard".

## Revision 66
- Save / Discard page: the "Revision / Built" line is now on the very bottom
  line of the screen, in a darker grey, with two blank lines between it and
  "Changes apply after Save and restart". The footer on that page sits
  16 px higher to make room.

## Revision 65
- The Save / Discard page in Configuration shows "Revision 65   Built
  YYYY-MM-DD HH:MM:SS" above the help line. The revision comes from the new
  FW_REVISION in Config.h (keep it equal to the header "Revision:" lines);
  the date and time are when Config Menu.cpp was compiled.

## Revision 64
- HDZero mode shows only the Quietest for HDZero list: it replaces Quietest
  Channels in the page order (KEY_R: ... Noise -> Quietest for HDZero ->
  IMD-Free -> Activity), with the same "Wait >60s" check for history. With
  HDZero mode off, only Quietest Channels is shown, as before. Help page
  page order follows the mode.

## Revision 63
- Page order: Quietest for HDZero now comes before IMD-Free Channels.
  KEY_R: ... Noise -> Quietest -> Quietest for HDZero (HDZero mode) ->
  IMD-Free -> Activity; KEY_L goes back the same way. The Configuration
  pages follow the same order, and the Help page shows it.
- IMD-Free Channels in HDZero mode: the footer reads "HDZero only: R1-8, ..."
  so it's clear the sets use HDZero channels only.

## Revision 62
- ENTER (short) now does nothing on Quietest Channels, Quietest for HDZero
  and IMD-Free Channels; it clears the noise history only on FPV Channel
  Noise. Help page updated ("Noise: clear the noise history"). Holding
  ENTER still re-learns the noise floor on every page.

## Revision 61
- Page titles in green (instead of amber) on FPV Channel Activity, Quietest
  Channels, IMD-Free Channels and Quietest for HDZero. FPV Channel Noise
  stays red and Spectrum stays amber.

## Revision 60
- The history clock at the bottom right of the Noise, Quietest and IMD-Free
  pages now reads "History D:HH:MM:SS" (for example 0:01:05:09).
- Quietest Channels, Quietest for HDZero and IMD-Free Channels: when a
  channel changes position, its band / channel label turns red and fades
  back to its normal amber over 3 seconds (MOVE_FADE_MS). The first fill of
  a list and a cleared history don't count as moves.

## Revision 59
- Configuration menu hint line (bottom of every page): key names stay
  white, each key's action ("Up", "Back", "Modify Value", ...) is green.

## Revision 58
- Help page: the action for each button (right-hand column) is shown in
  green instead of grey.

## Revision 57
- New IMD-Free Channels page, after Quietest Channels (KEY_R order: ...
  Quietest -> IMD-Free -> Quietest for HDZero / Activity). Four columns, for
  3, 4, 5 and 6 pilots, each listing the channel set (band / channel only)
  with the least intermodulation interference.
- Scoring follows ET's IMDTabler: two transmitters on f1 and f2 make
  products at 2*f1 - f2 and 2*f2 - f1; each product within 35 MHz of a
  channel in the set costs (35 - distance)^2, and the score is
  100 - total / 5 / channels (100 = clean). The score is shown under each
  column (green 100, amber 80-99, red below 80).
- Channels in a set are at least "Min spacing" apart (default 37 MHz).
  Among the best-scoring sets, the one on the quietest channels in the noise
  history wins (2 points per dB of average noise), so busy channels are
  avoided; the sets are re-picked every 10 s.
- HDZero mode: only channels an HDZero VTX can use, following the HDZero
  settings (L1-L8, E1/F1). Otherwise L1-L8 only with the new "Use L band"
  setting (default off).
- New Configuration page "IMD-Free Channels" with "Min spacing" and
  "Use L band". ENTER on the page clears the noise history, as on the
  Quietest pages. Help page updated.

## Revision 56
- Spectrum band buttons are square-cornered again (43 x 19 px, large letter)
  and spread evenly so the row lines up exactly with the left and right
  edges of the graph box. Selected band still green, others greyed out.

## Revision 55
- White 1-pixel box around the spectrum waterfall.
- Waterfall 31 pixels high (6 more rows), so it shows the last 31 sweeps.
- Band circles 19 px across instead of 15. The selected band is solid green
  with a black letter; the others are greyed out (dark grey outline, grey
  letter).

## Revision 54
- Spectrum band circles moved to the top of the page, just under the title.
  The graph (same 186 px height), waterfall and channel numbers / values
  moved down below them.
- Waterfall 25 pixels high, so it shows the last 25 sweeps.

## Revision 53
- Spectrum band buttons moved down 5 pixels, made half as tall (15 px instead
  of 30) and drawn as circles 15 px across, with a 1 px outline and the
  letter in the small font.
- The waterfall uses the freed space: 15 pixels, so the last 15 sweeps.

## Revision 52
- Spectrum page: the graph is 14 pixels shorter (186 instead of 200) to make
  room for a 10-pixel waterfall directly under it. Each sweep adds one pixel
  row at the top and pushes the older ones down, so the last 10 sweeps are
  visible. Colour shows the signal above the floor on the same span as the
  graph: dark blue for weak, through purple and orange, to bright yellow for
  strong. The history clears when the band changes.

## Revision 51
- Configuration hints say "Back" / "Forward" instead of "Page Back" /
  "Page Fwd" (Help page: "Left: Save Changes  Right: Forward  ENTER: Forward").

## Revision 50
- Configuration hint lines and the Help page call the keys "Left:",
  "Right:" and "ENTER:" (with a colon) instead of KEY_L / KEY_R / ENTER.

## Revision 49
- Configuration hints say "Page Back" / "Page Fwd" instead of "Prev Page" /
  "Next Page" (Help page: "KEY_L Save Changes  KEY_R Page Fwd  ENTER Page Fwd").

## Revision 48
- Configuration hint line reworded: KEY_L shows "Prev Page" when the top line
  of a page is selected (otherwise "Up"), KEY_R shows "Next Page" on the last
  line (otherwise "Down"), and ENTER shows "Modify Value" on a setting
  ("Select" on Save / Discard, "-", "+" and "Set" while editing).
- Help page hint: "KEY_L Save Changes  KEY_R Next Page  ENTER Next Page".
  KEY_L on the Help page now goes straight to "Save and restart".

## Revision 47
- Splash hint now reads "Press forward+backward buttons now for / Settings
  screen, or hold for >5s at any time".
- The Configuration menu opens on a new Help page listing what each button
  does on each screen (all screens, ENTER per screen, page order, and the
  menu itself). KEY_R or ENTER goes on to the first settings page; KEY_L
  wraps to Save / Discard.

## Revision 46
- Start-up splash screen from Splash.PNG, shown for 5 seconds while the radio
  starts, with a blue hint at the bottom: "Press forward+backward buttons now
  for Settings screen". Pressing KEY_L and KEY_R together during the splash
  opens the Configuration menu.
- The picture is stored in "Splash Image.h" as a 256-colour palette plus one
  index byte per pixel (about 100 KB of flash). tools/Make Splash Header.py
  regenerates it from Splash.PNG.

## Revision 45
- Removed the start-up "Configuration?" prompt and its "Startup wait"
  setting; start-up goes straight to the Activity grid. The Configuration
  menu is opened only by holding KEY_L + KEY_R for 5 seconds.
- Each Configuration page is titled like the screen it configures (FPV
  Channel Activity, Spectrum, FPV Channel Noise, Quietest Channels, Quietest
  for HDZero, plus General), shown on a blue bar. "unsaved" and the page
  counter moved to the top line.

## Revision 44
- Holding KEY_L and KEY_R together for 5 seconds opens the Configuration menu
  from any page (it shows "Release the buttons" first).
- KEY_L / KEY_R now change page when released instead of when pressed, so
  pressing both together never changes page.

## Revision 43
- Start-up "Configuration?" prompt (5 s countdown, any button) replaces the
  HDZero question. The Configuration menu lists every setting, one page per
  screen, then a Save / Discard page; both restart the device.
- Menu controls: KEY_L / KEY_R move up / down, ENTER flips On/Off settings or
  edits numbers (KEY_L / KEY_R step them, hold to repeat, ENTER sets).
- Settings (all kept in flash): span, learn sweeps, samples per channel,
  spectrum step, samples per point, peak decay, busy level, shading gamma,
  full-red spread, minimum history for Quietest, HDZero features, HDZero low
  band and E1/F1, backlight, start-up wait. Existing saved span and HDZero
  choice carry over.
- New files: Settings.cpp, Config Menu.cpp.

## Revision 42
- Spectrum: the sweep point nearest each channel is moved onto the channel's
  exact frequency, so each channel is measured directly instead of
  interpolated. Same number of points, so no change in sweep speed. The
  trace is drawn by real frequency, and the footer reports the exact
  frequency of the peak point.

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
