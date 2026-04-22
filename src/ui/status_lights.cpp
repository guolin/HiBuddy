#include "ui/status_lights.h"

#include "board/board_init.h"
namespace buddy {

void StatusLights::draw(const Snapshot& snapshot, int16_t x, int16_t y, uint16_t size, uint16_t gap) const {
  auto& canvas = frameBuffer();
  for (uint8_t i = 0; i < kSessionLightCount; ++i) {
    uint16_t color = 0x4208;
    if (i < snapshot.sessionCount) {
      color = colorFor(snapshot.sessionStates[i]);
    }
    canvas.fillRoundRect(x + i * (size + gap), y, size, size, 3, color);
  }
}

uint16_t StatusLights::colorFor(SessionState state) const {
  switch (state) {
    case SessionState::Thinking:
      return 0x2D7F;
    case SessionState::Busy:
      return 0xFD20;
    case SessionState::Waiting:
      return 0xFC60;
    case SessionState::Error:
      return 0xF800;
    case SessionState::Done:
      return 0x07E0;
  }
  return TFT_WHITE;
}

}  // namespace buddy
