#pragma once

#include <Arduino.h>
#include <M5Unified.h>

namespace buddy {

bool initializeBoard();
void applyBacklight(uint8_t brightness);
LGFX_Sprite& frameBuffer();
void beginFrame();
void presentFrame();

}  // namespace buddy
