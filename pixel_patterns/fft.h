#pragma once

#include <arduinoFFT.h>
#include "../pixel_pattern.h"

#define SAMPLES         64   // Must be a power of 2. Don't use sample 0 and only first SAMPLES/2 are usable.
#define SAMPLING_FREQ   80000 // Hz, must be 40000 or less due to ADC conversion time.
#define NOISE           500   // Used as a crude noise filter, values below this are ignored

class FFTPattern : public PixelPattern
{
public:
  FFTPattern( int en_pin, int input_pin )
    : fft{ vReal, vImag, SAMPLES, SAMPLING_FREQ }
    , en_pin{ en_pin }
    , mic_pin{ input_pin }
    , min{ 1000 }
    , max{ 0 }
  { }

  virtual void activate()
  {
    // enable mic
    //digitalWrite( en_pin, HIGH );
  }
  virtual void deactivate()
  {
    // disable mic
    //digitalWrite( en_pin, LOW );
  }

  virtual void advance2( int /*inc*/ )
  {
    static int k = 0;
    k++;
    if (k == 1000)
    {
      Serial.println("1k");
      k = 0;
    }

    //  unsigned int sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));

    for (auto i = 0; i < SAMPLES; i++)
    {
      newTime = micros();
      vReal[i] = analogRead( mic_pin ); // A conversion takes about 9.7uS on an ESP32
      vImag[i] = 0;
      //while ((micros() - newTime) < sampling_period_us) { /* chill */ }
    }

    float sum = 0.0;
    for (auto i = 0; i < SAMPLES; i++)
      sum = vReal[i] * vReal[i];
    //Serial.print("sum sq: ");
    //Serial.print(sum);
    //Serial.print(", av: ");

    sum /= SAMPLES;
    //Serial.print(sum);
    //Serial.print(", sqrt: ");
    sum = sqrt(sum);
    //Serial.print(sum);
    // sum now expected in range 0..4096

    //Serial.print(", new: ");


    float vu__ = sum;

    // long terms
    if (sum > max)
      max = sum;
    else
      max *= 0.99;

    if (sum < min)
      min = sum;
    else
      min /= 0.99;

    static int x = 0;
    x++;
    if (x > 100)
    {
        Serial.println(min);
        Serial.println(max);
        x = 0;
    }


    if (vu__ > vu)
      vu = vu__;
    else
      vu = (63 * (long)vu + vu__) / 64;


    vu2 = vu;
     if ((vu2 - 170) > 0)
      vu2 -= 170;
    else
      vu2 = 0;

    vu2 /= 6;// vu2 * NUM_PIXELS * 8 / 4096;
    //Serial.print(sum);


    auto max = 0;
    for (auto i = 0; i < SAMPLES; i++)
      if (vReal[i] > max)
        max = vReal[i];
    //Serial.print(", max: ");

  //  Serial.println(vu);

    // maths!
    // fft.DCRemoval();
    // fft.Windowing( FFT_WIN_TYP_HAMMING, FFT_FORWARD );
    // fft.Compute( FFT_FORWARD );
    // fft.ComplexToMagnitude();
  }

 virtual void advance( int /*inc*/ )
  {
    static int k = 0;
    k++;
    if (k == 1000)
    {
      Serial.println("leds 1k");
      k = 0;
    }

    int raw_vu = global_vu / 16;


    static int dec = 10;

    if (raw_vu > vu2)
    {
      vu2 = raw_vu;
      dec = 0;
    }
    else
    {
        // 10 or less is very rapid; 30 is a bit sluggish
      if (dec == 16 && vu2)
      {
        vu2 -= 1;//= (31*vu2+raw_vu)/32;
        dec = 0;
      }

      dec++;
    }

  }

  virtual uint32_t pixel( unsigned int i )
  {
    const int mid = NUM_PIXELS / 2;  // 59 / 2 --> 29
      // but actually, pixels 28 and 29 straddle the centre line



    int pos = 0;

    if (i < (mid-1))
      pos = mid - 1 - i;
    else if (i > mid)
      pos = i - mid;

    int bright = vu2 * 10;
    if (bright > 255)
      bright = 255;

    uint32_t col = bright * 0x000100; // green
    if (pos > 16)
      col = bright * 0x010000; // red
    else if (pos > 9)
      col = bright * 0x010100; // yellow



    if (vu2 && pos < (vu2-1))
      return col;
    else
      return 0;

    if (i > mid && (i-mid)<=vu)
      return col;
    else if (i < mid && (mid-i)<=vu)
      return col;
    else
      return 0;
    //Serial.println( vReal[i] );
    return vReal[i] / 100;
  }

private:
  arduinoFFT fft;
  int en_pin;
  int mic_pin;
  float vu;
  float min;
  float max;
  int vu2;

  double vReal[SAMPLES];
  double pReal[SAMPLES];
  double vImag[SAMPLES];
  unsigned long newTime;
};