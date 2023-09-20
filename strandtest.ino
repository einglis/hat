
#include <Adafruit_NeoPixel.h>
#include <vector>

#include "button.h"

namespace outputs {
  enum {
    status_pin = LED_BUILTIN, // 10
    pixels_pin = 0,
    pixels_en_pin = 1,
    button_en_pin = 5,
    mic_vdd_pin = 9,
    mic_gnd_pin = 8, // remove 10k pullup
  };
}
namespace inputs {
  enum {
    button_pin = 7,
    vsense_pin = A3,
    mic_pin = A2,
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

  pinMode( outputs::status_pin, OUTPUT );

  pinMode( inputs::vsense_pin, INPUT );
  adcAttachPin( inputs::vsense_pin );

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
  adcAttachPin( inputs::mic_pin );


  strip.begin();
  strip.setBrightness( PIXEL_BRIGHTNESS );
  strip.clear();
  strip.show();

  //button.begin( button_fn );


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


#include <driver/adc.h>
#include <driver/i2s.h>
#include "esp32_audio/ADCSampler.h"

i2s_config_t adcI2SConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_LSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};



TaskHandle_t vu_task;
void vu_task_fn( void* vu_x ) 
{
  Serial.print("vu_task_fn() running on core ");
  Serial.println(xPortGetCoreID());

  int k = 0;
  while (1)
  {
    k++;
    if (k == 10)
    {
      Serial.println("task 1k");
      k = 0;
    }

    #define SAMPLES 64

    //float sum = 0.0;
    for (auto i = 0; i < SAMPLES; i++) 
    {
      //auto newTime = micros();

      int x = analogRead( inputs::mic_pin ); // just read asap
      (void)x;
      //sum += x * x;
      //while ((micros() - newTime) < 20) { /* chill */ }
    }

    //int vu = sqrt( sum / SAMPLES );
    *((int *)vu_x) = k;//vu;
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
  static int on = 0;

  if (e == ButtonInput::HoldShort)
    on = !on;
  else if (e == ButtonInput::Press)
    cycle_pattern();

  digitalWrite( outputs::pixels_en_pin, on );
  digitalWrite( outputs::mic_vdd_pin, on );
}

// loop() function -- runs repeatedly as long as board is on ---------------

void loop() {
  //pixel_ticker_fn();
  return;
  int adc_raw = analogRead( inputs::mic_pin );
  Serial.println(adc_raw);
    // this println kills uploads?!

  delay( 100 ); // seems to make programming more reliable.

  return;
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
