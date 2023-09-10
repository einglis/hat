#pragma once

#include "Ticker.h"
#include "../pixel_pattern.h"

class TimedPattern : public PixelPattern
{
public:
  virtual void advance( int ) { }

  virtual uint32_t pixel( unsigned int ) { return col; }
  virtual void activate()
  {
    ticker.attach_scheduled(1, [this](){ 
      col = rgb_wheel(rand()); 
    } );
  }
  virtual void deactivate()
  {
    ticker.detach();
  }
private:
  Ticker ticker;
  uint32_t col;
};