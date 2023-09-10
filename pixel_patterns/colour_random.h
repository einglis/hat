#pragma once

#include "../pixel_pattern.h"

class ColourRandomPattern : public PixelPattern
{
public:
  virtual void advance( int ) { }
  virtual uint32_t pixel( unsigned int ) 
  { 
    return rgb_wheel(rand()); 
  }
};
