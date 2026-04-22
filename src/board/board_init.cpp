#include "board_init.h"

#include <M5Unified.h>

namespace buddy {

namespace {
LGFX_Sprite gFrameBuffer(&M5.Display);
}

bool initializeBoard() {
  Serial.begin(115200);
  const uint32_t serialWaitStartedAt = millis();
  while (!Serial && millis() - serialWaitStartedAt < 1500) {
    delay(10);
  }
  delay(50);
  auto cfg = M5.config();
  cfg.clear_display = true;
  cfg.internal_imu = true;
  cfg.output_power = true;
  M5.begin(cfg);
  M5.Display.setRotation(0);
  M5.Display.setTextSize(1);
  gFrameBuffer.setColorDepth(16);
  gFrameBuffer.createSprite(M5.Display.width(), M5.Display.height());
  gFrameBuffer.setTextSize(1);
  gFrameBuffer.setTextDatum(top_left);
  Serial.printf("[buddy] board init complete board=%d pmic=%d bat=%ld bat_mv=%d vbus_mv=%d charge=%d\n",
                static_cast<int>(M5.getBoard()), static_cast<int>(M5.Power.getType()),
                static_cast<long>(M5.Power.getBatteryLevel()), static_cast<int>(M5.Power.getBatteryVoltage()),
                static_cast<int>(M5.Power.getVBUSVoltage()), static_cast<int>(M5.Power.isCharging()));
  return true;
}

void applyBacklight(uint8_t brightness) {
  M5.Display.setBrightness(brightness);
}

LGFX_Sprite& frameBuffer() {
  return gFrameBuffer;
}

void beginFrame() {
  gFrameBuffer.clear(TFT_BLACK);
}

void presentFrame() {
  gFrameBuffer.pushSprite(0, 0);
}

}  // namespace buddy
