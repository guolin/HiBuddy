#pragma once

#include <Arduino.h>

namespace buddy {

enum class PetAnimState : uint8_t {
  Sleep,
  Idle,
  Busy,
  Attention,
  Celebrate,
  Dizzy,
  Heart,
};

enum class PetLoopMode : uint8_t {
  Loop,
  PingPong,
  PlayOnce,
};

struct DemoFrame {
  const char* rows[16];
};

struct DemoPalette {
  char key;
  uint16_t color;
};

struct PetAnimClip {
  uint8_t start;
  uint8_t count;
  uint16_t frameIntervalMs;
  uint16_t holdOnLastFrameMs;
  PetLoopMode loopMode;
  bool randomIdlePause;
};

constexpr DemoPalette kDemoPalette[] = {
    {'.', 0x0000},
    {'n', 0x3987},
    {'b', 0x7AED},
    {'t', 0xDEDB},
    {'p', 0xDA7A},
    {'w', 0xFFFF},
    {'r', 0xD1E8},
    {'y', 0xFEE0},
    {'h', 0xFB2C},
};

constexpr DemoFrame kDemoFrames[] = {
    // S00 sleep
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbttttttttbb..", "..btt..tt..ttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", ".....bb..bb.....", "......b..b......",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbttttttttbb..", "..btt..tt..ttb..", "..bttttttttttb..", "..btttnnnttttb..", "..bttt.rr.tttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", ".....b....b.....", "......b..b......",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbttttttttbb..", "..btt..tt..ttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", ".....bbbbbb.....", "....bb....bb....", ".....b....b.....",
      "................"}},

    // S01 idle
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", ".....bb..bb.....", "......b..b......",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbt..tt..tbb..", "..btt..tt..ttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", ".....bb..bb.....", "......b..b......",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtwwtttttbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", ".....bb..bb.....", "......b..b......",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbttttwwtbb...", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", ".....bb..bb.....", "......b..b......",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..bttt.rr.tttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "....b......b....", ".....bb..bb.....",
      "................"}},

    // S02 busy
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtyyttyytbb..", "..btttyyttyttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "......bbbb......", ".....bb..bb.....",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbt.yy.yy.bb..", "..btttyyttyttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", ".....b....b.....", ".....bb..bb.....",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtyyttyytbb..", "..btttyyttyttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", ".....bbbbbb.....", "....bb....bb....", "......b..b......",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbt.yy.yy.bb..", "..btttyyttyttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "...bb......bb...", ".....b....b.....",
      "................"}},

    // S03 attention
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...btttyyyttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", ".....bb..bb.....", "...y..b..b..y...", "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...btttyyyttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "..y..bb..bb..y..", ".....b....b.....",
      "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...btttyyyttb...",
      "..bbtyyttyytbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "..y..bb..bb..y..", "...y..b..b..y...", "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...btttyyyttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "...y.bb..bb.y...", "....y.b..b.y....", "................"}},

    // S04 celebrate
    {{"................", ".....bb..bb.....", "....bppbbppb....", "..ybbpttttpbby..", "...bttttttttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..bttrrrrrrttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "..y..bb..bb..y..", "...y..b..b..y...", "................"}},
    {{"................", ".....bb..bb.....", "..y.bppbbppb.y..", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..bttrrrrrrttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "..y.bbbbbbbby...", ".....bb..bb.....", "...y..b..b..y...", "................"}},
    {{"................", "...y.bb..bb.y...", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..bttrrrrrrttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "..y..bb..bb..y..", ".....b....b.....", "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "..ybttttttttby..",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..bttrrrrrrttb..",
      "..bbttttttttbb..", "..ybbttttttbby..", "....bbbbbbbb....", ".....bb..bb.....", "...y..b..b..y...", "................"}},

    // S05 dizzy
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtyyttyyttbb.", "..btt.yyttyy.tt.", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", ".....bb..bb.....", "......b..b......", "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbt.yy.yy.tbb.", "..bttyyttyyyttb.", "..bttttttttttb..", "..btttnnnttttb..", "..bttt.rr.tttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "....b......b....", ".....bb..bb.....", "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtyyttyytbb..", "..btt.yyttyy.tt.", "..bttttttttttb..", "..btttnnnttttb..", "..btttrrrrtttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbbbbbbb....", "...bb......bb...", "......b..b......", "................"}},

    // S06 heart
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..bttrrrrrrttb..",
      "..bbttttttttbb..", "...bbttttttbb...", ".....bbhhbb.....", "....bbhhhhbb....", ".....bhhhhb.....", "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..bttrrrrrrttb..",
      "..bbttttttttbb..", "...bbttttttbb...", "....bbhhhhbb....", "...bbhhhhhhbb...", "....bbhhhhbb....", "................"}},
    {{"................", ".....bb..bb.....", "....bppbbppb....", "...bbpttttpbb...", "...bttttttttb...",
      "..bbtwwttwwtbb..", "..bttwwttwwttb..", "..bttttttttttb..", "..btttnnnttttb..", "..bttrrrrrrttb..",
      "..bbttttttttbb..", "...bbttttttbb...", ".....bbhhbb.....", ".....bhhhhb.....", "......bhhb......", "................"}},
};

constexpr PetAnimClip kPetAnimClips[] = {
    {0, 3, 420, 180, PetLoopMode::PingPong, false},   // sleep
    {3, 5, 260, 160, PetLoopMode::Loop, true},        // idle
    {8, 4, 150, 0, PetLoopMode::Loop, false},         // busy
    {12, 4, 180, 60, PetLoopMode::Loop, false},       // attention
    {16, 4, 130, 120, PetLoopMode::Loop, false},      // celebrate
    {20, 3, 200, 80, PetLoopMode::PingPong, false},   // dizzy
    {23, 3, 140, 160, PetLoopMode::PlayOnce, false},  // heart
};

inline const PetAnimClip& clipFor(PetAnimState state) {
  return kPetAnimClips[static_cast<uint8_t>(state)];
}

}  // namespace buddy
