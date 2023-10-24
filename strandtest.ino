
#include <Adafruit_NeoPixel.h>
#include <vector>

#include "button.h"

namespace outputs {
  enum {
    pixels_pin = 4,
    pixels_en_N_pin = 14,
    button_en_pin = 15,
    mic_vdd_pin = 21,
    mic_gnd_pin = 22
  };
}
namespace inputs {
  enum {
    button_pin = 27,
    mic_pin = A4,
  };
}

#define NUM_PIXELS 59 // really 60, but the last one is mostly hidden
#define PIXEL_BRIGHTNESS 16 // 64 // full brightness (255) is: a) too much and b) will kill the battery/MCU

Adafruit_NeoPixel strip( NUM_PIXELS, outputs::pixels_pin, NEO_GRB + NEO_KHZ800 );
ButtonInput button( [](){ return digitalRead( inputs::button_pin ); } );

// ----------------------------------------------------------------------------

int global_vu;
int global_beat = 0;


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

void setup()
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("Hat's hat!");

  pinMode( outputs::pixels_pin, OUTPUT );
  pinMode( outputs::pixels_en_N_pin, OUTPUT );
  digitalWrite( outputs::pixels_en_N_pin, HIGH ); // pixels off

  pinMode( inputs::button_pin, INPUT_PULLDOWN );
  pinMode( outputs::button_en_pin, OUTPUT );
  digitalWrite( outputs::button_en_pin, HIGH ); // needed to recognise presses

  pinMode( outputs::mic_gnd_pin, OUTPUT );
  pinMode( outputs::mic_vdd_pin, OUTPUT );
  pinMode( inputs::mic_pin, INPUT );

  digitalWrite( outputs::mic_vdd_pin, HIGH ); // using GPOIs as power rails
  digitalWrite( outputs::mic_gnd_pin, LOW );
  //adcAttachPin( inputs::mic_pin );


  strip.begin();
  strip.setBrightness( PIXEL_BRIGHTNESS );
  strip.clear();
  strip.show();

  button.begin( button_fn );


//  pixel_patterns.push_back( &rainbow1 );
//  pixel_patterns.push_back( &rainbow2 );
//  pixel_patterns.push_back( &snakes );
//pixel_patterns.push_back( &random_colours );
//pixel_patterns.push_back( &sparkle_white );
//  pixel_patterns.push_back( &sparkle_red );
//pixel_patterns.push_back( &sparkle_yellow );
  pixel_patterns.push_back( &fft_basic );

  pixel_ticker.attach_ms( pixel_ticker_interval_ms, pixel_ticker_fn );

  start_vu_task( );
}


//#include <driver/adc.h>
//#include <driver/i2s.h>
//#include "esp32_audio/ADCSampler.h"

//#include <arduino_audio_tools/src/AnalogAudio.h>
#include <AudioTools.h>

#if 0

AudioInfo info(44100, 2, 16);
AnalogAudioStream in;
I2SStream out;
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // RX automatically uses port 0 with pin GPIO34
  auto cfgRx = in.defaultConfig(RX_MODE);
  cfgRx.copyFrom(info);
  in.begin(cfgRx);

  // TX on I2S_NUM_1
  auto cfgTx = out.defaultConfig(TX_MODE);
  cfgTx.port_no = 1;
  cfgTx.copyFrom(info);
  out.begin(cfgTx);
}

// Arduino loop - copy data
void loop() {
  copier.copy();
}
#endif

//AudioInfo info(44100, 2, 16);
AnalogAudioStream in;

int global_next_beat = 10000;
int global_beat_int = 10000;

void find_beats( int32_t *powers, const int num_powers, const int fsamp )
{
//  for (int i = 0; i < num_powers; i++)
//    Serial.println(powers[i]);



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
  for (int i = 0; i < 95; i++) // nice for the Arduino graph :)
    Serial.printf("%.0f, %0.f, %0.f\n ", bpms_max[i], bpms_min[i], max((float)0.0, bpms_max[i]-bpms_min[i]));

  for (int i = 0; i < 5; i++)
    Serial.println("0,0,0");
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
    float t = max((float)0.0, bpms_max[i]-bpms_min[i]);
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

  Serial.printf( "best bpm: %d  next is %d -> %d, interval %d\n", best_bpm, global_next_beat, bump_pos, beat_interval);
  global_next_beat = bump_pos;
  global_beat_int = beat_interval;

//  (void)best_bpm;
//  (void)best_bpm_phase;
//  (void)bpms_n;
}





TaskHandle_t vu_task;
void vu_task_fn( void* vu_x )
{
  Serial.print("vu_task_fn() running on core ");
  Serial.println(xPortGetCoreID());


  //AudioInfo info(44100, 2, 16); // not sure how to use this at present
  AnalogConfig config( RX_MODE );
  config.setInputPin1( inputs::mic_pin );
  config.sample_rate = 20000;
    // sample rates below 20kHz don't behave as expected;
    // not sure I can explain it though.
    // 20 kHz is more than adequate since we're only using powers not FFTs
  config.bits_per_sample = 16;
  config.channels = 1;
  config.buffer_size = 128; // smaller buffers give us smoother extraction
  config.buffer_count = 8; // probably overkill


  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  in.begin(config);


  const int chunk_size = 256; // must be a multiple of sub_chunk_size
  const int vu_chunk_size = 128; // empirically this is a good size at 20kHz
  const int sub_chunk_size = config.buffer_size;
  int16_t *sample_buf = (int16_t *)malloc(sub_chunk_size * sizeof(int16_t));
  int chunk_fill = 0;

  const int fsamp = config.sample_rate / chunk_size;
      // about 39Hz (-> 25.6 ms) with 512-long chunks at 20kHz sample rate

  const int window_length = fsamp * 3; // 3 second windows; 76 chunks long
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

      //long start = millis();
      find_beats( powers, num_powers, fsamp );
      //long end = millis();
      //Serial.printf("find_beats() took %ld ms\n", end-start );

      //num_powers -= window_length / 4;

      //memcpy(&powers[0], &powers2[0], num_powers*sizeof(int32_t));

      num_powers = 0;
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
    cycle_pattern();

  digitalWrite( outputs::pixels_en_N_pin, !on );
  digitalWrite( outputs::mic_vdd_pin, on );
}

// loop() function -- runs repeatedly as long as board is on ---------------

void loop() {
  return;
  int adc_raw = analogRead( inputs::mic_pin );
  Serial.println(adc_raw);
    // this println kills uploads?!

  delay( 100 ); // seems to make programming more reliable.

  return;
#if 0
  // Fill along the length of the strip in various colors...
//  colorWipe(strip.Color(255,   0,   0), 50); // Red
//  colorWipe(strip.Color(  0, 255,   0), 50); // Green
//  colorWipe(strip.Color(  0,   0, 255), 50); // Blue
//  colorWipe(strip.Color(255, 255, 255), 50); // White
//  colorWipe(strip.Color(255, 255, 255), 50); // White
//
//  // Do a theater marquee effect in various colors...
//  theaterChase(strip.Color(127, 127, 127), 50); // White, half brightness
//  theaterChase(strip.Color(127,   0,   0), 50); // Red, half brightness
//  theaterChase(strip.Color(  0,   0, 127), 50); // Blue, half brightness

 // Serial.println(digitalRead(5));
  //igitalWrite(1, HIGH); // turn on always

  adc_raw = analogRead( inputs::vsense_pin );
  Serial.print(adc_raw);
  Serial.print(", ");

  static int adc = adc_raw;
  adc = (7 * adc + adc_raw) / 8;
    // adc tends to be massively jittery, so average it out

  Serial.print(adc);
  Serial.print(", ");

  float volts = adc * 0.00094 + 0.3;

  Serial.print(volts);
  Serial.print(", ");

 // theaterChaseRainbow(50); // Rainbow-enhanced theaterChase variant

  int battery = adc * 0.1051 - 313;
    // The above based on calibration while discharging; would tend to under-estimate when charging.
    //   (I suspect the charging circuit pushes more current through the potential divider)
    //   0% charge on this scale is indicated at a real-world 3.1v, so should has a small safety margin.
    // 100% charge on this scale is indicated at a real-world 4.0v, to avoid batteries appearing to start at only 80% ish.
  if (battery > 100)
    battery = 100;

  Serial.println(battery);

  rainbow(0, battery);             // Flowing rainbow cycle along the whole strip

  if (battery <= 0)
  {
    colorWipe(strip.Color(255,   0,   0), 50); // Red
    colorWipe(strip.Color(0,   0,   0), 50); // Off


    // ~0.005A
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    // ~0.004A
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

gpio_reset_pin(GPIO_NUM_2);
gpio_reset_pin(GPIO_NUM_9);
//rtc_gpio_isolate(9);

    esp_deep_sleep_start();
  }
#endif
}


// Some functions of our own for creating animated effects -----------------

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait, int percent) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    // strip.rainbow() can take a single argument (first pixel hue) or
    // optionally a few extras: number of rainbow repetitions (default 1),
    // saturation and value (brightness) (both 0-255, similar to the
    // ColorHSV() function, default 255), and a true/false flag for whether
    // to apply gamma correction to provide 'truer' colors (default true).
    strip.rainbow(firstPixelHue);
    // Above line is equivalent to:
    // strip.rainbow(firstPixelHue, 1, 255, 255, true);

    for (int i = 0; i < NUM_PIXELS; i++) {
//      strip.setPixelColor(i, strip.getPixelColor(i)&0xff); // blue
//    strip.setPixelColor(i, strip.getPixelColor(i)&0xff00); // green
//      strip.setPixelColor(i, strip.getPixelColor(i)&0xff0000); // red
 //       strip.setPixelColor(i, (strip.getPixelColor(i)&0xff)*0x10101); // white
//        strip.setPixelColor(i, (strip.getPixelColor(0)&0xff)*0x10101); // solid white
  }

    for (int i = percent*NUM_PIXELS/100; i < NUM_PIXELS; i++) {
      strip.setPixelColor(i, 0);
      break;
    }




if (0)
{
    strip.setPixelColor(0, 0x00ff00); // green on right
    strip.setPixelColor(1, 0x00ff00); // green on right
    strip.setPixelColor(2, 0x008000); // green on right
    strip.setPixelColor(3, 0x004000); // green on right
    strip.setPixelColor(4, 0x002000); // green on right
    strip.setPixelColor(5, 0x001000); // green on right
    strip.setPixelColor(6, 0x000800); // green on right
    strip.setPixelColor(7, 0x000400); // green on right
    strip.setPixelColor(8, 0x000200); // green on right
    strip.setPixelColor(9, 0x000100); // green on right

    strip.setPixelColor(NUM_PIXELS-1, 0xff0000); // red on left
    strip.setPixelColor(NUM_PIXELS-2, 0xff0000); // red on left
    strip.setPixelColor(NUM_PIXELS-3, 0x800000); // red on left
    strip.setPixelColor(NUM_PIXELS-4, 0x400000); // red on left
    strip.setPixelColor(NUM_PIXELS-5, 0x200000); // red on left
    strip.setPixelColor(NUM_PIXELS-6, 0x100000); // red on left
    strip.setPixelColor(NUM_PIXELS-7, 0x080000); // red on left
    strip.setPixelColor(NUM_PIXELS-8, 0x040000); // red on left
    strip.setPixelColor(NUM_PIXELS-9, 0x020000); // red on left
    strip.setPixelColor(NUM_PIXELS-10, 0x010000); // red on left
}
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for(int a=0; a<30; a++) {  // Repeat 30 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}
