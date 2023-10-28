
// NOTE: hideously copied from Simple Node and thence hacked.


// 0x80FF00 - amazing lime green
// 0x10FF10 - intense cyan/green
// 0x8000FF - rich magenta

class DummyPattern : public PixelPattern
{
public:
  virtual void advance( int /*inc*/ ) { }
  virtual uint32_t pixel( unsigned int /*i*/ ) { return 0x8000FF; }
  virtual const char* name( ) { return "dummy"; }
};
DummyPattern dummy;


PixelPattern* curr_pattern = &dummy;
PixelPattern* prev_pattern = curr_pattern;
PixelPattern* next_pattern = curr_pattern;

int transition_count = 0;
Ticker transition_ticker;

void transition_ticker_fn( )
{
  if (transition_count)
    transition_count--;

  if (transition_count == 0)
  {
    if (next_pattern != curr_pattern)
    {
      Serial.printf( "new pattern: \"%s\" -> \"%s\"\n", curr_pattern->name(), next_pattern->name() );
      next_pattern->activate();
        // won't be called for the very first pattern; gloss over this for now.

      prev_pattern = curr_pattern;
      curr_pattern = next_pattern;
      transition_count = 255;
    }
    else
    {
      prev_pattern->deactivate();
      transition_ticker.detach();
    }
  }
}

void new_pattern( PixelPattern* next )
{
  if (next)
  {
    Serial.printf("new pattern: \"%s\"\n", next->name() );

    next_pattern = next;
    transition_ticker.attach_ms( 10, transition_ticker_fn );
  }
}

void cycle_pattern( )
{
  Serial.printf("cycle: looking for next after \"%s\"\n", curr_pattern->name() );

  auto it = pixel_patterns.begin();
  for ( ; it != pixel_patterns.end(); ++it)
  {
    if (*it == curr_pattern)
    {
      ++it;
      break;
    }
  }

  if (it == pixel_patterns.end())
  {
    Serial.printf("cycle: no next!\n" );
    it = pixel_patterns.begin();
  }

  Serial.printf("cycle: found \"%s\"\n", (*it)->name() );
  new_pattern( *it );
}

uint32_t mix( uint32_t a, uint32_t b, unsigned int amnt)
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

bool pixels_update( Adafruit_NeoPixel &strip, int interval_ms )
{
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

  return true; // currently ignored
}

// ----------------------------------------------------------------------------

PixelPattern::~PixelPattern() {} // pure virtual destructor.
