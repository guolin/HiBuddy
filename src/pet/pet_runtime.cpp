#include "pet/pet_runtime.h"

#include "board/board_init.h"

namespace buddy {

void PetRuntime::begin() {
  activeState_ = PetAnimState::Idle;
  fallbackState_ = PetAnimState::Idle;
  animationFrame_ = 0;
  animationDirection_ = 1;
  transientActive_ = false;
  lastFrameAt_ = millis();
  holdUntilAt_ = 0;
}

void PetRuntime::tick(const AppModel& model) {
  const PetAnimState baseState = resolveBaseState(model);
  if (transientActive_) {
    if (baseState != fallbackState_) {
      fallbackState_ = baseState;
    }
  } else if (baseState != activeState_) {
    if (baseState == PetAnimState::Celebrate) {
      setState(PetAnimState::Heart, true, PetAnimState::Celebrate);
    } else {
      setState(baseState);
    }
  }

  const PetAnimClip& clip = clipFor(activeState_);
  const uint32_t now = millis();
  if (holdUntilAt_ != 0 && now < holdUntilAt_) {
    return;
  }
  if (now - lastFrameAt_ < clip.frameIntervalMs) {
    return;
  }

  lastFrameAt_ = now;
  advanceFrame(clip);
}

void PetRuntime::draw(int16_t x, int16_t y, uint16_t scale, PetState) const {
  const DemoFrame& frame = kDemoFrames[currentFrameIndex()];
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

PetAnimState PetRuntime::resolveBaseState(const AppModel& model) const {
  if (model.systemState == SystemState::ScreenSleep || model.snapshot.petState == PetState::Sleep) {
    return PetAnimState::Sleep;
  }
  if (model.motionDizzyUntilMs > millis()) {
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
      return PetAnimState::Busy;
    case PetState::Done:
      return PetAnimState::Celebrate;
    case PetState::Error:
    case PetState::Offline:
      return PetAnimState::Dizzy;
    case PetState::Sleep:
      return PetAnimState::Sleep;
    case PetState::Waiting:
      return PetAnimState::Attention;
  }
  return PetAnimState::Idle;
}

void PetRuntime::setState(PetAnimState state, bool transient, PetAnimState fallback) {
  activeState_ = state;
  fallbackState_ = fallback;
  transientActive_ = transient;
  animationFrame_ = 0;
  animationDirection_ = 1;
  lastFrameAt_ = millis();
  holdUntilAt_ = 0;
}

void PetRuntime::advanceFrame(const PetAnimClip& clip) {
  if (clip.count <= 1) {
    return;
  }

  switch (clip.loopMode) {
    case PetLoopMode::Loop:
      animationFrame_ = static_cast<uint8_t>((animationFrame_ + 1U) % clip.count);
      if (animationFrame_ == clip.count - 1U && clip.holdOnLastFrameMs != 0) {
        holdUntilAt_ = millis() + clip.holdOnLastFrameMs;
      } else if (animationFrame_ == 0U && clip.randomIdlePause) {
        holdUntilAt_ = millis() + static_cast<uint32_t>(random(80, 260));
      } else {
        holdUntilAt_ = 0;
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
          holdUntilAt_ = 0;
        }
      } else if (animationFrame_ == 0U) {
        animationDirection_ = 1;
        animationFrame_ = 1U;
        holdUntilAt_ = clip.randomIdlePause ? millis() + static_cast<uint32_t>(random(80, 220)) : 0;
      } else {
        --animationFrame_;
        holdUntilAt_ = 0;
      }
      break;

    case PetLoopMode::PlayOnce:
      if (animationFrame_ + 1U < clip.count) {
        ++animationFrame_;
        if (animationFrame_ == clip.count - 1U && clip.holdOnLastFrameMs != 0) {
          holdUntilAt_ = millis() + clip.holdOnLastFrameMs;
        } else {
          holdUntilAt_ = 0;
        }
      } else if (transientActive_) {
        setState(fallbackState_);
      } else {
        holdUntilAt_ = clip.holdOnLastFrameMs != 0 ? millis() + clip.holdOnLastFrameMs : 0;
      }
      break;
  }
}

uint8_t PetRuntime::currentFrameIndex() const {
  const PetAnimClip& clip = clipFor(activeState_);
  return static_cast<uint8_t>(clip.start + animationFrame_);
}

}  // namespace buddy
