/*
 * FPV Channel Scanner - Bands.cpp
 * Revision: 95
 *
 * Band display order (by lowest frequency), shared by the grid rows and
 * the spectrum band buttons.
 */

#include "Scanner.h"

// Band buttons, left to right, in order of each band's lowest frequency.
void buildButtonOrder() {
  uint16_t lowest[NUM_BANDS];
  for (int b = 0; b < NUM_BANDS; b++) {
    lowest[b] = 65535;
    for (int c = 0; c < NUM_CH; c++) lowest[b] = min(lowest[b], FPV_MHZ[b][c]);
    btnOrder[b] = b;
  }
  for (int i = 1; i < NUM_BANDS; i++) {          // insertion sort by lowest freq
    int k = btnOrder[i]; int j = i - 1;
    while (j >= 0 && lowest[btnOrder[j]] > lowest[k]) { btnOrder[j + 1] = btnOrder[j]; j--; }
    btnOrder[j + 1] = k;
  }
  for (int i = 0; i < NUM_BANDS; i++) bandRow[btnOrder[i]] = i;
}

// Band of the next enabled button to the right of the current one (wraps
// around); disabled bands (HDZero mode) are skipped.
int nextBandInButtonOrder(int band) {
  int pos = 0;
  for (int i = 0; i < NUM_BANDS; i++) if (btnOrder[i] == band) pos = i;
  for (int k = 1; k <= NUM_BANDS; k++) {
    int b = btnOrder[(pos + k) % NUM_BANDS];
    if (bandEnabled(b)) return b;
  }
  return band;
}
