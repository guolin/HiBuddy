#include "ui/router.h"

#include <M5Unified.h>
#include "app_config.h"
#include "board/board_init.h"

namespace buddy {

namespace {

enum class MenuAction : uint8_t {
  Brightness,
  Volume,
  Wifi,
  Pair,
  Demo,
  Back,
};

struct MenuEntry {
  MenuAction action;
  const char* label;
};

constexpr MenuEntry kRestrictedWifiMenu[] = {
    {MenuAction::Wifi, "Set up Wi-Fi"},
    {MenuAction::Brightness, "Brightness"},
    {MenuAction::Demo, "Demo"},
    {MenuAction::Back, "Back"},
};

constexpr MenuEntry kRestrictedPairMenu[] = {
    {MenuAction::Pair, "Pair Device"},
    {MenuAction::Wifi, "Wi-Fi"},
    {MenuAction::Brightness, "Brightness"},
    {MenuAction::Demo, "Demo"},
    {MenuAction::Back, "Back"},
};

constexpr MenuEntry kFullMenu[] = {
    {MenuAction::Brightness, "Brightness"},
    {MenuAction::Volume, "Volume"},
    {MenuAction::Wifi, "Wi-Fi"},
    {MenuAction::Pair, "Re-pair"},
    {MenuAction::Demo, "Demo"},
    {MenuAction::Back, "Back"},
};

constexpr uint8_t kBrightnessSteps[] = {64, 128, 192, 255};
constexpr uint8_t kVolumeSteps[] = {25, 50, 75, 100};

uint8_t stepIndex(const uint8_t value, const uint8_t* steps, const size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (value <= steps[i]) {
      return static_cast<uint8_t>(i);
    }
  }
  return static_cast<uint8_t>(count - 1U);
}

String fitText(const String& text, uint8_t maxChars) {
  if (text.length() <= maxChars) {
    return text;
  }
  if (maxChars <= 3) {
    return text.substring(0, maxChars);
  }
  return text.substring(0, maxChars - 3) + "...";
}

String statLabel(const uint8_t value) {
  if (value >= 76) {
    return "High";
  }
  if (value >= 41) {
    return "Mid";
  }
  return "Low";
}

String stateText(const PetState state) {
  return String(reinterpret_cast<const __FlashStringHelper*>(toFlashString(state)));
}

String overviewHeadline(const AppModel& model) {
  if (model.demoMode) {
    return "Pet summary";
  }
  if (model.wifiState != WifiUiState::Ready) {
    return "Need Wi-Fi";
  }
  if (model.pairState != PairUiState::Ready) {
    return "Need pairing";
  }
  if (model.runtimeState == RuntimeUiState::Degraded) {
    return model.network.mqttConnected ? "Snapshot stale" : "Offline";
  }
  return "Pet summary";
}

String petHeadline(const AppModel& model) {
  if (model.motionDizzyUntilMs > millis()) {
    return "Dizzy";
  }
  if (model.snapshot.petState == PetState::Idle) {
    return "";
  }
  if (!model.demoMode && model.wifiState != WifiUiState::Ready) {
    return "Offline";
  }
  if (!model.demoMode && model.pairState != PairUiState::Ready) {
    return "Unpaired";
  }
  return stateText(model.snapshot.petState);
}

String overviewFocusText(const AppModel& model) {
  if (model.demoMode) {
    return fitText(model.snapshot.focusTitle, 24);
  }
  if (model.wifiState != WifiUiState::Ready) {
    return fitText("Need Wi-Fi to start Buddy", 24);
  }
  if (model.pairState != PairUiState::Ready) {
    return fitText("Pair device to start Buddy", 24);
  }
  return fitText(model.snapshot.focusTitle, 24);
}

bool isSnapshotFresh(const AppModel& model) {
  return model.network.lastSnapshotMs != 0 && millis() - model.network.lastSnapshotMs <= kStaleSnapshotMs;
}

String overviewOnlineLabel(const AppModel& model) {
  if (model.demoMode) {
    return "Online";
  }
  const bool online = model.wifiState == WifiUiState::Ready && model.pairState == PairUiState::Ready &&
                      model.network.mqttConnected && isSnapshotFresh(model);
  return online ? "Online" : "Offline";
}

String requiresUserLine(const AppModel& model) {
  if (model.demoMode) {
    return model.snapshot.requiresUser ? "Needs your attention" : "Demo mode active";
  }
  if (model.wifiState != WifiUiState::Ready) {
    return "Connect Wi-Fi first";
  }
  if (model.pairState != PairUiState::Ready) {
    return "Waiting for pairing";
  }
  if (model.snapshot.requiresUser) {
    return "Needs your attention";
  }
  return "No action needed";
}

String snapshotAgeLabel(uint32_t lastSnapshotMs) {
  if (lastSnapshotMs == 0) {
    return "none";
  }
  return String((millis() - lastSnapshotMs) / 1000U) + "s ago";
}

String batteryLevelLabel() {
  int32_t level = M5.Power.getBatteryLevel();
  if (level <= 0) {
    const int16_t batteryMv = M5.Power.getBatteryVoltage();
    if (batteryMv > 3300) {
      level = (batteryMv - 3300) * 100 / (4150 - 3300);
    }
  }
  if (level < 0) {
    return "--";
  }
  level = max<int32_t>(0, min<int32_t>(100, level));
  return String(level) + "%";
}

String chargingLabel() {
  const auto chargingState = M5.Power.isCharging();
  switch (chargingState) {
    case m5::Power_Class::is_charging_t::is_charging:
      return "Charging";
    case m5::Power_Class::is_charging_t::is_discharging:
      return "Battery";
    case m5::Power_Class::is_charging_t::charge_unknown:
    default:
      return M5.Power.getVBUSVoltage() > 4300 ? "Charging" : "Battery";
  }
}

uint16_t batteryAccentColor() {
  const auto chargingState = M5.Power.isCharging();
  if (chargingState == m5::Power_Class::is_charging_t::is_charging) {
    return 0x07E0;
  }
  if (chargingState == m5::Power_Class::is_charging_t::charge_unknown && M5.Power.getVBUSVoltage() > 4300) {
    return 0x07E0;
  }
  return TFT_WHITE;
}

uint16_t wifiAccentColor(const AppModel& model) {
  return model.network.wifiConnected ? 0x07E0 : 0xF800;
}

bool isGuideVisible(const AppModel& model) {
  return model.guide != GuideScreen::None && !model.userDismissedGuide;
}

String menuLabelFor(const AppModel& model, MenuAction action, const char* fallback) {
  if (action == MenuAction::Demo) {
    return model.demoMode ? "Demo Off" : "Demo On";
  }
  return fallback;
}

const MenuEntry* activeMenuEntries(const AppModel& model, size_t& count) {
  if (model.wifiState != WifiUiState::Ready) {
    count = sizeof(kRestrictedWifiMenu) / sizeof(kRestrictedWifiMenu[0]);
    return kRestrictedWifiMenu;
  }
  if (model.pairState != PairUiState::Ready) {
    count = sizeof(kRestrictedPairMenu) / sizeof(kRestrictedPairMenu[0]);
    return kRestrictedPairMenu;
  }
  count = sizeof(kFullMenu) / sizeof(kFullMenu[0]);
  return kFullMenu;
}

void clampMenuIndex(AppModel& model, size_t count) {
  if (count == 0) {
    model.menuIndex = 0;
    return;
  }
  model.menuIndex %= static_cast<uint8_t>(count);
}

void syncMenuIndex(AppModel& model) {
  size_t count = 0;
  activeMenuEntries(model, count);
  clampMenuIndex(model, count);
}

String levelLabel(const uint8_t index) {
  return "Level " + String(index + 1U);
}

void drawLabelValue(const String& label, const String& value, int16_t y, uint8_t font = 1) {
  auto& canvas = frameBuffer();
  canvas.setTextColor(0xBDF7, TFT_BLACK);
  canvas.drawString(label, 8, y, font);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawRightString(value, canvas.width() - 8, y, font);
}

void drawCenteredCopy(const String& text, int16_t y, uint16_t color = TFT_WHITE, uint8_t font = 2) {
  auto& canvas = frameBuffer();
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawCentreString(text, canvas.width() / 2, y, font);
}

void drawInfoSection(const String& title, const String& value, int16_t y, uint16_t accent, uint8_t valueFont = 4) {
  auto& canvas = frameBuffer();
  canvas.setTextColor(0x8C71, TFT_BLACK);
  canvas.drawCentreString(title, canvas.width() / 2, y, 2);
  canvas.setTextColor(accent, TFT_BLACK);
  canvas.drawCentreString(value, canvas.width() / 2, y + 22, valueFont);
}

void drawStatBar(const String& label, uint8_t value, int16_t y, uint16_t accent) {
  auto& canvas = frameBuffer();
  constexpr int16_t kBarX = 16;
  constexpr int16_t kBarW = 103;
  constexpr int16_t kBarH = 14;
  canvas.setTextColor(0xBDF7, TFT_BLACK);
  canvas.drawString(label, kBarX, y, 2);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawRightString(statLabel(value), canvas.width() - 16, y, 2);
  canvas.drawRoundRect(kBarX, y + 20, kBarW, kBarH, 6, 0x5AEB);
  canvas.fillRoundRect(kBarX + 2, y + 22, ((kBarW - 4) * value) / 100, kBarH - 4, 5, accent);
}

void drawMenuCard(const String& title, int16_t x, int16_t y, int16_t w, int16_t h) {
  auto& canvas = frameBuffer();
  canvas.fillRoundRect(x, y, w, h, 14, 0x18C3);
  canvas.drawRoundRect(x, y, w, h, 14, TFT_WHITE);
  canvas.setTextColor(TFT_WHITE, 0x18C3);
  canvas.drawCentreString(title, x + w / 2, y + 10, 2);
}

void drawMenuList(int16_t x, int16_t startY, int16_t w, const String* labels, size_t count, uint8_t selectedIndex) {
  auto& canvas = frameBuffer();
  for (uint8_t i = 0; i < count; ++i) {
    const bool selected = i == selectedIndex;
    const int16_t y = startY + i * 24;
    if (selected) {
      canvas.fillRoundRect(x, y - 3, w, 20, 6, 0xFFE0);
    }
    canvas.setTextColor(selected ? TFT_BLACK : TFT_WHITE, selected ? 0xFFE0 : 0x18C3);
    canvas.drawString(String(selected ? "> " : "  ") + labels[i], x + 4, y, 2);
  }
}

void openBrightnessMenu(AppModel& model) {
  model.overlay = OverlayState::Brightness;
  model.menuIndex = stepIndex(model.settings.brightness, kBrightnessSteps,
                              sizeof(kBrightnessSteps) / sizeof(kBrightnessSteps[0]));
}

void openVolumeMenu(AppModel& model) {
  model.overlay = OverlayState::Volume;
  model.menuIndex = stepIndex(model.settings.volume, kVolumeSteps,
                              sizeof(kVolumeSteps) / sizeof(kVolumeSteps[0]));
}

}  // namespace

void UiRouter::begin() {}

void UiRouter::handleButton(const ButtonEvent& event, AppModel& model) {
  if (!event.isValid()) {
    return;
  }

  if (isGuideVisible(model) && model.guide == GuideScreen::PairSetup && model.pairConfirmPending) {
    if (event.button == ButtonId::A && event.gesture == ButtonGesture::LongPress) {
      model.pairConfirmPending = false;
      model.guidePinned = false;
      model.pairSetupRequested = true;
      return;
    }
    if (event.button == ButtonId::B && event.gesture == ButtonGesture::LongPress) {
      model.pairConfirmPending = false;
      model.guidePinned = false;
      model.guide = GuideScreen::None;
      model.userDismissedGuide = false;
      return;
    }
    return;
  }

  if (event.button == ButtonId::A && event.gesture == ButtonGesture::LongPress) {
    if (model.overlay == OverlayState::Menu) {
      size_t count = 0;
      const MenuEntry* entries = activeMenuEntries(model, count);
      clampMenuIndex(model, count);
      if (count == 0) {
        model.overlay = OverlayState::None;
        return;
      }

      switch (entries[model.menuIndex].action) {
        case MenuAction::Brightness:
          openBrightnessMenu(model);
          break;
        case MenuAction::Volume:
          openVolumeMenu(model);
          break;
        case MenuAction::Wifi:
          model.overlay = OverlayState::None;
          model.guide = GuideScreen::WifiSetup;
          model.guidePinned = true;
          model.userDismissedGuide = false;
          model.wifiSetupRequested = true;
          break;
        case MenuAction::Pair:
          model.overlay = OverlayState::None;
          model.guide = GuideScreen::PairSetup;
          model.guidePinned = true;
          model.userDismissedGuide = false;
          model.pairConfirmPending = model.pairState == PairUiState::Ready;
          model.pairSetupRequested = model.pairState != PairUiState::Ready;
          break;
        case MenuAction::Demo:
          model.demoMode = !model.demoMode;
          model.overlay = OverlayState::None;
          break;
        case MenuAction::Back:
          model.overlay = OverlayState::None;
          break;
      }
      return;
    }

    if (model.overlay == OverlayState::Brightness) {
      if (model.menuIndex >= 4) {
        model.overlay = OverlayState::Menu;
        model.menuIndex = 0;
      } else {
        model.settings.brightness = kBrightnessSteps[model.menuIndex];
      }
      return;
    }

    if (model.overlay == OverlayState::Volume) {
      if (model.menuIndex >= 4) {
        model.overlay = OverlayState::Menu;
        model.menuIndex = 1;
      } else {
        model.settings.volume = kVolumeSteps[model.menuIndex];
      }
      return;
    }

    model.overlay = OverlayState::Menu;
    syncMenuIndex(model);
    return;
  }

  if (event.button == ButtonId::B && event.gesture == ButtonGesture::LongPress) {
    if (model.overlay == OverlayState::Brightness) {
      model.overlay = OverlayState::Menu;
      model.menuIndex = 0;
      return;
    }
    if (model.overlay == OverlayState::Volume) {
      model.overlay = OverlayState::Menu;
      model.menuIndex = 1;
      return;
    }
    if (model.overlay != OverlayState::None) {
      model.overlay = OverlayState::None;
      return;
    }
    if (isGuideVisible(model)) {
      if (model.guidePinned) {
        model.guidePinned = false;
        model.pairConfirmPending = false;
        model.guide = GuideScreen::None;
        model.userDismissedGuide = false;
      } else {
        model.userDismissedGuide = true;
      }
      return;
    }
    return;
  }

  if (model.overlay == OverlayState::Menu && event.gesture == ButtonGesture::ShortPress) {
    size_t count = 0;
    activeMenuEntries(model, count);
    if (count == 0) {
      return;
    }
    if (event.button == ButtonId::A) {
      model.menuIndex = static_cast<uint8_t>((model.menuIndex + 1U) % count);
    } else {
      model.menuIndex = static_cast<uint8_t>((model.menuIndex + count - 1U) % count);
    }
    return;
  }

  if ((model.overlay == OverlayState::Brightness || model.overlay == OverlayState::Volume) &&
      event.gesture == ButtonGesture::ShortPress) {
    constexpr uint8_t kSettingCount = 5;
    if (event.button == ButtonId::A) {
      model.menuIndex = static_cast<uint8_t>((model.menuIndex + 1U) % kSettingCount);
    } else {
      model.menuIndex = static_cast<uint8_t>((model.menuIndex + kSettingCount - 1U) % kSettingCount);
    }
    return;
  }

  if (event.gesture != ButtonGesture::ShortPress) {
    return;
  }

  if (isGuideVisible(model)) {
    model.userDismissedGuide = true;
  }

  cycleBaseScreen(model, event.button == ButtonId::A);
}

void UiRouter::tick(AppModel&) {}

void UiRouter::draw(const AppModel& model, const PetRuntime& petRuntime) const {
  beginFrame();

  if (isGuideVisible(model)) {
    drawGuide(model);
  } else {
    drawBase(model, petRuntime);
  }

  if (model.overlay == OverlayState::Menu) {
    drawMenuOverlay(model);
  } else if (model.overlay == OverlayState::Brightness) {
    drawBrightnessOverlay(model);
  } else if (model.overlay == OverlayState::Volume) {
    drawVolumeOverlay(model);
  }

  presentFrame();
}

void UiRouter::cycleBaseScreen(AppModel& model, bool forward) const {
  uint8_t raw = static_cast<uint8_t>(model.currentBaseScreen);
  raw = forward ? (raw + 1U) % 3U : (raw + 2U) % 3U;
  model.currentBaseScreen = static_cast<BaseScreen>(raw);
}

void UiRouter::drawBase(const AppModel& model, const PetRuntime& petRuntime) const {
  switch (model.currentBaseScreen) {
    case BaseScreen::Pet:
      drawPet(model, petRuntime);
      break;
    case BaseScreen::Overview:
      drawOverview(model, petRuntime);
      break;
    case BaseScreen::Info:
      drawInfo(model);
      break;
  }
}

void UiRouter::drawPet(const AppModel& model, const PetRuntime& petRuntime) const {
  petRuntime.draw(4, 8, 4, model.snapshot.petState);
  const String headline = petHeadline(model);
  if (!headline.isEmpty()) {
    drawCenteredCopy(headline, 180, 0xFFE0, 4);
  }
  statusLights_.draw(model.snapshot, 27, 210, 16, 8);
}

void UiRouter::drawOverview(const AppModel& model, const PetRuntime& petRuntime) const {
  (void)petRuntime;
  drawCenteredCopy(overviewHeadline(model), 12, 0xFFE0, 2);
  drawStatBar("Mood", model.petStats.mood, 42, 0xFCAA);
  drawStatBar("Energy", model.petStats.energy, 98, 0xA7E0);
  drawStatBar("Focus", model.petStats.focus, 154, 0x8D7F);
  drawCenteredCopy(overviewOnlineLabel(model), 208, overviewOnlineLabel(model) == "Online" ? 0x07E0 : 0xFD20, 2);
  drawCenteredCopy(requiresUserLine(model), 224, 0xC618, 1);
}

void UiRouter::drawInfo(const AppModel& model) const {
  const String ssid = model.network.wifiConnected
                          ? fitText(model.settings.wifiSsid.isEmpty() ? model.network.wifiStatusLabel : model.settings.wifiSsid, 16)
                          : "Not connected";

  drawCenteredCopy("Info", 10, 0x9E7F, 2);
  drawInfoSection("Battery", batteryLevelLabel(), 34, batteryAccentColor(), 4);
  drawCenteredCopy(chargingLabel(), 82, batteryAccentColor(), 2);

  drawInfoSection("Wi-Fi", ssid, 118, wifiAccentColor(model), model.network.wifiConnected ? 2 : 3);
  drawCenteredCopy(model.network.wifiConnected ? "Connected" : "Offline", 166, wifiAccentColor(model), 2);

  drawInfoSection("Firmware", fitText(kFirmwareVersion, 16), 194, TFT_WHITE, 2);
}

void UiRouter::drawGuide(const AppModel& model) const {
  switch (model.guide) {
    case GuideScreen::WifiSetup:
      drawWifiGuide(model);
      break;
    case GuideScreen::PairSetup:
      drawPairGuide(model);
      break;
    case GuideScreen::None:
      break;
  }
}

void UiRouter::drawWifiGuide(const AppModel& model) const {
  drawCenteredCopy("Wi-Fi Setup", 12, 0xFFE0, 2);
  drawLabelValue("Portal", model.network.portalSsid.isEmpty() ? "Open menu" : fitText(model.network.portalSsid, 18), 42);
  drawLabelValue("Open", kDefaultPortalHost, 62);
  drawLabelValue("Saved", model.settings.wifiSsid.isEmpty() ? "No" : fitText(model.settings.wifiSsid, 18), 82);
  drawLabelValue("State",
                 String(reinterpret_cast<const __FlashStringHelper*>(toFlashString(model.wifiState))), 102);
  drawCenteredCopy(fitText(model.network.portalStatus, 22), 144, TFT_WHITE, 2);
  drawCenteredCopy("A hold menu", 198, 0xC618, 2);
  drawCenteredCopy("B hold back", 216, 0xC618, 2);
}

void UiRouter::drawPairGuide(const AppModel& model) const {
  auto& canvas = frameBuffer();
  drawCenteredCopy(model.pairConfirmPending ? "Re-pair Device" : "Pair Device", 12, 0xFFE0, 2);
  if (model.pairConfirmPending) {
    drawCenteredCopy("Device already bound", 60, TFT_WHITE, 2);
    drawCenteredCopy("A hold for new code", 94, TFT_WHITE, 2);
    drawCenteredCopy("B hold keeps current link", 150, 0xC618, 1);
    return;
  }

  canvas.setTextColor(0xFFE0, TFT_BLACK);
  canvas.drawCentreString(model.pairing.pairCode, canvas.width() / 2, 40, 4);
  drawCenteredCopy(fitText(model.pairing.statusLabel, 18), 88, TFT_WHITE, 2);
  drawCenteredCopy(fitText(model.pairing.pairUrl.isEmpty() ? "Open buddy web" : model.pairing.pairUrl, 22), 112,
                   TFT_WHITE, 1);
  if (!model.pairing.errorDetail.isEmpty()) {
    drawCenteredCopy(fitText(model.pairing.errorDetail, 22), 136, 0xFD20, 1);
  }
  drawCenteredCopy("Short press: main pages", 196, 0xC618, 1);
  drawCenteredCopy("A hold menu / B hold back", 214, 0xC618, 1);
}

void UiRouter::drawMenuOverlay(const AppModel& model) const {
  auto& canvas = frameBuffer();
  const int16_t x = 10;
  const int16_t y = 16;
  const int16_t w = canvas.width() - 20;
  const int16_t h = canvas.height() - 32;
  drawMenuCard("Menu", x, y, w, h);

  size_t count = 0;
  const MenuEntry* entries = activeMenuEntries(model, count);
  String labels[7];
  for (uint8_t i = 0; i < count && i < 7; ++i) {
    labels[i] = menuLabelFor(model, entries[i].action, entries[i].label);
  }
  drawMenuList(x + 10, y + 38, w - 20, labels, count, model.menuIndex);
}

void UiRouter::drawBrightnessOverlay(const AppModel& model) const {
  auto& canvas = frameBuffer();
  const int16_t x = 10;
  const int16_t y = 16;
  const int16_t w = canvas.width() - 20;
  const int16_t h = canvas.height() - 32;
  drawMenuCard("Brightness", x, y, w, h);

  String labels[5];
  for (uint8_t i = 0; i < 4; ++i) {
    labels[i] = levelLabel(i);
  }
  labels[4] = "Back";
  drawMenuList(x + 10, y + 38, w - 20, labels, 5, model.menuIndex);

  const uint8_t active = stepIndex(model.settings.brightness, kBrightnessSteps,
                                   sizeof(kBrightnessSteps) / sizeof(kBrightnessSteps[0]));
  canvas.setTextColor(0xBDF7, 0x18C3);
  canvas.drawCentreString("Current: " + levelLabel(active), canvas.width() / 2, y + h - 22, 1);
}

void UiRouter::drawVolumeOverlay(const AppModel& model) const {
  auto& canvas = frameBuffer();
  const int16_t x = 10;
  const int16_t y = 16;
  const int16_t w = canvas.width() - 20;
  const int16_t h = canvas.height() - 32;
  drawMenuCard("Volume", x, y, w, h);

  String labels[5];
  for (uint8_t i = 0; i < 4; ++i) {
    labels[i] = levelLabel(i);
  }
  labels[4] = "Back";
  drawMenuList(x + 10, y + 38, w - 20, labels, 5, model.menuIndex);

  const uint8_t active = stepIndex(model.settings.volume, kVolumeSteps,
                                   sizeof(kVolumeSteps) / sizeof(kVolumeSteps[0]));
  canvas.setTextColor(0xBDF7, 0x18C3);
  canvas.drawCentreString("Current: " + levelLabel(active), canvas.width() / 2, y + h - 22, 1);
}

}  // namespace buddy
