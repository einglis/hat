#pragma once

#include "../pixel_pattern.h"

class SparklePattern : public PixelPattern
{
public:
  SparklePattern( uint32_t m = 0x01010101 ) 
    : mult{ m } 
    { }

  virtual void advance( int ) { }
  virtual uint32_t pixel( unsigned int )
  {
    const uint8_t k = gamma(rand());
    return (uint32_t)k * mult;
  }

private:
  const uint32_t mult;
};
