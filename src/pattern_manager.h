#pragma once

#include <Adafruit_NeoPixel.h>
#include <vector>

#include "pixel_pattern.h"

namespace patterns
{
  void force_new( PixelPattern* next, bool fast = false );

  void add_to_cycle( PixelPattern* p, bool auto_cycle = true );
  void user_cycle( );

  void beat( );
  void update_strip( Adafruit_NeoPixel &strip, int interval_ms );
}
