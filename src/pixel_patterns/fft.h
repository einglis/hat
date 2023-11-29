#pragma once

#include "../pixel_pattern.h"

class FFTPattern : public PixelPattern
{
public:
  FFTPattern( ) { }
  virtual const char* name() { return "VU and beat"; }

  virtual void advance( int /*inc*/ )
  {
    int raw_vu = global_vu /32;
    if (raw_vu > my_vu)
    {
      my_vu = raw_vu;
      my_vu_decay = 0;
    }
    else
    {
      if (my_vu > 0 && my_vu_decay == 3) // magic number
      {
        my_vu -= 1;
        my_vu_decay = 0;
      }

      my_vu_decay++;
    }

    if (beat_sustain > 0)
      beat_sustain--;
  }

  virtual void beat( )
  {
    beat_sustain = 16;
  }

  virtual uint32_t pixel( unsigned int i )
  {
    const int mid = NUM_PIXELS / 2;  // 59 / 2 --> 29
      // but actually, pixels 28 and 29 straddle the centre line


    if (i < 4 || i >= NUM_PIXELS - 4)
      if (beat_sustain && my_vu > 10) // vu fudge
        return beat_sustain*0x0f0f0f;


    int pos = 0;

    if (i < (mid-1))
      pos = mid - 1 - i;
    else if (i > mid)
      pos = i - mid;

    int bright = my_vu * 10;
    if (bright > 255)
      bright = 255;

    uint32_t col = bright * 0x000100; // green
    if (pos > 16)
      col = bright * 0x010000; // red
    else if (pos > 9)
      col = bright * 0x010100; // yellow



    if (my_vu && pos < (my_vu-1))
      return col;
    else
      return 0;
  }

private:
  int my_vu;
  int my_vu_decay;
  int last_global_beat;
  int beat_sustain;
};
