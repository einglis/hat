#pragma once
// NOTE: hideously copied from Simple Node and thence hacked.

#include <Adafruit_NeoPixel.h>

// 0x80FF00 - amazing lime green
// 0x10FF10 - intense cyan/green
// 0x8000FF - rich magenta

// ----------------------------------------------------------------------------

class PixelPattern
{
public:
  virtual uint32_t pixel( unsigned int i ) = 0;
  virtual void advance( int inc = 1 ) = 0;
  virtual void beat() { }
  virtual void activate() { }
  virtual void deactivate() { }
  virtual int interval_ms() { return 10; }
  virtual ~PixelPattern() = 0;
  virtual const char* name() { return "<unnamed>"; }

  void advance_( int inc_ms )
  {
    inc_acc += inc_ms;
    if (inc_acc >= interval_ms())
    {
      advance();
      inc_acc = 0;
    }
  }
private:
  int inc_acc;

protected:
  static uint32_t colour( uint8_t r, uint8_t g, uint8_t b )
  {
    return Adafruit_NeoPixel::Color(r, g, b);
  }
  static uint8_t gamma( uint8_t x )
  {
    return Adafruit_NeoPixel::gamma8(x);
  }

  static uint32_t rgb_wheel( uint8_t p )
  {
    if (p < 85)
    {
      return Adafruit_NeoPixel::Color(p * 3, 255 - p * 3, 0);
    }
    else if(p < 170)
    {
      p -= 85;
      return Adafruit_NeoPixel::Color(255 - p * 3, 0, p * 3);
    }
    else
    {
      p -= 170;
      return Adafruit_NeoPixel::Color(0, p * 3, 255 - p * 3);
    }
  }
};
