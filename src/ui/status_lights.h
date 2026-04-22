#pragma once

#include "app_types.h"

namespace buddy {

class StatusLights {
 public:
  void draw(const Snapshot& snapshot, int16_t x, int16_t y, uint16_t size, uint16_t gap) const;

 private:
  uint16_t colorFor(SessionState state) const;
};

}  // namespace buddy
