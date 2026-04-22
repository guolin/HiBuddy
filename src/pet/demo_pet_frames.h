#pragma once

#include <Arduino.h>

namespace buddy {

struct DemoFrame {
  const char* rows[16];
};

struct DemoPalette {
  char key;
  uint16_t color;
};

constexpr DemoPalette kDemoPalette[] = {
    {'.', 0x0000},
    {'o', 0xFD20},
    {'b', 0x61A8},
    {'e', 0x18C3},
    {'m', 0xA145},
    {'h', 0xFFFF},
};

constexpr DemoFrame kDemoFrames[] = {
    {{"................", "......oo........", ".....oooo.......", "....obbbbo......", "...obbbbboo.....",
      "...obhhhboo.....", "..oobhhhbboo....", "..obbbbbbbbo....", "..obommbmobo....", "..obbbbbbbbo....",
      "...obbbbbbo.....", "...oobbbboo.....", "....oooooo......", ".....oooo.......", "......oo........",
      "................"}},
    {{"................", "......oo........", ".....oooo.......", "....obbbbo......", "...obbbbboo.....",
      "...obh..boo.....", "..oobh..bboo....", "..obbbbbbbbo....", "..obommbmobo....", "..obbbbbbbbo....",
      "...obbbbbbo.....", "...oobbbboo.....", "....oooooo......", ".....oooo.......", "......oo........",
      "................"}},
    {{"................", "......oo........", ".....oooo.......", "....obbbbo......", "...obbbbboo.....",
      "...obhhhboo.....", "..oobhhhbboo....", "..obbbbbbbbo....", "..obohhmhobo....", "..obbbbbbbbo....",
      "...obbbbbbo.....", "...oobbbboo.....", "....oooooo......", ".....oooo.......", "......oo........",
      "................"}},
    {{"................", "......oo........", ".....oooo.......", "....obbbbo......", "...obbbbboo.....",
      "...obhhhboo.....", "..oobhhhbboo....", "..obbbbbbbbo....", "..obmommombo....", "..obbbbbbbbo....",
      "...obbbbbbo.....", "...oobbbboo.....", "....oooooo......", ".....oooo.......", "...o..oo..o.....",
      "................"}},
    {{"................", "......oo........", "...o.oooo.o.....", "....obbbbo......", "...obbbbboo.....",
      "...obhhhboo.....", "..oobhhhbboo....", "..obbbbbbbbo....", "..obmommombo....", "..obbbbbbbbo....",
      "...obbbbbbo.....", "...oobbbboo.....", "....oooooo......", ".....oooo.......", "......oo........",
      "................"}},
    {{"........h.......", "......oooh......", ".....oooooh.....", "....obbbboh.....", "...obbbbbooh....",
      "...obhhhbooh....", "..oobhhhbbooh...", "..obbbbbbbboh...", "..obommbmbooh...", "..obbbbbbbboh...",
      "...obbbbbboh....", "...oobbbbooh....", "....oooooo......", ".....oooo.......", "......oo........",
      "................"}},
    {{"................", "......oo........", ".....oooo.......", "....obbbbo......", "...obbbbboo.....",
      "...obhhhboo.....", "..oobh..bboo....", "..obbbbbbbbo....", "..obhmmmhbbo....", "..obbbbbbbbo....",
      "...obbbbbbo.....", "...oobbbboo.....", "...o.oooo.o.....", "..o..oooo..o....", "......oo........",
      "................"}},
    {{"................", "......oo........", ".....oooo.......", "....obbbbo......", "...obbbbboo.....",
      "...obhhhboo.....", "..oobhhhbboo....", "..obbbbbbbbo....", "..obmb..bmbo....", "..obbbmmbbbo....",
      "...obbbbbbo.....", "...oobbbboo.....", "....oooooo......", "...m.oooo.m.....", "......oo........",
      "................"}},
    {{"................", "......oo........", ".....oooo.......", "....obbbbo......", "...obbbbboo.....",
      "...obhhhboo.....", "..oobhhhbboo....", "..obbbbbbbbo....", "..obm....mbo....", "..obbbmmbbbo....",
      "...obbbbbbo.....", "...oobbbboo.....", "..m.oooooo.m....", ".....oooo.......", "......oo........",
      "................"}},
    {{"................", "................", "......oo........", ".....oooo.......", "....obbbbo......",
      "...obbbbboo.....", "...ob....oo.....", "..oob....boo....", "..ob..zz..bo....", "..obbbbbbbbo....",
      "...obbbbbbo.....", "...oobbbboo.....", "....oooooo......", ".....oooo.......", "......oo........",
      "................"}},
};

}  // namespace buddy
