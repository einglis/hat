#pragma once

#include <Adafruit_NeoPixel.h>
#include <vector>

#include "pixel_pattern.h"

extern std::vector< PixelPattern* >pixel_patterns;

void new_pattern( PixelPattern* next, bool fast = false );
void pattern_beat( );
void cycle_pattern( );
bool pixels_update( Adafruit_NeoPixel &strip, int interval_ms );
