#include "board/input.h"

#include <M5Unified.h>

namespace buddy {

void InputManager::begin() {
  wakeConsumed_ = false;
}

ButtonEvent InputManager::poll() {
  M5.update();

  if (M5.BtnA.wasHold()) {
    ButtonEvent event;
    event.button = ButtonId::A;
    event.gesture = ButtonGesture::LongPress;
    return event;
  }
  if (M5.BtnB.wasHold()) {
    ButtonEvent event;
    event.button = ButtonId::B;
    event.gesture = ButtonGesture::LongPress;
    return event;
  }
  if (M5.BtnA.wasClicked()) {
    ButtonEvent event;
    event.button = ButtonId::A;
    event.gesture = ButtonGesture::ShortPress;
    return event;
  }
  if (M5.BtnB.wasClicked()) {
    ButtonEvent event;
    event.button = ButtonId::B;
    event.gesture = ButtonGesture::ShortPress;
    return event;
  }

  return {};
}

}  // namespace buddy
