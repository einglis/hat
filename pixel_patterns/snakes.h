#pragma once

#include "../pixel_pattern.h"

static const uint16_t MAX_STEPS = 2048;
static const uint16_t STEPS_MASK = MAX_STEPS - 1;

class SnakesPattern : public PixelPattern
{
public:
  SnakesPattern( uint16_t num_pixels )
    : num_pixels{ (uint16_t)(3 * num_pixels / 2) } // elongate the apparent loop
    , pos{ 0, 0, 0 }
    , inc{ 3, 7, 5 }
    , dir{ 1, 1, -1 }
  { }

  virtual const char* name() { return "Snakes"; }
  virtual int interval_ms() { return 40; }

  void advance( int )
  {
    for (auto j = 0; j < num_snakes; j++)
    {
      pos[j] += inc[j] * 8;
      if (pos[j] > MAX_STEPS)
        pos[j] -= MAX_STEPS;
    }
  }

  uint32_t pixel( unsigned int i )
  {
    uint8_t val[num_snakes];

    uint16_t pix_pos = i * MAX_STEPS / num_pixels;

    for (uint8_t j = 0; j < num_snakes; j++)
    {
      if (dir[j] > 0)
        val[j] = shade((pix_pos + pos[j]) & STEPS_MASK);
      else
        val[j] = shade((MAX_STEPS + pos[j] - pix_pos) & STEPS_MASK);
    }

    return colour(((int)val[0] / 3 + val[1] + val[2]) / 3,
                  ((int)val[0] + val[1] / 3 + val[2]) / 3,
                  ((int)val[0] + val[1] + val[2] / 3) / 3);
  }

private:
  const uint16_t num_pixels;
  enum { num_snakes = 3 };
  uint16_t pos[num_snakes];
  uint8_t inc[num_snakes];
  int8_t dir[num_snakes];

  static uint8_t shade( uint16_t pos )
  {
    const int div = MAX_STEPS / 256;

    if      (pos <     MAX_STEPS / 16)  return pos * 16 / div;
    else if (pos < 5 * MAX_STEPS / 16)  return 255 - ((pos - MAX_STEPS / 16) * 4 / div);
    else                                return 0;
  }
};
