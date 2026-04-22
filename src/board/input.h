#pragma once

#include "app_types.h"

namespace buddy {

class InputManager {
 public:
  void begin();
  ButtonEvent poll();

 private:
  bool wakeConsumed_{false};
};

}  // namespace buddy
