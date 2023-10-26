
// NOTE: hideously copied from Simple Node and thence hacked.

int curr_pattern = 0;
int prev_pattern = curr_pattern;
int next_pattern = curr_pattern;

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
      Serial.print( "new pattern " );
      Serial.print( curr_pattern );
      Serial.print( " -> " );
      Serial.println( next_pattern );
      pixel_patterns[next_pattern]->activate();
        // won't be called for the very first pattern; gloss over this for now.

      prev_pattern = curr_pattern;
      curr_pattern = next_pattern;
      transition_count = 255;
    }
    else
    {
      pixel_patterns[prev_pattern]->deactivate();
      transition_ticker.detach();
    }
  }
}

void new_pattern( int next )
{
  if (next >= (int)pixel_patterns.size())
  {
    Serial.print( "unknown pattern " );
    Serial.println( next );
    return;
  }

  next_pattern = next;
  transition_ticker.attach_ms( 10, transition_ticker_fn );
}

void cycle_pattern( )
{
  new_pattern( (curr_pattern + 1) % pixel_patterns.size() );
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
  PixelPattern* pattern = pixel_patterns[ curr_pattern ];
  PixelPattern* pattern_outgoing = pixel_patterns[ prev_pattern ];

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
