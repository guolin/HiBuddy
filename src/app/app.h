#pragma once

#include "app_types.h"
#include "board/input.h"
#include "net/api_client.h"
#include "net/mqtt_client.h"
#include "net/wifi_manager.h"
#include "pet/pet_runtime.h"
#include "runtime/snapshot_runtime.h"
#include "storage/asset_store.h"
#include "storage/settings_store.h"
#include "ui/router.h"

namespace buddy {

class BuddyApp {
 public:
  void begin();
  void tick();

 private:
  AppModel model_;
  InputManager input_;
  UiRouter uiRouter_;
  PetRuntime petRuntime_;
  SnapshotRuntime snapshotRuntime_;
  SettingsStore settingsStore_;
  AssetStore assetStore_;
  WifiManager wifiManager_;
  ApiClient apiClient_;
  MqttClient mqttClient_;
  uint32_t lastInteractionAt_{0};
  uint32_t lastPairBootstrapPollAt_{0};
  uint32_t lastPairCodeRequestedAt_{0};
  uint32_t lastDemoStepAt_{0};
  uint8_t demoScenarioIndex_{0};
  bool lastWifiConnected_{false};

  void bootstrapRuntime();
  void updateSystemState();
  void processButton();
  void pollPairingBootstrap();
  void requestPairCode(bool refreshing = false);
  void clearMqttRuntime();
  void applyBootstrapToNetwork(const BootstrapPayload& bootstrap);
  void handleUiActions();
  void tickDemoMode();
  void applyDemoSnapshot(uint8_t scenarioIndex);
};

}  // namespace buddy
