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

std::vector< std::pair< PixelPattern*, bool > >cycle_patterns;

const int cycle_interval_sec = 60;
Ticker cycle_ticker;

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
  cycle_ticker.detach();

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

} // patterns

namespace {
void do_cycle( bool user_cycle = false )
{
  int curr_index = cycle_patterns.size() - 1;

  // try and find the current pattern in the list (it might not be there)
  // work backwards so if not found, when we then increment, we end up back at the first.
  for ( ; curr_index >= 0; curr_index--)
    if (cycle_patterns[curr_index].first == curr_pattern)
      break;

  // find the next pattern (or first if current is not int the cycle)
  int next_index = (curr_index + 1) % cycle_patterns.size();

  // if this is an auto-cycle, skip any that are not auto.
  if (!user_cycle)
  {
    for (int i = 0; i < cycle_patterns.size(); i++)
    {
      if (!cycle_patterns[next_index].second) // not in auto-cycle
        next_index = (next_index + 1) % cycle_patterns.size(); // skip
      else
        break;
    }
  }

  patterns::force_new( cycle_patterns[next_index].first );

  if (cycle_patterns[next_index].second) // auto_cycle
    cycle_ticker.once( cycle_interval_sec, [](){ do_cycle(); } );
}

} // anon

namespace patterns {

void add_to_cycle( PixelPattern* p, bool auto_cycle )
{
  cycle_patterns.push_back( std::make_pair( p, auto_cycle ) );
}

void user_cycle( )
{
  do_cycle( true /*user_cycle*/ );
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
