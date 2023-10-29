#pragma once

#include "../pixel_pattern.h"

class BatteryDeadPattern : public PixelPattern
{
public:
  BatteryDeadPattern() : phase { 1 } { } 

  virtual const char* name() { return "Battery Dead"; }
  virtual int interval_ms() { return 500; }
  void advance( int ) { phase = 1-phase; }

  virtual uint32_t pixel( unsigned int i )
  {
    if (phase && (i > NUM_PIXELS/2 - 4 && i <= NUM_PIXELS/2 + 4)) // deliberately uneven
      return 0x800000; // red
    else
      return 0x000000; // off
  }

private:
  int phase;
};

class BrightnessPattern : public PixelPattern
{
public:
  virtual const char* name() { return "Brightness"; }
  void advance( int ) { }

  virtual uint32_t pixel( unsigned int i )
  {
    switch (7 * i / NUM_PIXELS)
    {
      case 0: return 0xff0000; // red
      case 1: return 0xffff00; // yellow
      case 2: return 0x00ff00; // green
      case 3: return 0x00ffff; // cyan
      case 4: return 0x0000ff; // blue
      case 5: return 0xff00ff; // magenta
      default: return 0xffffff; // white
    }
  }
};
