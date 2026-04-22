#include "pet/pet_runtime.h"

#include "board/board_init.h"
#include "pet/demo_pet_frames.h"

namespace buddy {

void PetRuntime::begin() {
  animationPhase_ = 0;
  lastFrameAt_ = millis();
}

void PetRuntime::tick(const AppModel&) {
  if (millis() - lastFrameAt_ < 220) {
    return;
  }
  lastFrameAt_ = millis();
  animationPhase_ = (animationPhase_ + 1U) % 2U;
}

void PetRuntime::draw(int16_t x, int16_t y, uint16_t scale, PetState state) const {
  const DemoFrame& frame = kDemoFrames[currentFrame(state)];
  auto& canvas = frameBuffer();

  for (uint8_t row = 0; row < 16; ++row) {
    for (uint8_t col = 0; col < 16; ++col) {
      const char key = frame.rows[row][col];
      if (key == '.') {
        continue;
      }

      uint16_t color = TFT_WHITE;
      for (const auto& entry : kDemoPalette) {
        if (entry.key == key) {
          color = entry.color;
          break;
        }
      }
      canvas.fillRect(x + col * scale, y + row * scale, scale, scale, color);
    }
  }
}

uint8_t PetRuntime::currentFrame(PetState state) const {
  switch (state) {
    case PetState::Idle:
      return animationPhase_ == 0 ? 0 : 1;
    case PetState::Thinking:
      return animationPhase_ == 0 ? 0 : 2;
    case PetState::Busy:
      return animationPhase_ == 0 ? 3 : 4;
    case PetState::Waiting:
      return animationPhase_ == 0 ? 5 : 2;
    case PetState::Done:
      return animationPhase_ == 0 ? 6 : 1;
    case PetState::Error:
      return animationPhase_ == 0 ? 7 : 8;
    case PetState::Sleep:
      return 9;
    case PetState::Offline:
      return animationPhase_ == 0 ? 9 : 7;
  }
  return 0;
}

}  // namespace buddy
