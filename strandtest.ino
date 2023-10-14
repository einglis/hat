
#include <Adafruit_NeoPixel.h>
#include <vector>

#include "button.h"

namespace outputs {
  enum {
    pixels_pin = 4,
    pixels_en_pin = 14,
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
#define PIXEL_BRIGHTNESS 64 // full brightness (255) is: a) too much and b) will kill the battery/MCU

Adafruit_NeoPixel strip( NUM_PIXELS, outputs::pixels_pin, NEO_GRB + NEO_KHZ800 );
ButtonInput button( [](){ return digitalRead( inputs::button_pin ); } );

// ----------------------------------------------------------------------------

int global_vu;


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
FFTPattern fft_basic( outputs::mic_vdd_pin, inputs::mic_pin );

// ------------------------------------

Ticker pixel_ticker;
void pixel_ticker_fn( )
{
  bool need_update = pixels_update( strip );
  if (need_update)
    strip.show();
}

// ----------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("Hat's hat!");

  pinMode( outputs::pixels_pin, OUTPUT );
  pinMode( outputs::pixels_en_pin, OUTPUT );
  digitalWrite( outputs::pixels_en_pin, HIGH ); // pixels off

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

  pixel_ticker.attach_ms( 10, pixel_ticker_fn );

  Serial.print("loop() running on core ");
  Serial.println(xPortGetCoreID());

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


void find_beats( int32_t *powers, const int num_powers, const int fsamp )
{
  const int bump_width = 8;
  static int bump[ bump_width ] = { 0 };
  static int bump_sum = 0;

  if (bump_sum == 0)
  {
    for (int i = 0; i < bump_width; i++)
    {
      bump[i] = 255 * sin( 2*M_PI * (i + 0.5) / bump_width / 2 );
      bump_sum += bump[i];
      Serial.printf("bump %d - %d\n", i, bump[i]);
    }
    Serial.printf("bump sum %d\n", bump_sum);
  }
  Serial.println("find beats");



  int32_t power_av = 0;
  for (int i = 0; i < num_powers; i++)
    power_av += powers[i];
  power_av /= num_powers;

  for (int i = 0; i < num_powers; i++)
    powers[i] = max( 0, powers[i] - power_av );

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
  config.buffer_size = 64; // smaller buffers give us smoother extraction
  config.buffer_count = 8; // probably overkill


  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  in.begin(config);


  const int chunk_size = 512; // must be a multiple of sub_chunk_size
  const int sub_chunk_size = config.buffer_size; // smaller sub-chunks keep the VU meter more responsive
  int16_t *sample_buf = (int16_t *)malloc(sub_chunk_size * sizeof(int16_t));
  int chunk_fill = 0;

  const int fsamp = config.sample_rate / chunk_size;
      // about 39Hz (-> 25.6 ms) with 512-long chunks at 20kHz sample rate

  const int window_length = fsamp * 3; // 3 second windows; 76 chunks long
  int32_t *powers = (int32_t *)malloc(window_length * sizeof(int32_t));
  int num_powers = 0;
  
  uint32_t power_acc = 0;
    // samples are stored as signed 16-bit, but really only 12-bits of resolution,
    // so 512 squared values is  2^9 * 2^11 * 2^11 = 2^31.  Phew!  Just fits :)


  long last = millis();
  int num_loops = 0;


  while(1)
  {
    size_t rc = in.readBytes((uint8_t*)sample_buf, sub_chunk_size*sizeof(int16_t));
      // ignore the case where we get too little data; the rest of the logic should not explode, at least

    uint32_t power_sum = 0;
    for (int i = 0; i < sub_chunk_size; i++)
      power_sum += sample_buf[i] * sample_buf[i];
    power_acc += power_sum;

    // update the VU meter, regardless of what happens next
    *((int *)vu_x) = sqrt( power_sum / sub_chunk_size ); // XXXEDD: sqrt needs attention


    chunk_fill += sub_chunk_size;
    if (chunk_fill == chunk_size)
    {
      powers[ num_powers++ ] += power_acc / chunk_size; // divide just to reduce magnitude a bit

      chunk_fill = 0; // reset
      power_acc = 0; // reset

      num_loops++;
        // cound full chunks as a loop, rather than sub-chunks, because that's what we really care about
    }

    if (num_powers == window_length)
    {
      find_beats( powers, num_powers, fsamp );
      num_powers = 0; // reset
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

#if 0

  int kk = 0;

  in.begin(config);

  int16_t *buf2 = (int16_t *)malloc(4096);
  int16_t *buf3 = (int16_t *)malloc(4096);

  int16_t med[3] = { 0 };


  int16_t my_sin[2048];
  for (int i = 0; i < 2048; i++)
  {
    my_sin[i] = 255*sin(2*M_PI / 2048 * i);
    if (my_sin[i] < 0)
      my_sin[i] = 0;
   // Serial.println(my_sin[i]);
  }

  while(1)
  {
//    if (in.available() < 1024)
//      continue;
   // Serial.println(in.available());

   int k = millis();
    //size_t rc = in.readBytes(buf, 512*sizeof(int16_t)); // approx 10.7ms at 48kHz.
      // 512 is nominal FFT size.
      // but could collect smaller chunks for the VU meter.

    size_t rc = in.readBytes((uint8_t*)buf, 64*sizeof(int16_t)); // approx 5.2ms at 48kHz.
     int j = millis();

    for (int i = 0; i < rc/2; i++)
    {
      med[0] = med[1];
      med[1] = med[2];
      med[2] = buf[i];


      const int &a = med[0];
      const int &b = med[1];
      const int &c = med[2];

      int16_t m = 0;

      if (a <= b && a <= c)
        m = (b <= c) ? b : c;
      else if (b <= a && b <= c)
        m = (a <= c) ? a : c;
      else
        m = (a <= b) ? a : b;

      buf2[i] = m;

      //Serial.printf("%d %d %d -> %d\n", a,b,c,m);

    }



//     Serial.printf("%u, %d\n", 0, j-k);
    //rc = in.readBytes(buf2, 4096);
    //rc = in.readBytes(buf, sizeof(buf));
    //rc = in.readBytes(buf, sizeof(buf));
    //rc = in.readBytes(buf, sizeof(buf));
   // Serial.println(in.available());
   // Serial.println("");
    (void)rc;


if(0)
{
      Serial.println("2000, 2000");

    for (int i = 0; i < rc/2; i += 1)
        Serial.printf("%d, %d\n", buf[i], buf2[i] );

      Serial.println("-2000, -2000");
}


  // calculate power

    int32_t sum = 0;
      // 256 * int16 * int16 == 1 billion; eg fits in a 32-bit int.

    for (int i = 0; i < rc/2; i++)
      sum += buf2[i] * buf2[i];

    int vu = sqrt( sum / rc / 2 );

    //Serial.printf("%d %d\n", sum, vu );

    int bpm = 125;


   // Serial.println( 30000 * my_sin[(int)(2*1.3*kk)%2048 ] );
    buf3[kk] = vu;

    global_vu = vu;


   kk++;
    if (kk == 2048)
    {
      Serial.println("task 2 ki");
      kk = 0;

      for (int bpm = 80; bpm <= 180; bpm += 10)
      {
        int bpm_max = 0;
        int bpm_max_phase = 0;

        int sin_inc = bpm * 1.3 / 60;
//        double sin_factor = 0.000136135 * bpm;
    //Serial.println( 30000 * sin( 0.017*kk) );



        for (int phase = 0; phase < 2048; phase++)
        {
          int i_pulse = 0;
//          int pulse_pos = 0;

          int sum = 0;

          //while (pulse_pos < 2048 && i_pulse < 5)
          {
            int nn = 0;
            for (int s = 0; s < 2028; s++)
            {
              nn = s * bpm * 1.3 / 60;



              int ss = my_sin[(nn)%2048 ];
              sum +=  ss * buf3[(phase+s)%2048];

              //nn += sin_inc;
              i_pulse = nn / 2048;
              if (i_pulse >= 5)
                break;

            }

            // sum +=  5 * buf3[(pulse_pos+phase+2047)%2048];
            // sum += 10 * buf3[(pulse_pos+phase)%2048];
            // sum +=  5 * buf3[(pulse_pos+phase+1)%2048];

            // i_pulse++;
            // pulse_pos = 060000 * i_pulse / bpm / 1.3;
          }

          if (sum > bpm_max)
          {
            bpm_max = sum;
            bpm_max_phase = phase;
          }

        }

        Serial.printf("BPM %3d, had max sum %d at phase %d\n", bpm, bpm_max, bpm_max_phase );
      }


    }
//    delay( 1 );
  }

  int k;

  while (1)
  {
    k++;
    if (k == 1000)
    {
      Serial.println("task 1k");
      k = 0;
    }

    #define SAMPLES 64

    float sum = 0.0;
    for (auto i = 0; i < SAMPLES; i++)
    {
      auto newTime = micros();

      int x = analogRead( inputs::mic_pin ); // just read asap
      sum += x * x;
      while ((micros() - newTime) < 20) { /* chill */ }
    }

    int vu = sqrt( sum / SAMPLES );
    *((int *)vu_x) = vu;
  }
  #endif
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

  digitalWrite( outputs::pixels_en_pin, on );
  digitalWrite( outputs::mic_vdd_pin, on );
}

// loop() function -- runs repeatedly as long as board is on ---------------

void loop() {
  pixel_ticker_fn();
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
