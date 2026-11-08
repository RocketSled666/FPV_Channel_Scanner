# Changelog

Revision numbers match the `Revision:` line in the source headers.

## Revision 96
- The page shown last is saved in flash (with the spectrum band) and shown
  again after the splash at the next start-up, instead of always the
  Activity grid. If the saved page is a Quietest page and the HDZero
  setting has changed since, the Quietest page that matches the current
  setting opens instead. A restored Quietest or IMD-Free page fills in as
  noise history builds up (the "Wait" minimum isn't applied).
- New "Restore page" setting (Controls page, default On); Off always starts
  on the Activity grid with Raceband on the spectrum.

## Revision 95
- New Band Scan page between Spectrum and Noise, modelled on the ArduVidRx
  band scanner. One sweep covers the whole band: every channel frequency
  (sorted, duplicates once) plus fill-in points between them (3 when more
  than 35 MHz apart, 2 when more than 22, 1 when more than 9) and 37 MHz
  past each end, 91 points from 5325 to 5982 MHz.
- Drawn as bars in 10 MHz bins on a 5300-6000 MHz axis (each bar is the
  strongest point in its bin), with grid lines at quarters of the range and
  the spectrum's auto range. Peaks of 5 dB or more get a frequency label
  (ArduVidRx's peak marking, at least 7 bars apart); the strongest bar is
  green and the footer names it, e.g. "Peak: F1 5740 MHz 22 dB".
- ENTER pauses / resumes the scan ("Paused" in red); holding ENTER
  re-learns the page's floor. The floor is seeded from the grid's learned
  floor, and re-seeded after the grid floor is reset. Points above
  "Max MHz" aren't measured.
- With "Max MHz" set, the graph ends at it (the bin holding Max MHz is the
  last bar) instead of 6000 MHz, and the bars widen to fill the width.
- Help page lists the Band Scan ENTER action and the new page order.

## Revision 94
- Spectrum graph height is the span again (peaks above it clip). The
  Revision 86 auto range is now the "Auto range" setting (Display page,
  default Off).

## Revision 93
- Config page titles on the blue bar end in "Page" ("Help Page", "Radio
  Page", ..., "Save / Discard Page").

## Revision 92
- Configuration pages regrouped by job instead of by screen (10 pages ->
  8): Help, Radio (RSSI only, Tuning, RX filter, Max MHz, Meas timeout,
  802.11p), Scanning (samples/channel, learn sweeps, spectrum step and
  samples), Channel Selection (busy level, min history, L Band Limit, IMD
  spacing, IMD L band, IMD refresh), HDZero, Display (span, gamma, red
  spread, peak decay, waterfall log, move fade, backlight), Controls
  (button and splash timings, pop-up, scan dots), Save / Discard.
- Labels "Spectrum step", "Spectrum samples", "IMD min spacing", "IMD use
  L band"; help lines say which screen a setting affects. Flash keys are
  unchanged, so saved settings carry over.
- "Configuration" title (menu and the release-the-buttons screen) is red.

## Revision 91
- Defaults: Tuning = Wi-Fi, RX filter = 16 MHz (were Direct and Auto).

## Revision 90
- "Busy level" default 6 dB (was 3): with RSSI only's 1 dB steps, quiet
  channels wobbled past 3 dB.
- The outline on the highlighted grid box (Activity peak, Noise quietest)
  is amber, the same colour as the footer text naming it (was white).

## Revision 89
- New "Tuning" setting (General page; Direct by default): Direct =
  phy_set_chanfreq(MHz) as before; Wi-Fi = Wi-Fi driver on channel 140 and
  libphy phy_change_channel(MHz, 1, 0, 0) per retune; Wi-Fi+chan also
  calls esp_wifi_set_channel(140) before each retune. ESPsoup measured the
  Wi-Fi path receiving up to 5990 MHz where the direct path lost up to
  ~20 dB above ~5745 MHz. Falls back to Direct if libphy lacks the call.
- New "RX filter" setting (General page; Auto by default, or 11-23 MHz):
  forces the analog filter capacitor code (BBTOP 0x67 host 1, registers
  6/7, low 6 bits) after every retune; widths from ESP-SDR / ESPsoup
  measurements. Skipped (with a log line) if libphy lacks the I2C calls.
- Boot log reports the tuning path and RX filter (calibrated register
  values and the code used); the timing test uses the selected path.

## Revision 88
- Fix: KEY_L from IMD-Free with too little noise history showed the
  "Wait NNs" pop-up, and when it went the IMD rows under it stayed black
  until the sets changed. The Quietest and IMD pages now stop drawing while
  the pop-up is up, and the IMD table is redrawn (sets and hold kept) when
  it goes.
- Fix: the floor clamp (median + 4 dB) counted unlearned floors (0, e.g.
  above "Max MHz") in the median. Real floors are negative, so those
  zeros sorted to the top and lost the "not learned" marker; on the
  spectrum, with half a band above Max MHz, the clamp stopped working.
  Grid and spectrum now use learned floors only, and the spectrum floor
  seed skips unlearned grid channels.
- Fix: the Noise page no longer overflows an int when no channel has live
  data.
- The LCD is now driven through ESP-IDF's esp_lcd SPI panel IO (hardware
  CS and DC, DMA), as in Waveshare's BSP, instead of the Arduino SPI
  library. After a flash or a USB reset (which leaves the panel powered)
  the screen stayed blank or showed garbled text and stray pixels until a
  power cycle; Waveshare's factory firmware came back, and so does this.
- A software reset (0x01) is sent after the IO-expander reset, before the
  init sequence.
- Pixels go out in chunks of up to 16 full-width rows from two buffers used
  in turn (Memory Write, then Memory Write Continue), so a buffer is never
  refilled while it is being sent. About 30 KB more RAM; grid drawing takes
  about 40 ms per sweep (4.4 sweeps/s, was 5.0) at the same 40 MHz clock.

## Revision 87
- New "Max MHz" setting (General page; Off by default, or 5800-5995 MHz in
  5 MHz steps). Frequencies above it aren't measured: grid channels show
  "--" and are left out of the Noise ranking, Quietest lists and IMD-free
  sets; on the Spectrum page those columns and waterfall cells are grey,
  the channel values show "--", and they're ignored for the peak and the
  graph range. For boards that read junk at the top of the band (one board
  showed a steady -30 to -40 dBm block from about 5925 MHz up that the
  other board didn't see); 5915 removes R8, E7 and E8 and keeps E6.

## Revision 86
- Spectrum graph auto range: the top is at least the span, jumps up at once
  to fit the highest point (rounded up to 10 dB, max 100), and steps back
  down 10 dB at a time after 5 s without needing it. Peaks no longer clip:
  with RSSI only a nearby VTX reads 55-62 dB above the floor, more than the
  largest span. The axis labels follow the range; peak-hold dots are
  rescaled when it changes. The waterfall still uses the span.

## Revision 85
- "RSSI only" is now On by default: bench logs showed live readings on
  every channel (R8, E7 and E8 included), no timeouts and about 4 grid
  sweeps/s with 3 samples per channel.
- Samples/channel default 3 (was 1). Toggling RSSI only in the menu sets it
  to 1 (Off) or 3 (On); it can still be changed after.
- Samples/channel and Samples/point help show the time per reading for the
  current method (1 ms with RSSI only, 3.3 ms without). Meas timeout help
  says it applies to noise-floor readings.

## Revision 84
- "Sig RSSI fill" replaced by "RSSI only" (FPV Channel Activity page,
  default Off; a new settings key, so an old "Sig RSSI fill = On" doesn't
  carry over). When On, every reading comes from libphy's signal RSSI
  (phy_check_sigrssi_en(1) + phy_get_sigrssi(), dBm) instead of the
  noise-floor estimator, which that mode stops on the C5. Each sample is the
  median of 5 reads 250 us apart; out-of-range reads (outside -120..-10 dBm)
  are dropped, and a channel with no sane read holds its last value like a
  stall. 1 dB resolution.
- New "rssi only" serial line (each report, when On): samples, reads out of
  range, dBm range seen and the share of samples equal to the previous one.
- Boot timing test uses whichever method is on.

## Revision 83
- While "802.11p mode" is On, the measurement timeout is fixed at 12 ms
  (802.11p roughly doubles each measurement to about 6.4 ms, so 8 ms caused
  random timeouts). The saved "Meas timeout" value is not changed: turn
  802.11p Off and it is used again. The menu shows "12 ms (11p)" on that
  row and won't let you edit it while 802.11p is On.
- "Sig RSSI fill" help now says to leave it Off: on the C5, signal-RSSI mode
  stops the noise-floor estimator, so every normal reading times out.

## Revision 82
- Removed the phy_get_rssi() stand-in for stalled readings. Two ESP32-C5
  FPV projects (FPVGateC5RX, C5VRX) found that it only updates when a Wi-Fi
  packet is received, so on quiet channels it freezes at an old value; the
  Revision 81 logs showed exactly that (offsets of -7 and -38 dB on two
  boards, fake +25 dB signals). Stalled readings hold the last good value
  again.
- New "Sig RSSI fill" setting (FPV Channel Activity page, default Off):
  switches on libphy's signal-RSSI mode (phy_check_sigrssi_en(1)), which
  measures continuously, and fills stalled readings with phy_get_sigrssi():
  the median of 5 reads 250 us apart plus an offset learned from good
  readings ("sig rssi fill" line in serial). Experimental: the mode rewrites
  some AGC registers, which might change the normal readings.
- New "802.11p mode" setting (same page, default Off): calls
  phy_11p_set(1, 0) at start-up, reported to improve reception from
  5750 to 5990 MHz. Experimental.
- Both functions are declared weak: if a core's libphy lacks one, the
  sketch still links and serial says the setting was ignored.
- "No data" now has hysteresis: a channel becomes "--" above 60% held
  readings and returns below 30%, so it doesn't flicker at the threshold.

## Revision 81
- Stalled measurements now use the receiver's RSSI register, corrected by an
  offset learned on this board: on every good measurement the RSSI is read
  too and the difference is averaged (outliers over 10 dB ignored after the
  first 50). Once 50 pairs are in, a stall gives a live reading, RSSI plus
  offset. RSSI values outside -110..-20 are treated as garbage (the likely
  source of the old 250+ dB peaks). Until calibrated, or with garbage RSSI,
  the last good reading is held as in Revision 80. Serial prints an
  "rssi cal" line with the offset.
- Calibrated stand-in readings go into the noise history but never set the
  floor; held readings stay out of both.
- Channels with no live data (more than 80% of recent readings held) are
  drawn as dark "--" boxes on the Activity and Noise grids and left out of
  the Noise shading and ranking, the Quietest lists and the IMD-free sets.
  Previously such a channel had no history, so it looked like the quietest.

## Revision 80
- Stalled measurements: when every reading at a frequency times out, the
  scanner now repeats that frequency's last good reading (NAN, i.e. skipped,
  if there has never been one) instead of substituting the receiver's RSSI
  register. The RSSI value is on a different scale and produced impossible
  results (peaks of 250+ dB above the floor, false "busy" percentages on
  E7 / E8 and R8, and deep dips on the spectrum). Held readings are kept out
  of floor learning and out of the noise history.
- New "Meas timeout" setting on the FPV Channel Activity page (4-20 ms,
  default 8 ms; it was a fixed 5 ms). A normal measurement takes about
  3.3 ms; the longer limit lets slow readings finish instead of stalling.
  Only stalled readings take the extra time. Applies to every page.
- Serial "stalls" line now shows the held value ("held dB").

## Revision 79
- Waterfall: the colour for no signal is a darker blue (0, 0, 35 instead of
  0, 0, 60), and "Waterfall log" now defaults to 8.

## Revision 78
- Spectrum waterfall colours now follow a log curve, so weak signals show
  up brighter: colour = log(1 + k*x) / log(1 + k), where x is the signal
  as a fraction of the span. k is the new "Waterfall log" setting on the
  Spectrum Configuration page (0-20, default 4; 0 = linear, as before).

## Revision 77
- Releasing the IMD-Free hold now re-picks the sets from the latest noise
  history and redraws them the moment ENTER is pressed, instead of at the
  end of the next grid sweep. Turning hold on or off also updates "Hold"
  and the ENTER hint at once. (The noise history keeps collecting while
  the sets are held.)

## Revision 76
- IMD-Free hold: "Hold" is now red, and it and the "ENTER: Hold / Release"
  hint sit a little lower (6 px and 4 px).

## Revision 75
- IMD-Free Channels: ENTER now holds the channel sets. While held they are
  not re-picked, and "Hold" flashes (about once a second) under the table.
  ENTER again, or leaving the page, releases the hold and the sets are
  re-picked at once. A hint under the table shows "ENTER: Hold" or
  "ENTER: Release". The Help page lists "IMD-Free: hold / release the sets".

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
