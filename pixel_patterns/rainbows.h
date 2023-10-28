#pragma once

#include "../pixel_pattern.h"

class CycleRainbowPattern : public PixelPattern
{
public:
  CycleRainbowPattern( )
    : j{ 0 }
    { }

  virtual const char* name() { return "Step rainbow"; }
  virtual int interval_ms() { return 1000; }

  virtual void advance( int inc )
  {
    j += 50;
  }
  virtual uint32_t pixel( unsigned int i )
  {
    return rgb_wheel( (2*i+j) & 255 );
  }

private:
  unsigned int j;
};

class MonoRainbowPattern : public PixelPattern
{
public:
  MonoRainbowPattern() : j( 0 ) { }

  virtual void advance( int inc )
  {
    j += inc;
  }
  virtual uint32_t pixel( unsigned int )
  {
     return rgb_wheel( j & 255 );
  }
private:
  unsigned int j;
};
