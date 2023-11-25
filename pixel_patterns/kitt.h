#pragma once

#include "../pixel_pattern.h"

class KittPattern : public PixelPattern
{
private:
  static const int segments = 9;
  uint8_t seg[ segments ];
  int seg_pos;
  int phase;

public:
  KittPattern( )
    : seg_pos{ 0 }
    , phase{ 0 }
  {
    memset( seg, 0, sizeof(seg) );
  }

  virtual const char* name() { return "Knight Rider"; }
  virtual int interval_ms() { return 10; }

  void advance( int )
  {
    for (int i = 0; i < segments; i++)
      seg[i] = max(0, seg[i] - 4);


    int pos[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1 };
    const int num_pos = sizeof(pos) / sizeof(pos[0]);

    phase++;
    if (phase == 12)
    {
      seg_pos++;
      if (seg_pos >= num_pos)
        seg_pos = 0;

      seg[pos[seg_pos]] = 255;
      phase = 0;
    }
  }

  uint32_t pixel( unsigned int i )
  {
    //int slots[] = { 7, 15, 22, 30, 37, 45, 52, 59 };
    int slots[] = { 7, 13, 20, 26, 33, 39, 46, 52, 59 };

    int k = 0;
    for (; k < segments; k++)
      if (i < slots[k])
        break;

    return colour(seg[k],0,0);
  }
};
