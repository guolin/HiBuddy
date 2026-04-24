#pragma once

#include "app_types.h"
#include "pet/demo_pet_frames.h"

namespace buddy {

enum class PetAnimVariant : uint8_t {
  Base,
  Blink,
  Tilt,
  Focus,
};

class PetRuntime {
 public:
  void begin();
  void onShake();
  void tick(const AppModel& model);
  void draw(int16_t x, int16_t y, uint16_t scale, PetState state) const;

 private:
  PetAnimState baseState_{PetAnimState::Idle};
  PetAnimState activeState_{PetAnimState::Idle};
  PetAnimState fallbackState_{PetAnimState::Idle};
  PetAnimVariant activeVariant_{PetAnimVariant::Base};
  PetAnimVariant pendingVariant_{PetAnimVariant::Base};
  PetAnimVariant lastVariant_{PetAnimVariant::Base};
  uint8_t animationFrame_{0};
  uint8_t lastBaseFrame_{0};
  int8_t lastVariantOffsetX_{0};
  int8_t lastVariantOffsetY_{0};
  int8_t animationDirection_{1};
  int8_t offsetX_{0};
  int8_t offsetY_{0};
  int8_t offsetTargetX_{0};
  int8_t offsetTargetY_{0};
  bool transientActive_{false};
  bool celebrateConsumed_{false};
  bool pendingShakeWhileDizzy_{false};
  uint32_t lastFrameAt_{0};
  uint32_t holdUntilAt_{0};
  uint32_t nextMicroMotionAt_{0};
  uint32_t nextIdleActionAt_{0};
  uint32_t nextPoseChangeAt_{0};
  uint32_t lastOffsetStepAt_{0};
  uint8_t variantLoopsRemaining_{0};
  uint8_t pendingVariantLoopsRemaining_{0};
  uint8_t celebrateLoopsRemaining_{0};

  PetAnimState resolveBaseState(const AppModel& model) const;
  void setState(PetAnimState state, bool transient = false, PetAnimState fallback = PetAnimState::Idle);
  bool advanceFrame(const PetAnimClip& clip);
  void resetProceduralState(bool preserveOffset = false);
  void updateProceduralMotion(const AppModel& model, uint32_t now);
  void maybeTriggerVariant(const AppModel& model, uint32_t now);
  void updateOffsets(const AppModel& model, uint32_t now);
  void applyPendingVariant();
  void clearVariant();
  uint8_t currentFrameIndex() const;
};

}  // namespace buddy
