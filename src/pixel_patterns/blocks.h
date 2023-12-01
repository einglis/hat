#pragma once

#include "../pixel_pattern.h"

class BlockPattern2 : public PixelPattern
{
private:
  static const int segments = 9;
  uint8_t seg[ segments ];
  int seg_pos;
  int phase;

public:
  BlockPattern2( )
    : seg_pos{ 0 }
    , phase{ 0 }
  {
    memset( seg, 0, sizeof(seg) );
  }

  virtual const char* name() { return "Blocks"; }
  virtual int interval_ms() { return 1000; }

  void advance( int )
  {
    phase++;
  }

  uint32_t pixel( unsigned int i )
  {
    int slots[] = { 7, 13, 20, 26, 33, 39, 46, 52, 59 };

    int k = i / 4;
//    for (; k < segments; k++)
//      if (i < slots[k])
//        break;

    if (phase & 1)
      return (k & 1) ? 0x80FF00 : 0x8000FF; // amazing lime green or off
    else
      return (k & 1) ? 0x8000FF : 0x80FF00; // off or rich magenta
  }
};

class BlockPattern3 : public PixelPattern
{
private:
  static const int segments = 9;
  uint8_t seg[ segments ];
  int seg_pos;
  int phase;

public:
  BlockPattern3( )
    : seg_pos{ 0 }
    , phase{ 0 }
  {
    memset( seg, 0, sizeof(seg) );
  }

  virtual const char* name() { return "Blocks"; }
  virtual int interval_ms() { return 100; }

  void advance( int )
  {
    phase++;

    seg[phase&3] = rand();

  }

  uint32_t pixel( unsigned int i )
  {
    int k = i / 4;

    int x = 0;

    for (int ii = 0, i = phase & 3; ii < 4; ii++, i = (i+1)%4)
    {
      int start = seg[i] & 0x0f;
      int len = ((seg[i] & 0x30) >> 4) + 2;
      //int col = ((uint8_t)(seg[i] * 0x23) << 16) | ((uint8_t)(seg[i] * 0xef) << 8) | ((uint8_t)(seg[i] * 0x41) << 0);



      if (k >= start && k < start+len)
        x |= (seg[i] << (i*8)); 
        //x = col;
    }

    if ((x & 0xffffff) == 0)
      x = 0xffffff;

    return x;
  }
};



class BlockPattern : public PixelPattern
{
private:
  static const int segments = 9;
  uint32_t vals[3];
  int phase;

public:
  BlockPattern( )
    : phase{ 0 }
  {
  }

  virtual const char* name() { return "Blocks"; }
  virtual int interval_ms() { return 100; }

  void advance( int )
  {
    phase++;

    vals[phase % 3] = rand();
  }

  uint32_t pixel( unsigned int i )
  {
    int k = i / 2;

    int c = 0x300060;

    if (vals[0] & (1 << k)) c += 0x005000;
    if (vals[1] & (1 << k)) c += 0x005000;
    if (vals[2] & (1 << k)) c += 0x402000;

    return c;
  }
};
