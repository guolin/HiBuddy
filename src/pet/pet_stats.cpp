#include "pet/pet_stats.h"

#include "app_config.h"

namespace buddy {

namespace {
constexpr uint32_t kStatsTickMs = 1000;
constexpr uint32_t kFriendlyShakeCooldownMs = 30000;
constexpr uint32_t kHeartBoostMs = 1800;
constexpr uint8_t kMoodDefault = 60;
constexpr uint8_t kEnergyDefault = 70;
constexpr uint8_t kFocusDefault = 50;
}

void PetStatsRuntime::begin(AppModel& model) {
  model.petStats.mood = kMoodDefault;
  model.petStats.energy = clampStat(model.settings.petEnergy);
  model.petStats.focus = kFocusDefault;
  model.petStats.lastInteractionMs = millis();
  model.petStats.lastRestMs = millis();
  model.petStats.lastFriendlyShakeAtMs = 0;
  model.petStats.heartUntilMs = 0;
  lastTickAt_ = millis();
  lastSnapshotOnline_ = snapshotFresh(model);
}

void PetStatsRuntime::onSnapshot(AppModel& model) {
  model.petStats.lastInteractionMs = millis();

  switch (model.snapshot.petState) {
    case PetState::Thinking:
      addFocus(model, 6);
      addEnergy(model, -3);
      break;
    case PetState::Busy:
      addFocus(model, 8);
      addEnergy(model, -5);
      addMood(model, 1);
      break;
    case PetState::Waiting:
      addFocus(model, -2);
      addMood(model, -1);
      break;
    case PetState::Done:
      addMood(model, 8);
      addFocus(model, -4);
      model.petStats.heartUntilMs = millis() + kHeartBoostMs;
      break;
    case PetState::Error:
    case PetState::Offline:
      addMood(model, -7);
      addFocus(model, -6);
      break;
    case PetState::Sleep:
      addEnergy(model, 8);
      break;
    case PetState::Idle:
    default:
      addMood(model, 1);
      addEnergy(model, 2);
      addFocus(model, -3);
      break;
  }
}

void PetStatsRuntime::tick(AppModel& model) {
  const uint32_t now = millis();
  if (now - lastTickAt_ < kStatsTickMs) {
    return;
  }
  lastTickAt_ = now;

  const bool online = snapshotFresh(model);
  if (online && !lastSnapshotOnline_) {
    onMqttRestored(model);
  } else if (!online && model.pairState == PairUiState::Ready) {
    addMood(model, -2);
    addFocus(model, -2);
  }
  lastSnapshotOnline_ = online;

  if (model.systemState == SystemState::ScreenSleep || model.faceDownSleepActive ||
      model.snapshot.petState == PetState::Sleep) {
    addEnergy(model, 4);
    addMood(model, 1);
    model.petStats.lastRestMs = now;
    return;
  }

  if (model.snapshot.requiresUser || model.snapshot.petState == PetState::Waiting) {
    addEnergy(model, -2);
    addMood(model, -1);
    return;
  }

  switch (model.snapshot.petState) {
    case PetState::Thinking:
      addFocus(model, 2);
      addEnergy(model, -2);
      break;
    case PetState::Busy:
      addFocus(model, 3);
      addEnergy(model, -3);
      break;
    case PetState::Done:
      addMood(model, 1);
      addFocus(model, -2);
      break;
    case PetState::Error:
    case PetState::Offline:
      addMood(model, -2);
      addFocus(model, -2);
      break;
    case PetState::Idle:
    default:
      addEnergy(model, 2);
      addMood(model, 1);
      addFocus(model, -2);
      model.petStats.lastRestMs = now;
      break;
  }
}

void PetStatsRuntime::onFriendlyShake(AppModel& model) {
  const uint32_t now = millis();
  if (now - model.petStats.lastFriendlyShakeAtMs < kFriendlyShakeCooldownMs) {
    return;
  }
  model.petStats.lastFriendlyShakeAtMs = now;
  model.petStats.lastInteractionMs = now;
  addMood(model, 4);
  addFocus(model, -2);
  model.petStats.heartUntilMs = now + kHeartBoostMs;
}

void PetStatsRuntime::onRoughShake(AppModel& model) {
  addMood(model, -3);
  addFocus(model, -4);
}

void PetStatsRuntime::onSleep(AppModel& model) {
  model.petStats.lastRestMs = millis();
  addEnergy(model, 8);
  addMood(model, 2);
}

void PetStatsRuntime::onWake(AppModel& model) {
  model.petStats.lastInteractionMs = millis();
  addMood(model, 2);
  addEnergy(model, 3);
}

void PetStatsRuntime::onPairSuccess(AppModel& model) {
  addMood(model, 8);
  addEnergy(model, 4);
  model.petStats.heartUntilMs = millis() + kHeartBoostMs;
}

void PetStatsRuntime::onMqttRestored(AppModel& model) {
  addMood(model, 5);
  addFocus(model, 4);
}

uint8_t PetStatsRuntime::clampStat(int32_t value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return static_cast<uint8_t>(value);
}

void PetStatsRuntime::addMood(AppModel& model, int8_t delta) {
  model.petStats.mood = clampStat(static_cast<int32_t>(model.petStats.mood) + delta);
}

void PetStatsRuntime::addEnergy(AppModel& model, int8_t delta) {
  model.petStats.energy = clampStat(static_cast<int32_t>(model.petStats.energy) + delta);
}

void PetStatsRuntime::addFocus(AppModel& model, int8_t delta) {
  model.petStats.focus = clampStat(static_cast<int32_t>(model.petStats.focus) + delta);
}

bool PetStatsRuntime::snapshotFresh(const AppModel& model) {
  return model.network.lastSnapshotMs != 0 && millis() - model.network.lastSnapshotMs <= kStaleSnapshotMs &&
         model.network.mqttConnected;
}

}  // namespace buddy
