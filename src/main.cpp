#include <Adafruit_NeoPixel.h>
#include <AudioTools.h>
#include "esp_adc_cal.h"

#include "button.h"
#include "pattern_manager.h"

namespace outputs {
  enum {
    pixels_pin = 4,
    pixels_en_N_pin = 14,
    button_en_pin = 15,
    mic_vdd_pin = 21,
    mic_gnd_pin = 22,

    dotstar_en_N_pin = 13,
    battery_serial_tx_pin = 23,
  };
}
namespace inputs {
  enum {
    button_pin = 27,
    mic_pin = 32,
    mic_adc_channel = ADC1_CHANNEL_4,

    battery_pin = 25,
    battery_adc_channel = ADC2_CHANNEL_8,
      // The natural TinyPico pin is 35, but this is on ADC1 and would interfere with the
      // mic input.  A bodge wire between MCU pins (or equivalent does the trick here.
    battery_serial_rx_pin = 33,
  };
}

void pixels_enable( bool en ) { digitalWrite( outputs::pixels_en_N_pin, !en ); }
void    mic_enable( bool en ) { digitalWrite( outputs::mic_vdd_pin,      en ); }
void button_enable( bool en ) { digitalWrite( outputs::button_en_pin,    en ); }

#define NUM_PIXELS 59 // really 60, but the last one is mostly hidden
Adafruit_NeoPixel strip( NUM_PIXELS, outputs::pixels_pin, NEO_GRB + NEO_KHZ800 );

ButtonInput button( [](){ return digitalRead( inputs::button_pin ); } );

// ----------------------------------------------------------------------------

int global_vu = 0;
int global_power_shift = 0;

int global_battery_mv = 0;
int global_battery_charge = 0; // percentage

// ----------------------------------------------------------------------------

#include "pixel_patterns/blocks.h"
BlockPattern blocks1;

#include "pixel_patterns/rainbows.h"
CycleRainbowPattern rainbow1;
MonoRainbowPattern rainbow2;

#include "pixel_patterns/kitt.h"
KittPattern kitt;

#include "pixel_patterns/snakes.h"
SnakesPattern snakes( NUM_PIXELS );

#include "pixel_patterns/fft.h"
FFTPattern fft_basic;

#include "pixel_patterns/specials.h"
BatteryDeadPattern dead_battery;
BrightnessPattern brightness_pattern;

// ------------------------------------

Ticker pixel_ticker;
const int pixel_ticker_interval_ms = 3;
  // a full pixel update takes 1.7ms,
  // but back to back the minimum is 2.3ms.

void pixel_ticker_fn( )
{
  patterns::update_strip( strip, pixel_ticker_interval_ms );
  strip.show();

  #if 0
  static int k = 0;
  static int start = millis();
  k++;
  if (k == 1000)
  {
    long now = millis();
    Serial.printf("leds at %ld Hz\n", 1000000/(now-start));
    start = now;
    k = 0;
  }
  #endif
}

// ----------------------------------------------------------------------------

Ticker battery_ticker;
const int battery_ticker_interval_sec = 7; // pretty random

void battery_ticker_fn( )
{
  #define DEFAULT_VREF  1100  // reference voltage in mv
    // confirmed during development by measurement using: adc_vref_to_gpio( ADC_UNIT_2, (gpio_num_t)26 );

  static esp_adc_cal_characteristics_t chars;

  static bool first_time = true;
  if (first_time)
  {
    Serial1.begin(9600, SERIAL_8N1, outputs::battery_serial_tx_pin, inputs::battery_serial_rx_pin);

    adc2_config_channel_atten( (adc2_channel_t)inputs::battery_adc_channel, ADC_ATTEN_11db );
    esp_adc_cal_characterize( ADC_UNIT_2, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, DEFAULT_VREF, &chars);
    first_time = false;
  }

  int raw = 0;
  adc2_get_raw( (adc2_channel_t)inputs::battery_adc_channel, ADC_WIDTH_BIT_12, &raw );

  const uint32_t cal = esp_adc_cal_raw_to_voltage( raw, &chars );
  const uint32_t mv = cal * 3.55 + 180; // calculation from tedious calibration
  global_battery_mv = (int)mv;

  const int charge = 100 * ((int)mv - 3100) / (4100 - 3100);  // 3.1v to 4.1v seems a conservative operating range
  global_battery_charge = min( max( charge, 0 ), 100);

  //Serial.printf( "Battery: ~%dmv --> %d%% (raw %u, cal %u)\r\n", global_battery_mv, global_battery_charge, raw, cal );
  Serial1.printf( "Battery: ~%dmv --> %d%% (raw %u, cal %u)\r\n", global_battery_mv, global_battery_charge, raw, cal );

  if (global_battery_charge == 0)
  {
    //Serial.printf("Sleeping shortly...\n");
    Serial1.printf("Sleeping shortly...\n");

    mic_enable( false ); // mic off
    button_enable( false ); // lazy way to prevent user mode changes
    patterns::force_new( &dead_battery, true /*fast transition*/ );
      // obviously leave LEDs on to show this.

    battery_ticker.attach( battery_ticker_interval_sec, []()  // can't use delay() here.
    {
      //Serial.printf("I die :(\n");
      //Serial.flush();
      Serial1.printf("I die :(\n");
      Serial1.flush();
      esp_deep_sleep_start();
        // drops to about 20uA which close enough to expections
        // apparently no need to disable the various GPIO enables
    } );
  }
}

// ----------------------------------------------------------------------------

const int brightnesses[] = { 16, 32, 64, 128, 4, 8 };
  // full brightness (255) is: a) too much and b) will kill the battery/MCU
const int num_brightnesses = sizeof(brightnesses) / sizeof(brightnesses[0]);
int curr_brightness = 0;

enum HatMode { ModeOff, ModeCycle, ModeBright, ModeVol, ModeDead };
HatMode hat_mode = ModeOff;


void button_fn( ButtonInput::Event e, int count )
{
  static bool last_was_press = false;

  if (e == ButtonInput::HoldLong)
  {
    switch (hat_mode)
    {
      case ModeCycle:
      case ModeBright:
      case ModeVol:
        pixels_enable( false );
        mic_enable( false );
        hat_mode = ModeOff;
        break;

      default: break;
    }
  }
  else if (e == ButtonInput::HoldShort)
  {
    switch (hat_mode)
    {
      case ModeOff:
        mic_enable( true );
        pixels_enable( true );
        // fallthrough

      case ModeBright:
        patterns::user_cycle();
        // fallthrough

      case ModeVol:
        fft_basic.show_marker(false);
        hat_mode = ModeCycle;
        break;

      case ModeCycle:
        if (curr_pattern == &fft_basic)
        {
          fft_basic.show_marker(true);
          hat_mode = ModeVol;
        }
        else
        {
          patterns::force_new( &brightness_pattern, true /*fast transition*/ );
          hat_mode = ModeBright;
        }
        break;

      default: break;
    }
  }
  else if (e == ButtonInput::Final && last_was_press) // respond on button up, basically
  {
    switch (hat_mode)
    {
      case ModeCycle:
        patterns::user_cycle();
        break;

      case ModeBright:
        curr_brightness = (curr_brightness + 1) % num_brightnesses;
        Serial.printf( "Setting brightness to %d\n", brightnesses[curr_brightness] );
        strip.setBrightness( brightnesses[curr_brightness] );
        break;

      case ModeVol:
        global_power_shift++;
        if (global_power_shift > 10)
          global_power_shift = 0;
        Serial.printf( "Setting volume to %0.3f%%\n", 100.0 / (1 << global_power_shift) );
        break;

      default: break;
    }
  }

  last_was_press = (e == ButtonInput::Press);
}

// ----------------------------------------------------------------------------

void start_vu_task( ); // lazy

void setup()
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("Hat's hat!");

  // --------------

  pinMode( inputs::battery_pin, INPUT );
  battery_ticker.attach( battery_ticker_interval_sec, battery_ticker_fn );

  // --------------

  pinMode( outputs::dotstar_en_N_pin, OUTPUT );
  digitalWrite( outputs::dotstar_en_N_pin, HIGH ); // dotstar off

  pinMode( outputs::pixels_pin, OUTPUT );
  pinMode( outputs::pixels_en_N_pin, OUTPUT );
  digitalWrite( outputs::pixels_en_N_pin, HIGH ); // pixels off

  strip.begin();
  strip.setBrightness( brightnesses[0] );
  strip.clear();
  strip.show();

  patterns::add_to_cycle( &rainbow1 );
  patterns::add_to_cycle( &snakes );
  patterns::add_to_cycle( &rainbow2 );
  patterns::add_to_cycle( &fft_basic, false /*don't auto cycle*/ );
  patterns::add_to_cycle( &blocks1, false /*don't auto cycle*/ );
  patterns::add_to_cycle( &kitt );

  pixel_ticker.attach_ms( pixel_ticker_interval_ms, pixel_ticker_fn );

  // --------------

  pinMode( inputs::button_pin, INPUT_PULLDOWN );
  pinMode( outputs::button_en_pin, OUTPUT );
  digitalWrite( outputs::button_en_pin, HIGH ); // needed to recognise presses

  button.begin( button_fn );

  // --------------

  pinMode( outputs::mic_gnd_pin, OUTPUT );
  pinMode( outputs::mic_vdd_pin, OUTPUT );
  pinMode( inputs::mic_pin, INPUT );

  digitalWrite( outputs::mic_gnd_pin, LOW );
  digitalWrite( outputs::mic_vdd_pin, HIGH ); // using GPOIs as power rails

  start_vu_task( );


  button_fn( ButtonInput::HoldShort, 0 );
    // fake a button press to get the ball rolling.
}

void loop() { }

// ----------------------------------------------------------------------------

int correlate_beats( const int powers[], const int num_powers, const float beat_stride,
  int& max_phase_pos, int& max_phase_val, int& min_phase_val )
{
  const int bump_width = 8;
  static int bump[ bump_width ] = { 0 };

  static bool first_time = true;
  if (first_time)
  {
    for (int i = 0; i < bump_width; i++)
      bump[i] = 255 * sin( 2*M_PI * (i + 0.5) / bump_width / 2 );
    first_time = false;
  }


  const int num_phases = min( 32, (int)beat_stride ); // avoid too much work at low bpms
  int phases[ num_phases ];

  for (int p = 0; p < num_phases; p++)
  {
    float bump_pos = num_powers - bump_width - p * beat_stride / num_phases + 0.5;
      // * float to maintain resolution of fractional phases and strides,
      //   but decimated to an integer whenever used as an index.
      // * start with the rightmost bump and work backwards; means we're always
      //   looking at the newest audio, even with a truncated number of bumps
      // * for proper rounding, we should add 0.5 before every conversion to
      //   int, but we can actually just do it once at the start

    int sum = 0;
    int num_bumps = 0;

    while ((int)bump_pos >= 0 && num_bumps < 6) // 6 bumps is 3.75s @ 80 bpm; 1.67s @ 180 bpm
    {
      for (int i = 0; i < bump_width; i++)
        sum += bump[i] * powers[(int)bump_pos + i];  // +0.5 for correct rounding

      num_bumps++;
      bump_pos -= beat_stride;
    };

    phases[p] = sum / num_bumps;  // divide to normalise
  }

  // separate out the min/max deduction purely for division of labour
  min_phase_val = phases[0];
  max_phase_val = phases[0];
  max_phase_pos = 0;

  for (int p = 0; p < num_phases; p++)
  {
    if (phases[p] > max_phase_val)
    {
      max_phase_val = phases[p];
      max_phase_pos = (int)(p * beat_stride / num_phases) + bump_width/2;
    }

    if (phases[p] < min_phase_val)
      min_phase_val = phases[p]; // don't care where though
  }

  return max_phase_val; // why not?!
}

void find_peaks( int vals[], const int num_vals, int peaks[], const int num_peaks )
{
  int pvals[ num_peaks ] = { 0 };

  for (int i = 1; i < num_vals-1; i++)
  {
    const int prev_gradient = vals[i] - vals[i-1];
    const int next_gradient = vals[i+1] - vals[i];

    if (prev_gradient >= 0 && next_gradient < 0) // peak or end of plateau
    {
      if (vals[i] > pvals[num_peaks-1])
      {
        peaks[num_peaks-1] = i; // new entry, at least better than previous worst
        pvals[num_peaks-1] = vals[i];
        for (int p = num_peaks-1; p > 0; p--) // bubble into position; fine for the small lists we're using
        {
          if (pvals[p] > pvals[p-1])
          {
            std::swap( peaks[p], peaks[p-1] );
            std::swap( pvals[p], pvals[p-1] );
          }
          else
          {
            break;
          }
        }
      }
    }
  }
}



int global_curr_beat = 0;
float global_next_beat = 0.0;
float global_beat_inc = 100.0;
float global_beat_adj = 0.0;


void find_beats( const int32_t powers[], const int num_powers, const float fsamp )
{
  const int min_bpm =  80;
  const int max_bpm = 180;
  const int num_bpms = max_bpm - min_bpm + 1;

  int bpms_pos[ num_bpms ];
  int bpms_val[ num_bpms ];

  for (int bpm = min_bpm; bpm <= max_bpm; bpm++)
  {
    const float beat_stride = fsamp * 60.0 / bpm;

    int phase, bmax, bmin;
    correlate_beats( powers, num_powers, beat_stride, phase, bmax, bmin );

    bpms_pos[bpm-min_bpm] = phase;
    bpms_val[bpm-min_bpm] = max(0, bmax-bmin);

    //Serial.printf("%3d bpm - %d - %d at %d\n", bpm, bmin, bmax, phase);   // human
    //Serial.printf("%d, %d,%d\n", bmin, bmax, max(0, bmax-bmin));          // graph
  }

  // ----------------------------------

  const int num_peaks = 3;
  int peaks[ num_peaks ]; // strongest bpms
  find_peaks( bpms_val, num_bpms, &peaks[0], num_peaks );

  Serial.printf("Best BPMs: %3d, %3d, %3d\n", peaks[0]+min_bpm, peaks[1]+min_bpm, peaks[2]+min_bpm );  // human

  #if 0 // graph
  for (int i = 0; i < num_bpms; i++)
  {
    int x = 0;
    for (int p = 0; p < num_peaks; p++)
      if (i == peaks[p])
        x = bpms_val[i];
    Serial.printf("%d, %d\n", bpms_val[i], x );
  }
  Serial.println("0,0");
  Serial.println("0,0");
  Serial.println("0,0");
  Serial.println("0,0");
  #endif


  int best_bpm = peaks[0] + min_bpm;

  const float beat_stride = fsamp * 60.0 / best_bpm;
  const int beat_pos = bpms_pos[best_bpm-min_bpm];
    // how far back from the end; ie a smaller value means a more recent beat
    // the reported phase really is only integer accuracy, even though it was calculated from floats

  float planned_next = beat_stride - beat_pos;
  if (planned_next <= 0)
    planned_next += beat_stride;

  const float expected_next = global_next_beat - global_curr_beat;

  float error = expected_next - planned_next; // positive means expected beat is too slow;
  if (error > beat_stride/2) // probably too early, in fact
    error -= beat_stride;
  else if (error < -beat_stride/2)
    error += beat_stride;


  Serial.printf("beat stride %f, pos is %d\n", beat_stride, beat_pos );
  Serial.printf("next should be %f; currently is %f - error %f\n", planned_next, expected_next, error );

  global_beat_inc = beat_stride;
  global_beat_adj = -error / 2;
}



int update_vu( uint32_t power_sum, int chunk_size )
{
  // Some grungy, empirical maths to give a nice VU meter.  Blended over this sub-chunk and
  // the last for reasons lost to time.  Ideall would not use an expensive log() but okay for now.

  // Works well at 312-ish Hz.  (20kHz samples, 128-byte buffers -> 3.2ms)

  static uint32_t last_power_sum = 0;

  const int temp = max(0, (int)(150 * (log( (power_sum + last_power_sum) / chunk_size / 4 ) - 8)));
  last_power_sum = power_sum;
  return temp;
}

int32_t subtract_average( int32_t* src, int32_t* dst, int num )
{
  int32_t power_av = 0;
  for (auto i = 0; i < num; i++)
    power_av += src[i];
  power_av /= num;

  for (auto i = 0; i < num; i++)
    dst[i] = max( 0, src[i] - power_av );

  return power_av;
}

void update_beat( )
{
  global_curr_beat++;

  if (global_curr_beat > global_next_beat + 100)
  {
    Serial.println("resync");
    global_next_beat = global_curr_beat;
  }

  if (global_curr_beat >= global_next_beat)
  {
    patterns::beat();
    global_next_beat += global_beat_inc + global_beat_adj;
    global_beat_adj = 0; // apply only once
  }

  // keep things in a plausible range
  if (global_curr_beat > 2000)
  {
    global_curr_beat -= 1000;
    global_next_beat -= 1000;
  }
}

// ----------------------------------------------------------------------------

TaskHandle_t vu_task;
void vu_task_fn( void* vu_x )
{
  Serial.print("vu_task_fn() running on core ");
  Serial.println(xPortGetCoreID());

  AnalogConfig config( RX_MODE );
  config.setInputPin1( inputs::mic_pin );
  config.sample_rate = 20000;
    // sample rates below 20kHz don't behave as expected;
    // not sure I can explain it though.
    // 20 kHz is more than adequate since we're only using powers not FFTs
  config.bits_per_sample = 16;
  config.channels = 1;
  config.buffer_size = 128; // smaller buffers give us smoother extraction
  config.buffer_count = 64; // lots of buffering saves us losing samples while processing
    // at 20kHz, 128 bytes of 16-bit samples takes just 3.2ms
    // so 64 buffers gives 205ms of leeway.  Not convinced it really does though...

  AnalogAudioStream in;
  in.begin(config);


  const int sub_chunk_size = config.buffer_size;  // a good size for a responsive VU meter
  int16_t *sample_buf = (int16_t *)malloc(sub_chunk_size * sizeof(int16_t));

  const int chunk_size = 256; // must be a multiple of sub_chunk_size
  int chunk_fill = 0;

  const float fsamp = (float)config.sample_rate / chunk_size;
      // about 78Hz (-> 12.8 ms) with 256-long chunks at 20kHz sample rate

  const int window_length = fsamp * 4; // 4 second windows; 312 chunks long
  int32_t *powers  = (int32_t *)malloc(window_length * sizeof(int32_t));
  int32_t *powers2 = (int32_t *)malloc(window_length * sizeof(int32_t));
  int num_powers = 0;


  Serial.printf("  Sample rate: %dHz\n", config.sample_rate);
  Serial.printf("  Buffer size: %d * %d, Sub-chunk size: %d --> %dHz (VU happy around 312Hz)\n",
    config.buffer_size, config.buffer_count, sub_chunk_size, config.sample_rate / (config.buffer_size / (config.bits_per_sample / 8)));
  Serial.printf("  fsamp: %.3fHz, Window length %d (%d seconds)\n", fsamp, window_length, (int)(window_length / fsamp));


  uint32_t power_acc = 0;
    // samples are stored as signed 16-bit, but really only 12-bits of resolution,
    // so 256 squared values is  2^8 * 2^12 * 2^12 = 2^32.
    // Strictly, this doesn't fit, but in reality it's fine.

    // In any case, I've only seen max samples of 1762 by tapping the mic; far short
    // of the expected 4095.  (Typically about 500-ish for 'silent' conditions)


  long last = millis();
  int num_loops = 0;


  while(1)
  {
    (void)in.readBytes((uint8_t*)sample_buf, sub_chunk_size*sizeof(int16_t));
      // ignore the case where we get too little data; the rest of the logic should not explode, at least

    uint32_t power_sum = 0;
    for (int i = 0; i < sub_chunk_size; i++)
      power_sum += sample_buf[i] * sample_buf[i];

    power_sum >>= global_power_shift;

    *((int *)vu_x) = update_vu( power_sum, sub_chunk_size );


    power_acc += power_sum; // incremental calculation

    chunk_fill += sub_chunk_size;
    if (chunk_fill == chunk_size)
    {
      powers[ num_powers++ ] = power_acc / chunk_size / 512; // divide just to reduce magnitude a bit
        // with typical max sample values of 1729 seen experimentally, this gives
        // an expected max of ~6000, but typically see about half that.

      chunk_fill = 0; // reset
      power_acc = 0; // reset

      update_beat();
        // beat estimation has a chunk_size'd resolution, so update once a chunk
        // do it before the time-consuming bits, just in case

      if (num_powers == window_length) // accrued a full window
      {
        int av = subtract_average( powers, powers2, num_powers );
        //Serial.printf("average %d\n", av);

#if 0
        for (auto i = 0; i < num_powers; i++)
          Serial.printf("%d, %d\n", powers[i], powers2[i]);
#endif

        long start = millis();
        find_beats( powers2, num_powers, fsamp );
        long end = millis();
        //Serial.printf("find_beats() took %ld ms\n", end-start );
          // 1 second of window takes approximately 8ms of processing

        memmove(&powers[0], &powers[window_length/4], (num_powers-window_length/4)*sizeof(int32_t));
        num_powers -= window_length / 4;
      }

      num_loops++;
        // count full chunks as a loop, rather than sub-chunks, because that's what we really care about
    }

#if 0
    long now = millis();
    if (now - last >= 5000) // report every five seconds
    {
      Serial.printf("%d loops in %ld ms --> %.2f Hz\n", num_loops, (now-last), 1000.0 * num_loops / (now-last) );
      num_loops = 0; // reset
      last = now;
    }
#endif
  }
}

void start_vu_task( )
{
  xTaskCreatePinnedToCore( vu_task_fn, "VU sampling",
    10000,  /* stack size in words */
    (void *)&global_vu,   /* context */
    0,      /* priority */
    &vu_task,
    0       /* core */
  );
}
