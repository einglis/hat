#pragma once

// NOTE: hideously copied from Simple Node and thence hacked.

#include <Ticker.h>

namespace node {

class DebouncedInput
{
public:
  virtual ~DebouncedInput( ) { }

  void update_debounce_ms( int debounce_ms )
  {
    max_count = debounce_ms;
  }

protected:
  DebouncedInput( std::function<int()> input_fn_, int debounce_ms )
    : state{ 0 }
    , count{ 0 }
    , max_count{ debounce_ms }
    , input_fn{ input_fn_ }
    {
      if (!input_fn())
      {
        // force an event at startup even if input is low.
        state = 1;
        count = max_count;
      }
    }

  void begin()
  {
    ticker.attach_ms( 1, [this](){ poll(); } );
  }

  virtual void poll( )
  {
    const int in = input_fn();
    if (in && count < max_count)
      ++count;
    else if (!in && count > 0)
      --count;

    if ((state == 0 && count == max_count) 
      || (state == 1 && count == 0))
    {
      state = !state;
      new_state();
    }
  }

  int state;
  virtual void new_state( ) = 0; 

private:
  int count;
  int max_count;
  std::function<int()> input_fn;
  Ticker ticker;
};

// ----------------------------------------------------------------------------

class ButtonInput 
  : public DebouncedInput
{
public:
  ButtonInput( std::function<int()> input_fn )
    : DebouncedInput{ input_fn, debounce_ms }
    , count{ 0 }
    , timer{ 0 }
    { }

  enum Event { Press = 1, HoldShort, HoldLong, Final };

  void begin( std::function<void( Event, int )> fn )
  {
    event = fn;
    DebouncedInput::begin();
  }

private:
  virtual void new_state( ) 
  { 
    if (state) // button up -> down
      event( Press, ++count );
    timer = 0;
  }

  virtual void poll( )
  {
    DebouncedInput::poll();

    if (timer < std::numeric_limits<decltype(timer)>::max()) timer++;
      // don't overflow even if nothing happens for a while

    if (state) // button is down
    {
      if (timer == short_hold_ms)
        event( HoldShort, count );
      else if (timer == long_hold_ms)
        event( HoldLong, count );
    }
    else // button is up
    {
      if (timer == recent_press_ms)
      {
        event( Final, count );
        count = 0;
      }
    }
  }

  enum {
    debounce_ms = 20,
    recent_press_ms = 400,
    short_hold_ms = 1500,
    long_hold_ms = 3000,
  };

  int count; // number of presses
  int timer; // aka hold_time when button is down; recent_time when up
  std::function< void( Event, int ) > event;
};

// ----------------------------------------------------------------------------

class SwitchInput 
  : public DebouncedInput
{
public:
  SwitchInput( std::function<int()> input_fn )
    : DebouncedInput{ input_fn, debounce_ms }
    , count{ 0 }
    , timer{ 0 }
    { }

  enum Event{ FlipOpen = 1, FlipClose, Final };

 void begin( std::function<void( Event, int )> fn )
  {
    event = fn;
    DebouncedInput::begin();
  }

private:
  virtual void new_state( )
  {
    event( (state) ? FlipClose : FlipOpen, ++count );
    timer = 0;
  }

  virtual void poll( )
  {
    DebouncedInput::poll( );

    if (timer < std::numeric_limits<decltype(timer)>::max()) timer++;
      // don't overflow even if nothing happens for a while

    if (timer == recent_flip_ms)
    {
      event( Final, count );
      count = 0;
    }
  }

  enum {
    debounce_ms = 20,
    recent_flip_ms = 600,
  };

  int count;
  int timer; // aka recent_time
  std::function< void( Event, int ) > event;
};

} // node