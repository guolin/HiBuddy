#include "runtime/snapshot_runtime.h"

#include "app_config.h"

namespace buddy {

void SnapshotRuntime::begin(const Snapshot& snapshot) {
  lastPetState_ = snapshot.petState;
  staleLatched_ = false;
}

void SnapshotRuntime::onSnapshot(AppModel& model) {
  staleLatched_ = false;
  model.reminder.active = ReminderType::None;
  lastPetState_ = model.snapshot.petState;
}

void SnapshotRuntime::tick(AppModel& model) {
  if (model.network.lastSnapshotMs == 0) {
    return;
  }

  if (millis() - model.network.lastSnapshotMs <= kStaleSnapshotMs) {
    return;
  }

  if (staleLatched_) {
    return;
  }

  staleLatched_ = true;
  model.snapshot.petState = PetState::Offline;
  model.reminder.active = ReminderType::None;
}

}  // namespace buddy
