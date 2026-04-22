#include "runtime/snapshot_runtime.h"

#include "app_config.h"

namespace buddy {

void SnapshotRuntime::begin(const Snapshot& snapshot) {
  lastPetState_ = snapshot.petState;
  staleLatched_ = false;
}

void SnapshotRuntime::onSnapshot(AppModel& model) {
  staleLatched_ = false;
  const ReminderType nextReminder = reminderFor(model.snapshot.petState);
  if (model.snapshot.petState != lastPetState_ && nextReminder != ReminderType::None) {
    activateReminder(model, nextReminder);
  }

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
  activateReminder(model, ReminderType::Stale);
}

ReminderType SnapshotRuntime::reminderFor(PetState state) const {
  switch (state) {
    case PetState::Waiting:
      return ReminderType::Waiting;
    case PetState::Error:
      return ReminderType::Error;
    case PetState::Done:
      return ReminderType::Done;
    default:
      return ReminderType::None;
  }
}

void SnapshotRuntime::activateReminder(AppModel& model, ReminderType reminder) const {
  model.reminder.active = reminder;
  model.reminder.startedAtMs = millis();
}

}  // namespace buddy
