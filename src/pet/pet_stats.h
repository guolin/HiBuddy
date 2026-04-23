#pragma once

#include "app_types.h"

namespace buddy {

class PetStatsRuntime {
 public:
  void begin(AppModel& model);
  void onSnapshot(AppModel& model);
  void tick(AppModel& model);
  void onFriendlyShake(AppModel& model);
  void onRoughShake(AppModel& model);
  void onSleep(AppModel& model);
  void onWake(AppModel& model);
  void onPairSuccess(AppModel& model);
  void onMqttRestored(AppModel& model);

 private:
  uint32_t lastTickAt_{0};
  bool lastSnapshotOnline_{false};

  static uint8_t clampStat(int32_t value);
  static void addMood(AppModel& model, int8_t delta);
  static void addEnergy(AppModel& model, int8_t delta);
  static void addFocus(AppModel& model, int8_t delta);
  static bool snapshotFresh(const AppModel& model);
};

}  // namespace buddy
