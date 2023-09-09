
#include <Adafruit_NeoPixel.h>

namespace outputs {
  enum {
    status_pin = LED_BUILTIN,
    pixels_pin    = 0,
    pixels_en_pin = 1,
    button_en_pin = 5,
  };
}
namespace inputs {
  enum {
    button_pin = 7,
    vsense_pin = A3,
  };
}

#define LED_COUNT 59 // really 60, but the last one is mostly hidden

Adafruit_NeoPixel strip( LED_COUNT, outputs::pixels_pin, NEO_GRB + NEO_KHZ800 );


void myISR() 
{
  int button = digitalRead(7);
  digitalWrite(1, button);
  Serial.println(button);
}

// setup() function -- runs once at startup --------------------------------

void setup() 
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("Hat's hat!");

  pinMode( inputs::button_pin, INPUT_PULLDOWN );
  pinMode( inputs::vsense_pin, INPUT );
  adcAttachPin( inputs::vsense_pin );

  pinMode( outputs::status_pin, OUTPUT );
  pinMode( outputs::pixels_pin, OUTPUT );
  pinMode( outputs::pixels_en_pin, OUTPUT );
  pinMode( outputs::button_en_pin, OUTPUT );

  digitalWrite( outputs::pixels_en_pin, LOW ); // pixels off
  digitalWrite( outputs::button_en_pin, HIGH ); // needed to recognise presses


  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(64); // Set BRIGHTNESS to about 1/5 (max = 255)
  //strip.setBrightness(255); // Set BRIGHTNESS to about 1/5 (max = 255)


  attachInterrupt(digitalPinToInterrupt(7), myISR, CHANGE);
}


// loop() function -- runs repeatedly as long as board is on ---------------

void loop() {
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

  int adc_raw = analogRead(A3);
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

    for (int i = 0; i < LED_COUNT; i++) {
//      strip.setPixelColor(i, strip.getPixelColor(i)&0xff); // blue 
//    strip.setPixelColor(i, strip.getPixelColor(i)&0xff00); // green
//      strip.setPixelColor(i, strip.getPixelColor(i)&0xff0000); // red
 //       strip.setPixelColor(i, (strip.getPixelColor(i)&0xff)*0x10101); // white
//        strip.setPixelColor(i, (strip.getPixelColor(0)&0xff)*0x10101); // solid white
  }

    for (int i = percent*LED_COUNT/100; i < LED_COUNT; i++) {
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
        
    strip.setPixelColor(LED_COUNT-1, 0xff0000); // red on left
    strip.setPixelColor(LED_COUNT-2, 0xff0000); // red on left
    strip.setPixelColor(LED_COUNT-3, 0x800000); // red on left
    strip.setPixelColor(LED_COUNT-4, 0x400000); // red on left
    strip.setPixelColor(LED_COUNT-5, 0x200000); // red on left
    strip.setPixelColor(LED_COUNT-6, 0x100000); // red on left
    strip.setPixelColor(LED_COUNT-7, 0x080000); // red on left
    strip.setPixelColor(LED_COUNT-8, 0x040000); // red on left
    strip.setPixelColor(LED_COUNT-9, 0x020000); // red on left
    strip.setPixelColor(LED_COUNT-10, 0x010000); // red on left
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
