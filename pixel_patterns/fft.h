#pragma once

#include <arduinoFFT.h>
#include "../pixel_pattern.h"

class FFTPattern : public PixelPattern
{
public:
  FFTPattern( int en_pin, int input_pin )
    : en_pin{ en_pin }
    , mic_pin{ input_pin }
  { }

  virtual void activate()
  {
    // enable mic
    //digitalWrite( en_pin, HIGH );
  }
  virtual void deactivate()
  {
    // disable mic
    //digitalWrite( en_pin, LOW );
  }

  virtual void advance( int /*inc*/ )
  {


    int raw_vu = global_vu / 32;


    static int dec = 10;

    if (raw_vu > vu2)
    {
      vu2 = raw_vu;
      dec = 0;
    }
    else
    {
      if (dec == 3 && vu2)
      {
        vu2 -= 1;//= (31*vu2+raw_vu)/32;
        dec = 0;
      }

      dec++;
    }


    if (global_beat != last_global_beat)
    {
      last_global_beat = global_beat;
      beat_sustain = 16;
    }
    else if (beat_sustain > 0)
    {
    beat_sustain--;
    }
  }

  virtual uint32_t pixel( unsigned int i )
  {
    const int mid = NUM_PIXELS / 2;  // 59 / 2 --> 29
      // but actually, pixels 28 and 29 straddle the centre line


    if (i < 4 || i >= NUM_PIXELS - 4)
      if (beat_sustain)
        return beat_sustain*0x0f0f0f;


    int pos = 0;

    if (i < (mid-1))
      pos = mid - 1 - i;
    else if (i > mid)
      pos = i - mid;

    int bright = vu2 * 10;
    if (bright > 255)
      bright = 255;

    uint32_t col = bright * 0x000100; // green
    if (pos > 16)
      col = bright * 0x010000; // red
    else if (pos > 9)
      col = bright * 0x010100; // yellow



    if (vu2 && pos < (vu2-1))
      return col;
    else
      return 0;
  }

private:
  int en_pin;
  int mic_pin;
  int vu2;
  int last_global_beat;
  int beat_sustain;
};