#pragma once

#include "app_types.h"

namespace buddy {

class SnapshotRuntime {
 public:
  void begin(const Snapshot& snapshot);
  void onSnapshot(AppModel& model);
  void tick(AppModel& model);

 private:
  PetState lastPetState_{PetState::Idle};
  bool staleLatched_{false};

  ReminderType reminderFor(PetState state) const;
  void activateReminder(AppModel& model, ReminderType reminder) const;
};

}  // namespace buddy
