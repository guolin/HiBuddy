#pragma once

#include <Arduino.h>
#include <array>

#include "app_config.h"

namespace buddy {

enum class SystemState : uint8_t {
  Boot,
  NoWifi,
  WifiPortal,
  ConnectingWifi,
  NoBinding,
  Pairing,
  Syncing,
  Running,
  OfflineDegraded,
  Error,
  ScreenSleep,
};

enum class BaseScreen : uint8_t {
  Pet,
  Overview,
  Info,
};

enum class OverlayState : uint8_t {
  None,
  Menu,
  Brightness,
  Volume,
};

enum class GuideScreen : uint8_t {
  None,
  WifiSetup,
  PairSetup,
};

enum class WifiUiState : uint8_t {
  Missing,
  Connecting,
  Ready,
};

enum class PairUiState : uint8_t {
  Missing,
  Pending,
  Ready,
};

enum class RuntimeUiState : uint8_t {
  Normal,
  Degraded,
};

enum class ReminderType : uint8_t {
  None,
  Waiting,
  Error,
  Done,
  Stale,
};

enum class PetState : uint8_t {
  Idle = 0,
  Thinking = 1,
  Busy = 2,
  Waiting = 3,
  Error = 4,
  Done = 5,
  Sleep = 6,
  Offline = 7,
};

enum class SessionState : uint8_t {
  Thinking = 0,
  Busy = 1,
  Waiting = 2,
  Error = 3,
  Done = 4,
};

enum class ButtonId : uint8_t {
  A,
  B,
};

enum class ButtonGesture : uint8_t {
  None,
  ShortPress,
  LongPress,
};

struct ButtonEvent {
  ButtonId button{ButtonId::A};
  ButtonGesture gesture{ButtonGesture::None};

  bool isValid() const { return gesture != ButtonGesture::None; }
};

struct Snapshot {
  uint8_t version{1};
  uint32_t updatedAt{0};
  PetState petState{PetState::Idle};
  std::array<SessionState, kSessionLightCount> sessionStates{
      SessionState::Thinking,
      SessionState::Thinking,
      SessionState::Thinking,
      SessionState::Thinking,
  };
  uint8_t sessionCount{0};
  String focusTitle{"Buddy is ready."};
  bool requiresUser{false};
  uint32_t totalTokens{0};
  uint32_t todayTokens{0};
};

struct PetStats {
  uint8_t mood{60};
  uint8_t energy{70};
  uint8_t focus{50};
  uint32_t lastInteractionMs{0};
  uint32_t lastRestMs{0};
  uint32_t lastFriendlyShakeAtMs{0};
  uint32_t heartUntilMs{0};
};

struct DeviceSettings {
  String deviceId{kDefaultDeviceId};
  String deviceToken;
  String wifiSsid;
  String wifiPassword;
  uint8_t brightness{192};
  uint8_t volume{75};
  uint8_t petEnergy{70};
  bool demoMode{false};
  BaseScreen preferredBaseScreen{BaseScreen::Overview};
};

struct PairingState {
  String pairCode{"------"};
  String pairUrl;
  String statusLabel{"Pending"};
  String errorDetail;
  uint32_t expiresAtEpoch{0};
  bool paired{false};
};

struct NetworkState {
  bool wifiConfigured{false};
  bool wifiConnected{false};
  bool portalActive{false};
  bool portalHasCredentials{false};
  bool mqttConfigured{false};
  bool mqttConnected{false};
  int32_t wifiRssi{0};
  String portalSsid;
  String portalStatus{"Connect to AP and open 192.168.4.1"};
  String wifiStatusLabel{"Offline"};
  String mqttBrokerUrl;
  String mqttUsername;
  String mqttToken;
  String mqttClientId;
  String mqttTopic;
  String mqttStatus{"Idle"};
  String lastError;
  uint32_t lastSnapshotMs{0};
};

struct ReminderState {
  ReminderType active{ReminderType::None};
  uint32_t startedAtMs{0};
};

struct AppModel {
  SystemState systemState{SystemState::Boot};
  BaseScreen currentBaseScreen{BaseScreen::Overview};
  OverlayState overlay{OverlayState::None};
  GuideScreen guide{GuideScreen::None};
  WifiUiState wifiState{WifiUiState::Missing};
  PairUiState pairState{PairUiState::Missing};
  RuntimeUiState runtimeState{RuntimeUiState::Normal};
  bool userDismissedGuide{false};
  bool guidePinned{false};
  bool pairConfirmPending{false};
  uint8_t menuIndex{0};
  Snapshot snapshot;
  PetStats petStats;
  DeviceSettings settings;
  PairingState pairing;
  NetworkState network;
  ReminderState reminder;
  String petName{kDefaultPetName};
  String assetVersion{kDefaultAssetVersion};
  String petPackId{"demo-pack"};
  bool demoMode{false};
  bool faceDownSleepActive{false};
  uint32_t motionDizzyUntilMs{0};
  bool wifiSetupRequested{false};
  bool pairSetupRequested{false};
  bool restartRequested{false};
};

const __FlashStringHelper* toFlashString(SystemState state);
const __FlashStringHelper* toFlashString(BaseScreen screen);
const __FlashStringHelper* toFlashString(GuideScreen screen);
const __FlashStringHelper* toFlashString(OverlayState overlay);
const __FlashStringHelper* toFlashString(PetState petState);
const __FlashStringHelper* toFlashString(SessionState sessionState);
const __FlashStringHelper* toFlashString(ReminderType reminder);
const __FlashStringHelper* toFlashString(WifiUiState state);
const __FlashStringHelper* toFlashString(PairUiState state);
const __FlashStringHelper* toFlashString(RuntimeUiState state);

}  // namespace buddy
