#include "storage/settings_store.h"

#include <Preferences.h>

namespace buddy {

namespace {
constexpr const char* kNamespace = "buddy";

uint8_t normalizeBrightness(uint8_t value) {
  if (value <= 96) {
    return 64;
  }
  if (value <= 160) {
    return 128;
  }
  if (value <= 223) {
    return 192;
  }
  return 255;
}

uint8_t normalizeVolume(uint8_t value) {
  if (value <= 37) {
    return 25;
  }
  if (value <= 62) {
    return 50;
  }
  if (value <= 87) {
    return 75;
  }
  return 100;
}
}

bool SettingsStore::begin() {
  Preferences prefs;
  const bool ok = prefs.begin(kNamespace, false);
  prefs.end();
  return ok;
}

DeviceSettings SettingsStore::load() {
  Preferences prefs;
  DeviceSettings settings;
  if (!prefs.begin(kNamespace, true)) {
    return settings;
  }

  settings.deviceId = prefs.getString("device_id", kDefaultDeviceId);
  settings.deviceToken = prefs.getString("device_tok", "");
  settings.wifiSsid = prefs.getString("wifi_ssid", "");
  settings.wifiPassword = prefs.getString("wifi_pass", "");
  settings.brightness = normalizeBrightness(prefs.getUChar("bright", settings.brightness));
  settings.volume = normalizeVolume(prefs.getUChar("volume", settings.volume));
  settings.demoMode = prefs.getBool("demo_mode", settings.demoMode);
  settings.preferredBaseScreen =
      static_cast<BaseScreen>(prefs.getUChar("home_view", static_cast<uint8_t>(BaseScreen::Overview)));
  prefs.end();
  return settings;
}

bool SettingsStore::save(const DeviceSettings& settings) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }

  prefs.putString("device_id", settings.deviceId);
  prefs.putString("device_tok", settings.deviceToken);
  prefs.putString("wifi_ssid", settings.wifiSsid);
  prefs.putString("wifi_pass", settings.wifiPassword);
  prefs.putUChar("bright", normalizeBrightness(settings.brightness));
  prefs.putUChar("volume", normalizeVolume(settings.volume));
  prefs.putBool("demo_mode", settings.demoMode);
  prefs.putUChar("home_view", static_cast<uint8_t>(settings.preferredBaseScreen));
  prefs.end();
  return true;
}

void SettingsStore::clearWifi() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return;
  }
  prefs.remove("wifi_ssid");
  prefs.remove("wifi_pass");
  prefs.end();
}

}  // namespace buddy
