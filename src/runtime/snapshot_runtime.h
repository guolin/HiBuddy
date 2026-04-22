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
};

}  // namespace buddy
