#pragma once

#include <stdint.h>

// How to fit a JPEG onto the screen with the decoder's fixed scales (1, 1/2, 1/4, 1/8).
struct Fit {
  int scale;    // 1, 2, 4 or 8: the image is divided by this while decoding
  int offsetX;  // where the scaled image's top-left lands on screen (negative = cropped)
  int offsetY;
};

// Picks the strongest reduction that still covers the whole screen ("cover", then crop the
// centre). An image smaller than the screen is shown unscaled and centred.
Fit fitImage(int imageW, int imageH, int screenW, int screenH);

// Ordered (Bayer 8x8) dithering: true when the pixel at (x, y) with this 0-255 gray level
// should be black. Stateless, so it works block by block as the decoder streams pixels.
bool ditherBlack(int x, int y, uint8_t gray);
