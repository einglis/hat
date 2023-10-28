
#include <Adafruit_NeoPixel.h>
#include <AudioTools.h>
#include "esp_adc_cal.h"
#include <vector>

#include "button.h"

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

ButtonInput button( [](){ return digitalRead( inputs::button_pin ); } );


#define NUM_PIXELS 59 // really 60, but the last one is mostly hidden
Adafruit_NeoPixel strip( NUM_PIXELS, outputs::pixels_pin, NEO_GRB + NEO_KHZ800 );

const int brightnesses[] = { 16, 32, 64, 128, 4, 8 };
  // full brightness (255) is: a) too much and b) will kill the battery/MCU
const int num_brightnesses = sizeof(brightnesses) / sizeof(brightnesses[0]);
int curr_brightness = 0;


// ----------------------------------------------------------------------------

int global_vu;
int global_beat = 0;
  // used by fft pattern.  To be improved!


#include "pixel_pattern.h"
static std::vector< PixelPattern* >pixel_patterns;

#include "pixel_patterns/rainbows.h"
CycleRainbowPattern rainbow1;
MonoRainbowPattern rainbow2;

#include "pixel_patterns/colour_random.h"
ColourRandomPattern random_colours;

#include "pixel_patterns/snakes.h"
SnakesPattern snakes( NUM_PIXELS );

#include "pixel_patterns/sparkle.h"
SparklePattern sparkle_white;
SparklePattern sparkle_red( 0x010000 );
SparklePattern sparkle_yellow( 0x010100 );

#include "pixel_patterns/fft.h"
FFTPattern fft_basic;

// ------------------------------------

Ticker pixel_ticker;
const int pixel_ticker_interval_ms = 3;
  // a full pixel update takes 1.7ms,
  // but back to back the minimum is 2.3ms.

void pixel_ticker_fn( )
{
  pixels_update( strip, pixel_ticker_interval_ms );
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

int global_battery_mv = 0;
int global_battery_charge = 0;

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

   Serial.printf( "Battery: ~%dmv --> %d%% (raw %u, cal %u)\r\n", global_battery_mv, global_battery_charge, raw, cal );
  Serial1.printf( "Battery: ~%dmv --> %d%% (raw %u, cal %u)\r\n", global_battery_mv, global_battery_charge, raw, cal );

  if (global_battery_charge == 0)
  {
    Serial1.printf("Sleeping shortly...\n");
    Serial1.flush();

    delay(1000);
      // would be pretty to do some light effect, but...

    esp_deep_sleep_start();
      // drops to about 20uA which close enough to expections
      // apparently no need to disable the various GPIO enables
  }
}

// ----------------------------------------------------------------------------

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

  pixel_patterns.push_back( &fft_basic );
  pixel_patterns.push_back( &rainbow1 );
//  pixel_patterns.push_back( &rainbow2 );
  pixel_patterns.push_back( &snakes );
//pixel_patterns.push_back( &random_colours );
//pixel_patterns.push_back( &sparkle_white );
  pixel_patterns.push_back( &sparkle_red );
//pixel_patterns.push_back( &sparkle_yellow );

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
}



AnalogAudioStream in;

int global_next_beat = 100;
int global_beat_int = 100;

void find_beats( int32_t *powers, const int num_powers, const int fsamp )
{
  #if 0
  for (int i = 0; i < num_powers; i++)
    Serial.println(powers[i]);
  #endif


  const int bump_width = 8;
  static int bump[ bump_width ] = { 0 };

  if (bump[0] == 0) // first time
    for (int i = 0; i < bump_width; i++)
      bump[i] = 255 * sin( 2*M_PI * (i + 0.5) / bump_width / 2 );


  int32_t power_av = 0;
  for (int i = 0; i < num_powers; i++)
    power_av += powers[i];
  power_av /= num_powers;

  for (int i = 0; i < num_powers; i++)
    powers[i] = max( 0, powers[i] - power_av ) / 1024;



  // int best_bpm = 0;
  // int best_bpm_phase = 0;
  // float best_bpm_sum = 0;

  int last_val = 0;
  int last_gradient = 0;
  int bests[3] = {0};
  float bests_vals[3] = {0};


  float bpms_max[101];
  float bpms_min[101];
  int bpms_pos[101];

  for (int bpm = 80; bpm <= 180; bpm += 1)
  {
    //  Serial.printf("----------- %d bpm ----------\n", bpm);
      const int max_phases = fsamp * 60 / bpm;
      const int num_phases = min(32, max_phases );

      float phases[ num_phases ];

      for (int p = 0; p < num_phases; p++)
      {
          const int phase = p * (max_phases/num_phases); // offset

          float sum = 0;
          int num_bumps = 0;
          for (; num_bumps < 100; num_bumps++) // XXXEDD: move this out
          {
            const int bump_pos = num_bumps * fsamp * 60 / bpm + phase;
            if (bump_pos + bump_width > num_powers)
              break; // no room at the inn!

            for (int i = 0; i < bump_width; i++)
            {
              sum += bump[i] * (float)powers[bump_pos + i]; // /64 to make it fit
              //Serial.printf("phase %d, sum %f, bump %d, pwr %d, mult %f\n",
              //  phase, sum, bump[i], powers[bump_pos + i], bump[i] * (float)powers[bump_pos + i]);
            }
          }

          phases[p] = sum / num_bumps;  // divide to normalise
      }

      float min_phase_val = phases[0];
      float max_phase_val = 0;
      int max_phase_pos = 0;

      for (int i = 0; i < num_phases; i++)
      {
        if (phases[i] > max_phase_val)
        {
          max_phase_val = phases[i];
          max_phase_pos = i * (max_phases/num_phases);
        }
        if (phases[i] < min_phase_val)
          min_phase_val = phases[i]; // don't care where though
      }

      //Serial.printf("%3d bpm - %.0f - %.0f at %d\n", bpm, min_phase_val, max_phase_val, max_phase_pos);
      bpms_min[ bpm - 80 ] = min_phase_val;
      bpms_max[ bpm - 80 ] = max_phase_val;
      bpms_pos[ bpm - 80 ] = max_phase_pos;
  }

#if 0
  float avav = 0;
  for (int i = 0; i < 101; i++) // nice for the Arduino graph :)
    avav += max((float)0, bpms_max[i] - bpms_min[i]);
  avav /= 101;


  for (int i = 0; i < 95; i++) // nice for the Arduino graph :)
    Serial.printf("%.0f, %0.f\n ", bpms_max[i], max((float)0.0, bpms_max[i]-bpms_min[i]-avav));

  for (int i = 0; i < 5; i++)
    Serial.println("0,0");
#endif

  (void)bpms_min;
  (void)bpms_max;
  (void)bpms_pos;

#if 0
      if (best_phase_sum > best_bpm_sum)
      {
        best_bpm_sum = best_phase_sum;
        best_bpm_phase = best_phase;
        best_bpm = bpm;
      }


      int this_gradient = best_phase_sum - last_val;
      if (last_gradient > 0 && this_gradient <= 0)
      {
          // last was local peak
          int peak_bpm = bpm - 1;
          int peak_val = last_val;

          if (peak_val >= bests_vals[0])
          {
            bests[2] = bests[1];
            bests[1] = bests[0];
            bests[0] = peak_bpm;
            bests_vals[2] = bests_vals[1];
            bests_vals[1] = bests_vals[0];
            bests_vals[0] = peak_val;
          }
          else if (peak_val >= bests_vals[1])
          {
            bests[2] = bests[1];
            bests[1] = peak_bpm;
            bests_vals[2] = bests_vals[1];
            bests_vals[1] = peak_val;
          }
          else if (peak_val > bests_vals[2])
          {
            bests[2] = peak_bpm;
            bests_vals[2] = peak_val;
          }
      }
      last_val = best_phase_sum;
      last_gradient = this_gradient;

      bpms[bpm-80] = best_phase_sum;
      bpms_n[bpm-80] = phases[ (best_bpm_phase + num_phases / 2)%num_phases ];

  }



  float av_sum = 0.0;
  for (auto i : bpms)
    av_sum += i;

  av_sum /= (sizeof(bpms)/sizeof(bpms[0]));

#if 1
  for (int i = 0; i < 101; i++)
    Serial.printf("%.0f, %0.f, %.0f\n ", bpms[i], bpms_n[i], bpms[i]-bpms_n[i]);
  Serial.println("0,0,0");
  Serial.println("0,0,0");
  Serial.println("0,0,0");
  Serial.println("0,0,0");
  #else
  Serial.printf("Best %d bpm with phase %d - conf %.2f\n", best_bpm, best_bpm_phase, best_bpm_sum/1000 );
  Serial.printf(" -- %d (%.0f) >= %d (%.0f) >= %d (%.0f)\n", bests[0], bests_vals[0], bests[1], bests_vals[1], bests[2], bests_vals[2] );
#endif
  int best_best = bests[0];
  if (bests[1] > best_best) best_best = bests[1];
  if (bests[2] > best_best) best_best = bests[2];

  static float av_best = best_best;
  av_best = (31 * av_best + best_best) / 32;

//  Serial.printf(" ---- best_best %d,  av %.1f\n", best_best, av_best);

#endif

  int best_bpm = 0;
  float best_bpm_val = bpms_max[0];
  int best_bpm_pos = bpms_pos[0];

  for (int i = 0; i < 101; i++)
  {
    float t = bpms_max[i];//max((float)0.0, bpms_max[i]-bpms_min[i]);
    if (t > best_bpm_val)
    {
      best_bpm_val = t;
      best_bpm_pos = bpms_pos[i];
      best_bpm = i;
    }
  }

  best_bpm += 80;

  int beat_interval = (fsamp * 60 + best_bpm/2 ) / best_bpm;

  int bump_pos = 0;
  for (int i = 0; true; i++)
  {
      bump_pos  = i * fsamp * 60 / best_bpm + best_bpm_pos;
      if (bump_pos > num_powers)
        break;
  }
  bump_pos -= num_powers;

  //Serial.printf( "best bpm: %d  %6.0f  next is %d -> %d, interval %d\n",
  //  best_bpm, best_bpm_val, global_next_beat, bump_pos, beat_interval);



  //int next_curr = global_next_beat + global_beat_int;
  //int next_next = bump_pos + beat_interval;
  int max_int = max(global_beat_int, beat_interval);

  int error = (bump_pos - global_next_beat + max_int) % max_int;
    // +ve => running fast

  if (error > max_int/2)
    error -= max_int;

  if (error > 0)
  {
    //Serial.printf("Running too fast %d\n", error);
    if (error > 2)
      error *= 0.3;
    else
      error = 1;
  }
  else if (error < 0)
  {
    //Serial.printf("Running too slow %d\n", error);
    if (error < -2)
      error *= 0.3;
    else
      error = -1;
  }


  // if (bump_pos > global_next_beat)
  // {
  //   int phase_error = bump_pos - global_next_beat;
  //   int phase_error__ = (phase_error < 2)? phase_error : phase_error * 0.7;
  //   Serial.printf("Running too fast %d %d\n", phase_error, phase_error__);
  //   global_next_beat += phase_error; // extra delay
  // }
  // else if (bump_pos < global_next_beat)
  // {
  //   int phase_error = global_next_beat - bump_pos;
  //   int phase_error__ = (phase_error < 2)? phase_error : phase_error * 0.7;
  //   Serial.printf("Running too slow %d %d\n", phase_error, phase_error__);
  //   if (global_next_beat > phase_error)
  //     global_next_beat -= phase_error; // reduce delay
  // }


  //Serial.printf("new int %d, curr int %d\n", beat_interval, global_beat_int);
  int beat_err = beat_interval - global_beat_int;
  if (beat_err > 1 || beat_err < -1)
    beat_err *= 0.7;

  global_beat_int += beat_err;



  if (global_beat_int + error > 0)
    global_beat_int += error;


  //Serial.printf("finally %d\n", global_beat_int);

//  (void)best_bpm;
//  (void)best_bpm_phase;
//  (void)bpms_n;
}





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
    // so 64 buffers gives 205ms of leeway.  A lot!


  //AudioLogger::instance().begin(Serial, AudioLogger::Info);
  in.begin(config);


  const int chunk_size = 256; // must be a multiple of sub_chunk_size
  const int vu_chunk_size = 128; // empirically this is a good size at 20kHz
  const int sub_chunk_size = config.buffer_size;
  int16_t *sample_buf = (int16_t *)malloc(sub_chunk_size * sizeof(int16_t));
  int chunk_fill = 0;

  const int fsamp = config.sample_rate / chunk_size;
      // about 39Hz (-> 25.6 ms) with 512-long chunks at 20kHz sample rate

  const int window_length = fsamp * 4; // 3 second windows; 76 chunks long
  int32_t *powers = (int32_t *)malloc(window_length * sizeof(int32_t));
  int32_t *powers2 = (int32_t *)malloc(window_length * sizeof(int32_t));
  int num_powers = 0;

  uint32_t power_acc = 0;
    // samples are stored as signed 16-bit, but really only 12-bits of resolution,
    // so 512 squared values is  2^9 * 2^11 * 2^11 = 2^31.  Phew!  Just fits :)


  long last = millis();
  int num_loops = 0;

  uint32_t last_power_sum = 0;

  while(1)
  {
    size_t rc = in.readBytes((uint8_t*)sample_buf, sub_chunk_size*sizeof(int16_t));
      // ignore the case where we get too little data; the rest of the logic should not explode, at least

    uint32_t power_sum = 0;
    for (int i = 0; i < sub_chunk_size; i++)
      power_sum += sample_buf[i] * sample_buf[i];
    power_acc += power_sum;

    //Serial.printf("%8u, %8u\n", power_sum/sub_chunk_size, (power_sum + last_power_sum) / sub_chunk_size);

    *((int *)vu_x) = max(0, (int)(150 * (log( (power_sum + last_power_sum) / sub_chunk_size / 4 ) - 8)));
      // XXXEDD: sqrt needs attention
      // XXXEDD: agc further to normalise display?

    last_power_sum = power_sum;


    chunk_fill += sub_chunk_size;
    if (chunk_fill == chunk_size)
    {
      //Serial.println(power_acc / chunk_size);
      powers[ num_powers++ ] = power_acc / chunk_size; // divide just to reduce magnitude a bit
      chunk_fill = 0; // reset
      power_acc = 0; // reset

      num_loops++;
        // cound full chunks as a loop, rather than sub-chunks, because that's what we really care about


      if (global_next_beat > 0)
      {
        global_next_beat--;
      }
      else
      {
        global_beat++;
        global_next_beat = global_beat_int;
      }
    }

    if (num_powers == window_length)
    {
      //memcpy(&powers2[0], &powers[window_length/4], (num_powers-window_length/4)*sizeof(int32_t));

//      long start = millis();
      find_beats( powers, num_powers, fsamp );
//      long end = millis();
//      Serial.printf("find_beats() took %ld ms\n", end-start );
        // 1 second of window takes approximately 8ms of processing

      //num_powers -= window_length / 4;

      //memcpy(&powers[0], &powers2[0], num_powers*sizeof(int32_t));

      num_powers = 0;
    }




#if 1
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


//void button_fn ( ButtonInput::Event e, int count );
void button_fn ( ButtonInput::Event e, int count )
{
  static int on = 1;

  if (e == ButtonInput::HoldShort)
  {
    on = !on;
    Serial.printf("Now %s\n", (on)? "on":"off");
  }
  else if (e == ButtonInput::Press)
  {
    curr_brightness = (curr_brightness + 1) % num_brightnesses;
    Serial.printf( "Setting brightness to %d\n", brightnesses[curr_brightness] );
  //  strip.setBrightness( brightnesses[curr_brightness] );

    cycle_pattern();
  }

  digitalWrite( outputs::pixels_en_N_pin, !on );
  digitalWrite( outputs::mic_vdd_pin, on );
}

// ----------------------------------------------------------------------------

void loop() { }
