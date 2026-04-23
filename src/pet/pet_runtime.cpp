#include "pet/pet_runtime.h"

#include "board/board_init.h"

namespace buddy {

namespace {

int8_t randomRange(const int8_t minValue, const int8_t maxValue) {
  if (minValue >= maxValue) {
    return minValue;
  }
  return static_cast<int8_t>(random(minValue, maxValue + 1));
}

uint16_t adjustFrameInterval(const PetAnimState state, const PetAnimVariant variant, const AppModel& model,
                             const uint16_t baseInterval) {
  int32_t interval = baseInterval;

  switch (state) {
    case PetAnimState::Sleep:
      if (model.petStats.energy <= 30) {
        interval += 60;
      }
      break;
    case PetAnimState::Idle:
      if (model.petStats.mood >= 75 && model.petStats.energy >= 60) {
        interval -= 30;
      }
      if (model.petStats.mood <= 30 || model.petStats.energy <= 30) {
        interval += 45;
      }
      break;
    case PetAnimState::Busy:
      if (model.petStats.focus >= 75) {
        interval -= 20;
      }
      if (model.petStats.focus <= 30) {
        interval += 30;
      }
      break;
    case PetAnimState::Attention:
      if (model.petStats.focus <= 30) {
        interval += 20;
      }
      break;
    case PetAnimState::Celebrate:
    case PetAnimState::Heart:
      if (model.petStats.mood >= 70) {
        interval -= 15;
      }
      break;
    case PetAnimState::Dizzy:
      break;
  }

  if (variant == PetAnimVariant::Blink) {
    interval -= 30;
  } else if (variant == PetAnimVariant::Focus) {
    interval -= 15;
  }

  if (interval < 90) {
    interval = 90;
  }
  return static_cast<uint16_t>(interval);
}

}  // namespace

void PetRuntime::begin() {
  baseState_ = PetAnimState::Idle;
  activeState_ = PetAnimState::Idle;
  fallbackState_ = PetAnimState::Idle;
  animationFrame_ = 0;
  animationDirection_ = 1;
  transientActive_ = false;
  lastFrameAt_ = millis();
  holdUntilAt_ = 0;
  resetProceduralState();
}

void PetRuntime::tick(const AppModel& model) {
  const PetAnimState resolvedBaseState = resolveBaseState(model);
  if (transientActive_) {
    if (resolvedBaseState != fallbackState_) {
      fallbackState_ = resolvedBaseState;
    }
  } else if (resolvedBaseState != activeState_) {
    if (resolvedBaseState == PetAnimState::Celebrate) {
      setState(PetAnimState::Heart, true, PetAnimState::Celebrate);
    } else {
      setState(resolvedBaseState);
    }
  }
  if (!transientActive_) {
    baseState_ = activeState_;
  }

  const uint32_t now = millis();
  updateProceduralMotion(model, now);

  const PetAnimClip& clip = clipFor(activeState_);
  if (holdUntilAt_ != 0 && now < holdUntilAt_) {
    return;
  }

  const uint16_t frameInterval = adjustFrameInterval(activeState_, activeVariant_, model, clip.frameIntervalMs);
  if (now - lastFrameAt_ < frameInterval) {
    return;
  }

  lastFrameAt_ = now;
  const bool loopCompleted = advanceFrame(clip);
  if (loopCompleted && variantLoopsRemaining_ > 0) {
    --variantLoopsRemaining_;
    if (variantLoopsRemaining_ == 0) {
      clearVariant();
    }
  }
}

void PetRuntime::draw(int16_t x, int16_t y, uint16_t scale, PetState) const {
  const DemoFrame& frame = kDemoFrames[currentFrameIndex()];
  auto& canvas = frameBuffer();

  for (uint8_t row = 0; row < kPetSpriteSize; ++row) {
    for (uint8_t col = 0; col < kPetSpriteSize; ++col) {
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
      canvas.fillRect(x + offsetX_ + col * scale, y + offsetY_ + row * scale, scale, scale, color);
    }
  }
}

PetAnimState PetRuntime::resolveBaseState(const AppModel& model) const {
  if (model.petStats.heartUntilMs > millis()) {
    return PetAnimState::Heart;
  }
  if (model.systemState == SystemState::ScreenSleep || model.snapshot.petState == PetState::Sleep) {
    return PetAnimState::Sleep;
  }
  if (model.faceDownSleepActive || model.petStats.energy <= 18) {
    return PetAnimState::Sleep;
  }
  if (model.motionDizzyUntilMs > millis()) {
    return PetAnimState::Dizzy;
  }
  if (!model.demoMode && model.runtimeState == RuntimeUiState::Degraded && model.petStats.mood <= 32) {
    return PetAnimState::Dizzy;
  }
  if (model.snapshot.requiresUser || model.snapshot.petState == PetState::Waiting) {
    return PetAnimState::Attention;
  }

  switch (model.snapshot.petState) {
    case PetState::Idle:
      return PetAnimState::Idle;
    case PetState::Thinking:
    case PetState::Busy:
      return model.petStats.focus >= 45 ? PetAnimState::Busy : PetAnimState::Idle;
    case PetState::Done:
      return model.petStats.mood >= 50 ? PetAnimState::Celebrate : PetAnimState::Idle;
    case PetState::Error:
    case PetState::Offline:
      return model.petStats.energy <= 25 ? PetAnimState::Sleep : PetAnimState::Dizzy;
    case PetState::Sleep:
      return PetAnimState::Sleep;
    case PetState::Waiting:
      return PetAnimState::Attention;
  }
  return PetAnimState::Idle;
}

void PetRuntime::setState(PetAnimState state, bool transient, PetAnimState fallback) {
  activeState_ = state;
  baseState_ = transient ? fallback : state;
  fallbackState_ = fallback;
  transientActive_ = transient;
  animationFrame_ = 0;
  animationDirection_ = 1;
  lastFrameAt_ = millis();
  holdUntilAt_ = 0;
  resetProceduralState();
}

bool PetRuntime::advanceFrame(const PetAnimClip& clip) {
  if (clip.count <= 1) {
    return false;
  }

  bool loopCompleted = false;
  holdUntilAt_ = 0;

  switch (clip.loopMode) {
    case PetLoopMode::Loop:
      animationFrame_ = static_cast<uint8_t>((animationFrame_ + 1U) % clip.count);
      loopCompleted = animationFrame_ == 0U;
      if (animationFrame_ == clip.count - 1U && clip.holdOnLastFrameMs != 0) {
        holdUntilAt_ = millis() + clip.holdOnLastFrameMs;
      } else if (animationFrame_ == 0U && clip.randomIdlePause) {
        holdUntilAt_ = millis() + static_cast<uint32_t>(random(80, 260));
      }
      break;

    case PetLoopMode::PingPong:
      if (animationDirection_ > 0) {
        if (animationFrame_ + 1U >= clip.count) {
          animationDirection_ = -1;
          animationFrame_ = clip.count - 2U;
          holdUntilAt_ = clip.holdOnLastFrameMs != 0 ? millis() + clip.holdOnLastFrameMs : 0;
        } else {
          ++animationFrame_;
        }
      } else if (animationFrame_ == 0U) {
        animationDirection_ = 1;
        animationFrame_ = 1U;
        holdUntilAt_ = clip.randomIdlePause ? millis() + static_cast<uint32_t>(random(80, 220)) : 0;
        loopCompleted = true;
      } else {
        --animationFrame_;
        if (animationFrame_ == 0U) {
          loopCompleted = true;
        }
      }
      break;

    case PetLoopMode::PlayOnce:
      if (animationFrame_ + 1U < clip.count) {
        ++animationFrame_;
        if (animationFrame_ == clip.count - 1U && clip.holdOnLastFrameMs != 0) {
          holdUntilAt_ = millis() + clip.holdOnLastFrameMs;
        }
      } else if (transientActive_) {
        setState(fallbackState_);
        loopCompleted = true;
      } else if (clip.holdOnLastFrameMs != 0) {
        holdUntilAt_ = millis() + clip.holdOnLastFrameMs;
        loopCompleted = true;
      }
      break;
  }

  const PetMotionProfile& motion = motionProfileFor(activeState_);
  if (holdUntilAt_ == 0 && motion.pauseChancePct > 0 && random(100) < motion.pauseChancePct) {
    holdUntilAt_ = millis() + static_cast<uint32_t>(random(50, 140));
  }

  return loopCompleted;
}

void PetRuntime::resetProceduralState() {
  activeVariant_ = PetAnimVariant::Base;
  variantLoopsRemaining_ = 0;
  offsetX_ = 0;
  offsetY_ = 0;
  const uint32_t now = millis();
  nextMicroMotionAt_ = now;
  nextIdleActionAt_ = now + static_cast<uint32_t>(random(2200, 4200));
}

void PetRuntime::updateProceduralMotion(const AppModel& model, uint32_t now) {
  updateOffsets(model, now);
  maybeTriggerVariant(model, now);
}

void PetRuntime::maybeTriggerVariant(const AppModel& model, uint32_t now) {
  if (transientActive_ || now < nextIdleActionAt_) {
    return;
  }

  const PetMotionProfile& motion = motionProfileFor(activeState_);
  nextIdleActionAt_ = now + static_cast<uint32_t>(random(2600, 6200));
  if (random(100) >= motion.variantChancePct) {
    return;
  }

  switch (activeState_) {
    case PetAnimState::Idle:
      if (random(100) < motion.rareActionChancePct) {
        activeVariant_ = PetAnimVariant::Blink;
      } else {
        activeVariant_ = model.petStats.mood >= 65 ? PetAnimVariant::Tilt : PetAnimVariant::Blink;
      }
      variantLoopsRemaining_ = 1;
      break;
    case PetAnimState::Busy:
      activeVariant_ = PetAnimVariant::Focus;
      variantLoopsRemaining_ = 2;
      break;
    case PetAnimState::Sleep:
      if (random(100) < motion.rareActionChancePct) {
        activeVariant_ = PetAnimVariant::Blink;
        variantLoopsRemaining_ = 1;
      }
      break;
    case PetAnimState::Attention:
      activeVariant_ = PetAnimVariant::Tilt;
      variantLoopsRemaining_ = 1;
      break;
    case PetAnimState::Celebrate:
    case PetAnimState::Dizzy:
    case PetAnimState::Heart:
      break;
  }

  if (variantLoopsRemaining_ > 0) {
    animationFrame_ = 0;
    animationDirection_ = 1;
    holdUntilAt_ = 0;
  }
}

void PetRuntime::updateOffsets(const AppModel& model, uint32_t now) {
  if (now < nextMicroMotionAt_) {
    return;
  }

  PetMotionProfile motion = motionProfileFor(activeState_);
  if (model.petStats.energy <= 30) {
    motion.maxOffsetY = static_cast<int8_t>(motion.maxOffsetY > 1 ? motion.maxOffsetY - 1 : motion.maxOffsetY);
  }
  if (activeState_ == PetAnimState::Busy && model.petStats.focus >= 75) {
    motion.minOffsetX = -1;
    motion.maxOffsetX = 1;
    motion.maxOffsetY = static_cast<int8_t>(motion.maxOffsetY > 1 ? motion.maxOffsetY - 1 : motion.maxOffsetY);
  }
  if (activeState_ == PetAnimState::Idle && model.petStats.mood >= 75 && model.petStats.energy >= 60) {
    motion.maxOffsetY = static_cast<int8_t>(motion.maxOffsetY < 5 ? motion.maxOffsetY + 1 : motion.maxOffsetY);
  }

  switch (activeVariant_) {
    case PetAnimVariant::Base:
      offsetX_ = randomRange(motion.minOffsetX, motion.maxOffsetX);
      offsetY_ = randomRange(motion.minOffsetY, motion.maxOffsetY);
      break;
    case PetAnimVariant::Blink:
      offsetX_ = 0;
      offsetY_ = randomRange(motion.minOffsetY, motion.maxOffsetY);
      break;
    case PetAnimVariant::Tilt:
      offsetX_ = random(2) == 0 ? motion.minOffsetX : motion.maxOffsetX;
      offsetY_ = randomRange(motion.minOffsetY, motion.maxOffsetY);
      break;
    case PetAnimVariant::Focus:
      offsetX_ = 0;
      offsetY_ = randomRange(0, motion.maxOffsetY > 1 ? 1 : motion.maxOffsetY);
      break;
  }

  if (activeState_ == PetAnimState::Heart) {
    offsetY_ = -1;
  } else if (activeState_ == PetAnimState::Celebrate) {
    offsetY_ = randomRange(0, 3);
  }

  nextMicroMotionAt_ =
      now + static_cast<uint32_t>(random(motion.minMotionIntervalMs, motion.maxMotionIntervalMs + 1));
}

void PetRuntime::clearVariant() {
  activeVariant_ = PetAnimVariant::Base;
  variantLoopsRemaining_ = 0;
}

uint8_t PetRuntime::currentFrameIndex() const {
  const PetAnimClip& clip = clipFor(activeState_);

  switch (activeState_) {
    case PetAnimState::Sleep:
      if (activeVariant_ == PetAnimVariant::Blink) {
        constexpr uint8_t kSleepBlinkFrames[] = {1, 2, 1};
        return static_cast<uint8_t>(clip.start + kSleepBlinkFrames[animationFrame_ % 3U]);
      }
      break;
    case PetAnimState::Idle:
      if (activeVariant_ == PetAnimVariant::Blink) {
        constexpr uint8_t kIdleBlinkFrames[] = {0, 4, 0};
        return static_cast<uint8_t>(clip.start + kIdleBlinkFrames[animationFrame_ % 3U]);
      }
      if (activeVariant_ == PetAnimVariant::Tilt) {
        constexpr uint8_t kIdleTiltFrames[] = {2, 3, 2, 1};
        return static_cast<uint8_t>(clip.start + kIdleTiltFrames[animationFrame_ % 4U]);
      }
      break;
    case PetAnimState::Busy:
      if (activeVariant_ == PetAnimVariant::Focus) {
        constexpr uint8_t kBusyFocusFrames[] = {0, 1, 0, 2};
        return static_cast<uint8_t>(clip.start + kBusyFocusFrames[animationFrame_ % 4U]);
      }
      break;
    case PetAnimState::Attention:
      if (activeVariant_ == PetAnimVariant::Tilt) {
        constexpr uint8_t kAttentionTiltFrames[] = {0, 1, 2, 1};
        return static_cast<uint8_t>(clip.start + kAttentionTiltFrames[animationFrame_ % 4U]);
      }
      break;
    case PetAnimState::Celebrate:
    case PetAnimState::Dizzy:
    case PetAnimState::Heart:
      break;
  }

  return static_cast<uint8_t>(clip.start + animationFrame_);
}

}  // namespace buddy
