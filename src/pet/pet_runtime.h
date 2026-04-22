#pragma once

#include "app_types.h"
#include "pet/demo_pet_frames.h"

namespace buddy {

class PetRuntime {
 public:
  void begin();
  void tick(const AppModel& model);
  void draw(int16_t x, int16_t y, uint16_t scale, PetState state) const;

 private:
  PetAnimState activeState_{PetAnimState::Idle};
  PetAnimState fallbackState_{PetAnimState::Idle};
  uint8_t animationFrame_{0};
  int8_t animationDirection_{1};
  bool transientActive_{false};
  uint32_t lastFrameAt_{0};
  uint32_t holdUntilAt_{0};

  PetAnimState resolveBaseState(const AppModel& model) const;
  void setState(PetAnimState state, bool transient = false, PetAnimState fallback = PetAnimState::Idle);
  void advanceFrame(const PetAnimClip& clip);
  uint8_t currentFrameIndex() const;
};

}  // namespace buddy
