#include "pet/pet_runtime.h"

#include "board/board_init.h"

#include "../../arduino_pack.h"

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
        interval -= 10;
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
  lastBaseFrame_ = 0;
  lastVariantOffsetX_ = 0;
  lastVariantOffsetY_ = 0;
  animationDirection_ = 1;
  transientActive_ = false;
  celebrateConsumed_ = false;
  pendingShakeWhileDizzy_ = false;
  celebrateLoopsRemaining_ = 0;
  lastFrameAt_ = millis();
  holdUntilAt_ = 0;
  resetProceduralState();
}

void PetRuntime::onShake() {
  pendingShakeWhileDizzy_ = true;
  if (activeState_ != PetAnimState::Dizzy) {
    return;
  }

  const PetAnimClip& clip = clipFor(PetAnimState::Dizzy);
  uint8_t nextFrame = lastBaseFrame_;
  while (nextFrame == lastBaseFrame_) {
    nextFrame = static_cast<uint8_t>(random(clip.count));
  }
  lastBaseFrame_ = nextFrame;
  lastFrameAt_ = millis();
  nextPoseChangeAt_ = lastFrameAt_ + static_cast<uint32_t>(random(1600, 2400));
  pendingShakeWhileDizzy_ = false;
}

void PetRuntime::tick(const AppModel& model) {
  const PetAnimState resolvedBaseState = resolveBaseState(model);
  if (model.snapshot.petState != PetState::Done) {
    celebrateConsumed_ = false;
  }

  if (transientActive_) {
    if (resolvedBaseState != fallbackState_) {
      fallbackState_ = resolvedBaseState;
    }
  } else if (resolvedBaseState != activeState_) {
    if (resolvedBaseState == PetAnimState::Celebrate) {
      if (!celebrateConsumed_) {
        celebrateConsumed_ = true;
        celebrateLoopsRemaining_ = 2;
        setState(PetAnimState::Heart, true, PetAnimState::Celebrate);
      }
    } else {
      setState(resolvedBaseState);
    }
  }
  if (!transientActive_) {
    baseState_ = activeState_;
  }

  if (pendingShakeWhileDizzy_ && activeState_ == PetAnimState::Dizzy) {
    onShake();
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
  const PetAnimState stateBeforeAdvance = activeState_;
  const bool loopCompleted = advanceFrame(clip);
  if (loopCompleted) {
    if (pendingVariant_ != PetAnimVariant::Base && activeVariant_ == PetAnimVariant::Base) {
      applyPendingVariant();
    }
    if (variantLoopsRemaining_ > 0) {
      --variantLoopsRemaining_;
      if (variantLoopsRemaining_ == 0) {
        clearVariant();
      }
    }
    if (stateBeforeAdvance == PetAnimState::Celebrate && !transientActive_ && celebrateLoopsRemaining_ > 0) {
      --celebrateLoopsRemaining_;
      if (celebrateLoopsRemaining_ == 0) {
        setState(PetAnimState::Idle);
      }
    }
  }
}

void PetRuntime::draw(int16_t x, int16_t y, uint16_t scale, PetState) const {
  auto& canvas = frameBuffer();
  const uint8_t frameIndex = currentFrameIndex();
  const uint8_t* frame = static_cast<const uint8_t*>(pgm_read_ptr(&BUDDY_PACK_FRAMES[frameIndex]));
  const int16_t drawX = x + offsetX_ - static_cast<int16_t>(2 * scale)-4;
  const int16_t drawY = y + offsetY_;

  for (uint16_t row = 0; row < BUDDY_PACK_FRAME_HEIGHT; ++row) {
    for (uint16_t col = 0; col < BUDDY_PACK_FRAME_WIDTH; ++col) {
      const uint16_t pixelIndex = row * BUDDY_PACK_FRAME_WIDTH + col;
      const uint8_t paletteIndex = buddy_pack_palette_index(frame, pixelIndex);
      if (paletteIndex == BUDDY_PACK_TRANSPARENT_INDEX) {
        continue;
      }

      const uint16_t color = pgm_read_word(&PALETTE_RGB565[paletteIndex]);
      canvas.fillRect(drawX + static_cast<int16_t>(col * scale), drawY + static_cast<int16_t>(row * scale), scale,
                      scale, color);
    }
  }
}

PetAnimState PetRuntime::resolveBaseState(const AppModel& model) const {
  if (model.petStats.heartUntilMs > millis()) {
    return PetAnimState::Heart;
  }
  if (model.motionDizzyUntilMs > millis()) {
    return PetAnimState::Dizzy;
  }
  if (model.systemState == SystemState::ScreenSleep || model.snapshot.petState == PetState::Sleep) {
    return PetAnimState::Sleep;
  }
  if (model.faceDownSleepActive || model.petStats.energy <= 18) {
    return PetAnimState::Sleep;
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
  const PetAnimState previousState = activeState_;
  const PetAnimClip& previousClip = clipFor(previousState);
  const PetAnimClip& nextClip = clipFor(state);
  const bool preservePhase = !transient && !transientActive_ && state != PetAnimState::Celebrate &&
                             state != PetAnimState::Dizzy && state != PetAnimState::Heart;

  activeState_ = state;
  baseState_ = transient ? fallback : state;
  fallbackState_ = fallback;
  transientActive_ = transient;

  if (preservePhase && previousClip.count > 1 && nextClip.count > 1) {
    const uint8_t previousFrame = lastBaseFrame_;
    const uint8_t previousMaxFrame = static_cast<uint8_t>(previousClip.count - 1U);
    const uint8_t nextMaxFrame = static_cast<uint8_t>(nextClip.count - 1U);
    animationFrame_ = previousMaxFrame == 0
                          ? 0
                          : static_cast<uint8_t>((static_cast<uint16_t>(previousFrame) * nextMaxFrame) /
                                                 previousMaxFrame);
  } else {
    animationFrame_ = 0;
    lastBaseFrame_ = 0;
    animationDirection_ = 1;
  }

  if (!preservePhase) {
    animationDirection_ = 1;
  }
  lastFrameAt_ = millis();
  holdUntilAt_ = 0;
  resetProceduralState(preservePhase);
}

bool PetRuntime::advanceFrame(const PetAnimClip& clip) {
  if (clip.count <= 1) {
    return false;
  }

  bool loopCompleted = false;
  holdUntilAt_ = 0;

  if (activeVariant_ == PetAnimVariant::Base) {
    const uint32_t now = millis();
    if (now < nextPoseChangeAt_) {
      return false;
    }

    const uint8_t previousBaseFrame = lastBaseFrame_;
    uint8_t nextBaseFrame = previousBaseFrame;
    while (nextBaseFrame == previousBaseFrame) {
      nextBaseFrame = static_cast<uint8_t>(random(clip.count));
    }
    lastBaseFrame_ = nextBaseFrame;

    switch (activeState_) {
      case PetAnimState::Sleep:
        nextPoseChangeAt_ = now + static_cast<uint32_t>(random(4200, 6800));
        break;
      case PetAnimState::Idle:
        nextPoseChangeAt_ = now + static_cast<uint32_t>(random(3200, 5200));
        break;
      case PetAnimState::Busy:
        nextPoseChangeAt_ = now + static_cast<uint32_t>(random(2400, 4200));
        break;
      case PetAnimState::Attention:
        nextPoseChangeAt_ = now + static_cast<uint32_t>(random(3600, 5600));
        break;
      case PetAnimState::Celebrate:
        nextPoseChangeAt_ = now + static_cast<uint32_t>(random(1400, 2200));
        break;
      case PetAnimState::Dizzy:
        nextPoseChangeAt_ = now + static_cast<uint32_t>(random(1600, 2400));
        break;
      case PetAnimState::Heart:
        nextPoseChangeAt_ = now + static_cast<uint32_t>(random(1200, 1800));
        break;
    }

    switch (clip.loopMode) {
      case PetLoopMode::Loop:
      case PetLoopMode::PingPong:
        animationFrame_ = static_cast<uint8_t>((animationFrame_ + 1U) % clip.count);
        loopCompleted = animationFrame_ == 0U;
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

    if (holdUntilAt_ == 0 && clip.randomIdlePause && random(100) < 42) {
      holdUntilAt_ = millis() + static_cast<uint32_t>(random(180, 420));
    }
  } else {
    switch (clip.loopMode) {
      case PetLoopMode::Loop:
        animationFrame_ = static_cast<uint8_t>((animationFrame_ + 1U) % clip.count);
        loopCompleted = animationFrame_ == 0U;
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
          holdUntilAt_ = clip.randomIdlePause && random(100) < 40 ? millis() + static_cast<uint32_t>(random(60, 150)) : 0;
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
  }

  const PetMotionProfile& motion = motionProfileFor(activeState_);
  if (holdUntilAt_ == 0 && loopCompleted && motion.pauseChancePct > 0 && random(100) < motion.pauseChancePct) {
    holdUntilAt_ = millis() + static_cast<uint32_t>(random(45, 110));
  }

  return loopCompleted;
}

void PetRuntime::resetProceduralState(bool preserveOffset) {
  activeVariant_ = PetAnimVariant::Base;
  pendingVariant_ = PetAnimVariant::Base;
  pendingVariantLoopsRemaining_ = 0;
  variantLoopsRemaining_ = 0;
  const uint32_t now = millis();
  if (!preserveOffset) {
    offsetX_ = 0;
    offsetY_ = 0;
    offsetTargetX_ = 0;
    offsetTargetY_ = 0;
    lastVariantOffsetX_ = 0;
    lastVariantOffsetY_ = 0;
  } else {
    offsetTargetX_ = offsetX_;
    offsetTargetY_ = offsetY_;
  }
  lastOffsetStepAt_ = now;
  nextMicroMotionAt_ = now;
  nextIdleActionAt_ = now + static_cast<uint32_t>(random(2600, 5200));
  nextPoseChangeAt_ = now + static_cast<uint32_t>(random(2200, 4200));
}

void PetRuntime::updateProceduralMotion(const AppModel& model, uint32_t now) {
  updateOffsets(model, now);
  maybeTriggerVariant(model, now);
}

void PetRuntime::maybeTriggerVariant(const AppModel& model, uint32_t now) {
  if (transientActive_ || activeVariant_ != PetAnimVariant::Base || pendingVariant_ != PetAnimVariant::Base ||
      now < nextIdleActionAt_) {
    return;
  }

  const PetMotionProfile& motion = motionProfileFor(activeState_);
  nextIdleActionAt_ = now + static_cast<uint32_t>(random(3200, 6800));
  if (random(100) >= motion.variantChancePct) {
    return;
  }

  PetAnimVariant nextVariant = PetAnimVariant::Base;
  uint8_t nextLoops = 0;
  switch (activeState_) {
    case PetAnimState::Idle:
      if (random(100) < motion.rareActionChancePct) {
        nextVariant = lastVariant_ == PetAnimVariant::Blink ? PetAnimVariant::Tilt : PetAnimVariant::Blink;
      } else {
        nextVariant = model.petStats.mood >= 65 ? PetAnimVariant::Tilt : PetAnimVariant::Blink;
      }
      nextLoops = 1;
      break;
    case PetAnimState::Busy:
      nextVariant = PetAnimVariant::Focus;
      nextLoops = 1;
      break;
    case PetAnimState::Sleep:
      if (random(100) < motion.rareActionChancePct) {
        nextVariant = PetAnimVariant::Blink;
        nextLoops = 1;
      }
      break;
    case PetAnimState::Attention:
      nextVariant = PetAnimVariant::Tilt;
      nextLoops = 1;
      break;
    case PetAnimState::Celebrate:
    case PetAnimState::Dizzy:
    case PetAnimState::Heart:
      break;
  }

  pendingVariant_ = nextVariant;
  pendingVariantLoopsRemaining_ = nextLoops;
}

void PetRuntime::updateOffsets(const AppModel& model, uint32_t now) {
  if (now >= nextMicroMotionAt_) {
    PetMotionProfile motion = motionProfileFor(activeState_);
    if (model.petStats.energy <= 30) {
      motion.maxOffsetY = static_cast<int8_t>(motion.maxOffsetY > 1 ? motion.maxOffsetY - 1 : motion.maxOffsetY);
    }
    if (activeState_ == PetAnimState::Busy && model.petStats.focus >= 75) {
      motion.minOffsetX = 0;
      motion.maxOffsetX = 0;
      motion.maxOffsetY = static_cast<int8_t>(motion.maxOffsetY > 1 ? motion.maxOffsetY - 1 : motion.maxOffsetY);
    }
    if (activeState_ == PetAnimState::Idle && model.petStats.mood >= 75 && model.petStats.energy >= 60) {
      motion.maxOffsetY = static_cast<int8_t>(motion.maxOffsetY < 4 ? motion.maxOffsetY + 1 : motion.maxOffsetY);
    }

    switch (activeVariant_) {
      case PetAnimVariant::Base:
        offsetTargetX_ = randomRange(motion.minOffsetX, motion.maxOffsetX);
        offsetTargetY_ = randomRange(motion.minOffsetY, motion.maxOffsetY);
        break;
      case PetAnimVariant::Blink:
        offsetTargetX_ = 0;
        offsetTargetY_ = randomRange(motion.minOffsetY, motion.maxOffsetY);
        if (offsetTargetX_ == lastVariantOffsetX_ && offsetTargetY_ == lastVariantOffsetY_) {
          offsetTargetY_ = offsetTargetY_ == 0 ? 1 : 0;
        }
        lastVariantOffsetX_ = offsetTargetX_;
        lastVariantOffsetY_ = offsetTargetY_;
        break;
      case PetAnimVariant::Tilt:
        offsetTargetX_ = random(2) == 0 ? motion.minOffsetX : motion.maxOffsetX;
        offsetTargetY_ = randomRange(motion.minOffsetY, motion.maxOffsetY);
        if (offsetTargetX_ == lastVariantOffsetX_ && offsetTargetY_ == lastVariantOffsetY_) {
          offsetTargetX_ = offsetTargetX_ <= 0 ? 1 : -1;
        }
        lastVariantOffsetX_ = offsetTargetX_;
        lastVariantOffsetY_ = offsetTargetY_;
        break;
      case PetAnimVariant::Focus:
        offsetTargetX_ = 0;
        offsetTargetY_ = randomRange(0, motion.maxOffsetY > 1 ? 1 : motion.maxOffsetY);
        if (offsetTargetX_ == lastVariantOffsetX_ && offsetTargetY_ == lastVariantOffsetY_) {
          offsetTargetY_ = offsetTargetY_ == 0 ? 1 : 0;
        }
        lastVariantOffsetX_ = offsetTargetX_;
        lastVariantOffsetY_ = offsetTargetY_;
        break;
    }

    if (offsetTargetX_ < -1) {
      offsetTargetX_ = -1;
    } else if (offsetTargetX_ > 1) {
      offsetTargetX_ = 1;
    }
    if (offsetTargetY_ < 0) {
      offsetTargetY_ = 0;
    } else if (offsetTargetY_ > 1) {
      offsetTargetY_ = 1;
    }

    nextMicroMotionAt_ =
        now + static_cast<uint32_t>(random(motion.minMotionIntervalMs, motion.maxMotionIntervalMs + 1));
  }

  if (now - lastOffsetStepAt_ < 90) {
    return;
  }
  lastOffsetStepAt_ = now;

  if (offsetX_ < offsetTargetX_) {
    ++offsetX_;
  } else if (offsetX_ > offsetTargetX_) {
    --offsetX_;
  }
  if (offsetY_ < offsetTargetY_) {
    ++offsetY_;
  } else if (offsetY_ > offsetTargetY_) {
    --offsetY_;
  }
}

void PetRuntime::applyPendingVariant() {
  if (pendingVariant_ == PetAnimVariant::Base || pendingVariantLoopsRemaining_ == 0) {
    return;
  }
  activeVariant_ = pendingVariant_;
  variantLoopsRemaining_ = pendingVariantLoopsRemaining_;
  lastVariant_ = activeVariant_;
  pendingVariant_ = PetAnimVariant::Base;
  pendingVariantLoopsRemaining_ = 0;
}

void PetRuntime::clearVariant() {
  activeVariant_ = PetAnimVariant::Base;
  pendingVariant_ = PetAnimVariant::Base;
  pendingVariantLoopsRemaining_ = 0;
  variantLoopsRemaining_ = 0;
}

uint8_t PetRuntime::currentFrameIndex() const {
  const PetAnimClip& clip = clipFor(activeState_);

  switch (activeState_) {
    case PetAnimState::Sleep:
      if (activeVariant_ == PetAnimVariant::Blink) {
        constexpr uint8_t kSleepBlinkFrames[] = {1, 3, 2};
        return static_cast<uint8_t>(clip.start + kSleepBlinkFrames[animationFrame_ % 3U]);
      }
      break;
    case PetAnimState::Idle:
      if (activeVariant_ == PetAnimVariant::Blink) {
        constexpr uint8_t kIdleBlinkFrames[] = {0, 4, 7};
        return static_cast<uint8_t>(clip.start + kIdleBlinkFrames[animationFrame_ % 3U]);
      }
      if (activeVariant_ == PetAnimVariant::Tilt) {
        constexpr uint8_t kIdleTiltFrames[] = {1, 3, 5, 6};
        return static_cast<uint8_t>(clip.start + kIdleTiltFrames[animationFrame_ % 4U]);
      }
      break;
    case PetAnimState::Busy:
      if (activeVariant_ == PetAnimVariant::Focus) {
        constexpr uint8_t kBusyFocusFrames[] = {0, 2, 4, 6};
        return static_cast<uint8_t>(clip.start + kBusyFocusFrames[animationFrame_ % 4U]);
      }
      break;
    case PetAnimState::Attention:
      if (activeVariant_ == PetAnimVariant::Tilt) {
        constexpr uint8_t kAttentionTiltFrames[] = {1, 3, 4, 6};
        return static_cast<uint8_t>(clip.start + kAttentionTiltFrames[animationFrame_ % 4U]);
      }
      break;
    case PetAnimState::Celebrate:
    case PetAnimState::Dizzy:
    case PetAnimState::Heart:
      break;
  }

  return static_cast<uint8_t>(clip.start + lastBaseFrame_);
}

}  // namespace buddy
