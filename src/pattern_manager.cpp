#include <Ticker.h>
#include <vector>

#include "pattern_manager.h"

namespace {

class DummyPattern : public PixelPattern
{
public:
  virtual void advance( int /*inc*/ ) { }
  virtual uint32_t pixel( unsigned int /*i*/ ) { return 0x000000; /*off*/ }
  virtual const char* name( ) { return "dummy"; }
};
DummyPattern dummy;

std::vector< PixelPattern* >cycle_patterns;

//-------------------------------------

PixelPattern* curr_pattern = &dummy;
PixelPattern* prev_pattern = curr_pattern;
PixelPattern* next_pattern = curr_pattern;

int transition_count = 0;
int transition_rate = 1;
Ticker transition_ticker;

void transition_ticker_fn( )
{
  if (transition_count > 0 && transition_count <= transition_rate)
  {
    if (prev_pattern != curr_pattern)
    {
      Serial.printf( "deactivate: \"%s\"\n", prev_pattern->name() );
      prev_pattern->deactivate();
    }
  }

  transition_count = max( 0, transition_count - transition_rate );
  if (transition_count == 0)
  {
    if (next_pattern != curr_pattern)
    {
      Serial.printf( "new pattern: \"%s\" -> \"%s\"\n", curr_pattern->name(), next_pattern->name() );
      Serial.printf( "activate: \"%s\"\n", next_pattern->name() );
      next_pattern->activate();
        // won't be called for the very first pattern; gloss over this for now.

      prev_pattern = curr_pattern;
      curr_pattern = next_pattern;
      transition_count = 255;
    }
  }
}
} // anon

namespace patterns {

void force_new( PixelPattern* next, bool fast )
{
  if (next)
  {
    Serial.printf("new pattern: \"%s\"\n", next->name() );
    next_pattern = next;

    transition_rate = (fast) ? 32 : 2;
      // if we're in the middle of a slow transition, the fast
      // one will complete it rapidly then move on; this seems good.
  }
}

void beat( )
{
  curr_pattern->beat();
  if (prev_pattern != curr_pattern)
    prev_pattern->beat();
}

void user_cycle( )
{
  auto it = cycle_patterns.begin();
  for ( ; it != cycle_patterns.end(); ++it)
  {
    if (*it == curr_pattern)
    {
      ++it;
      break;
    }
  }

  if (it == cycle_patterns.end())
    it = cycle_patterns.begin();

  force_new( *it );
}

void add_to_cycle( PixelPattern* p, bool )
{
  cycle_patterns.push_back( p );
}

} // patterns

// ------------------------------------

namespace {
uint32_t mix( uint32_t a, uint32_t b, unsigned int amnt )
{
    uint32_t x = 0;
    for (int i = 0; i < 4; i++)
    {
        unsigned int aa = a & 0xff;
        unsigned int bb = b & 0xff;
        unsigned int xx = (aa * amnt + bb * (255-amnt)) >> 8;

        x = (x >> 8) | (xx << 24);
        a >>= 8;
        b >>= 8;
    }

    return x;
}
} // anon

namespace patterns {

void update_strip( Adafruit_NeoPixel &strip, int interval_ms )
{
  static bool first_time = true;
  if (first_time)
  {
    transition_ticker.attach_ms( 10, transition_ticker_fn );
    first_time = false;
  }

  PixelPattern* pattern = curr_pattern;
  PixelPattern* pattern_outgoing = prev_pattern;

  if (transition_count > 0)
  {
      if (pattern && pattern_outgoing)
      {
          pattern->advance_( interval_ms );
          pattern_outgoing->advance_( interval_ms );
          for (auto i = 0; i < strip.numPixels(); i++)
              strip.setPixelColor( i, mix(pattern_outgoing->pixel(i), pattern->pixel(i),transition_count) );
      }
  }
  else
  {
      if (pattern)
      {
          pattern->advance_( interval_ms );
          for (auto i = 0; i < strip.numPixels(); i++)
              strip.setPixelColor( i, pattern->pixel(i) );
      }
  }
}

} // patterns

// ----------------------------------------------------------------------------

PixelPattern::~PixelPattern() {} // pure virtual destructor.
