#pragma once

#include "app_types.h"

namespace buddy {

class PetRuntime {
 public:
  void begin();
  void tick(const AppModel& model);
  void draw(int16_t x, int16_t y, uint16_t scale, PetState state) const;

 private:
  uint8_t animationPhase_{0};
  uint32_t lastFrameAt_{0};

  uint8_t currentFrame(PetState state) const;
};

}  // namespace buddy
