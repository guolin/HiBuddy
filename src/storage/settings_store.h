#pragma once

#include "app_types.h"

namespace buddy {

class SettingsStore {
 public:
  bool begin();
  DeviceSettings load();
  bool save(const DeviceSettings& settings);
  void clearWifi();
};

}  // namespace buddy
