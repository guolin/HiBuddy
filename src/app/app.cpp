#include "app/app.h"

#include <M5Unified.h>
#include <math.h>

#include "app_config.h"
#include "board/board_init.h"

namespace buddy {

namespace {
constexpr uint32_t kDemoStepIntervalMs = 8000;
constexpr uint32_t kMotionDizzyMs = 2200;
constexpr uint32_t kShakeCooldownMs = 1800;
constexpr uint32_t kFaceDownHoldMs = 450;
constexpr float kShakeDeltaThreshold = 1.05f;
constexpr float kDominantZThreshold = 0.78f;
constexpr float kFlatAxisThreshold = 0.42f;
constexpr float kWakeZThreshold = 0.45f;
}

void BuddyApp::clearMqttRuntime() {
  model_.network.mqttConfigured = false;
  model_.network.mqttConnected = false;
  model_.network.mqttBrokerUrl = "";
  model_.network.mqttUsername = "";
  model_.network.mqttToken = "";
  model_.network.mqttClientId = "";
  model_.network.mqttTopic = "";
  model_.network.mqttStatus = "Idle";
  model_.network.lastSnapshotMs = 0;
}

void BuddyApp::applyBootstrapToNetwork(const BootstrapPayload& bootstrap) {
  model_.network.mqttBrokerUrl = bootstrap.mqttBrokerUrl;
  model_.network.mqttUsername = bootstrap.mqttUsername;
  model_.network.mqttToken = bootstrap.mqttToken;
  model_.network.mqttClientId = bootstrap.mqttClientId;
  model_.network.mqttTopic = bootstrap.mqttTopic;
  model_.network.mqttStatus = "Config ready";
}

void BuddyApp::requestPairCode(bool refreshing) {
  if (refreshing) {
    model_.pairing.statusLabel = "Refreshing";
  }

  model_.pairing = apiClient_.requestPairCode(model_.settings);
  model_.pairing.paired = false;
  model_.pairConfirmPending = false;
  model_.guidePinned = false;
  settingsStore_.save(model_.settings);
  lastPairCodeRequestedAt_ = millis();
  lastPairBootstrapPollAt_ = lastPairCodeRequestedAt_;
  model_.guide = GuideScreen::PairSetup;
  model_.userDismissedGuide = false;
  Serial.printf("[buddy][pair] pairCode=%s pairUrl=%s token_len=%u\n", model_.pairing.pairCode.c_str(),
                model_.pairing.pairUrl.c_str(), static_cast<unsigned>(model_.settings.deviceToken.length()));
}

void BuddyApp::begin() {
  initializeBoard();

  settingsStore_.begin();
  assetStore_.begin();

  model_.settings = settingsStore_.load();
  model_.currentBaseScreen = model_.settings.preferredBaseScreen;
  model_.demoMode = model_.settings.demoMode;
  model_.assetVersion = assetStore_.activeAssetVersion();

  applyBacklight(model_.settings.brightness);
  assetStore_.ensureDemoPack();
  input_.begin();
  uiRouter_.begin();
  petRuntime_.begin();
  snapshotRuntime_.begin(model_.snapshot);
  wifiManager_.begin(model_.settings);
  apiClient_.begin(kApiBaseUrl);
  mqttClient_.begin();

  bootstrapRuntime();
  updateSystemState();
  lastWifiConnected_ = model_.network.wifiConnected;
  lastInteractionAt_ = millis();
  if (model_.demoMode) {
    applyDemoSnapshot(0);
    lastDemoStepAt_ = millis();
  }
}

void BuddyApp::tick() {
  processButton();
  wifiManager_.tick(model_.network, model_.settings);
  processMotion();

  if (model_.network.wifiConnected && !lastWifiConnected_) {
    settingsStore_.save(model_.settings);
    Serial.println("[buddy][wifi] wifi ready, continuing online bootstrap");
    bootstrapRuntime();
  }
  lastWifiConnected_ = model_.network.wifiConnected;

  pollPairingBootstrap();

  if (model_.demoMode) {
    tickDemoMode();
  } else if (model_.pairing.paired) {
    if (mqttClient_.tick(model_.network, model_.snapshot)) {
      snapshotRuntime_.onSnapshot(model_);
    }
  } else {
    mqttClient_.disconnect();
    model_.network.mqttConnected = false;
  }

  petRuntime_.tick(model_);
  snapshotRuntime_.tick(model_);
  updateSystemState();
  updateDisplaySleep();
  uiRouter_.tick(model_);
  uiRouter_.draw(model_, petRuntime_);
  delay(16);
}

void BuddyApp::bootstrapRuntime() {
  model_.network.wifiConfigured = !model_.settings.wifiSsid.isEmpty();

  if (!model_.network.wifiConfigured) {
    clearMqttRuntime();
    mqttClient_.disconnect();
    if (!model_.network.portalActive) {
      wifiManager_.startPortal(model_.network);
    }
    model_.guide = GuideScreen::WifiSetup;
    model_.guidePinned = false;
    model_.userDismissedGuide = false;
    return;
  }

  if (!model_.network.wifiConnected) {
    model_.network.mqttConnected = false;
    model_.network.mqttStatus = "WiFi down";
    if (model_.network.portalStatus.isEmpty()) {
      model_.network.portalStatus = "Waiting for Wi-Fi...";
    }
    return;
  }

  if (model_.network.portalActive) {
    wifiManager_.stopPortal(model_.network);
  }

  if (model_.settings.deviceToken.isEmpty()) {
    clearMqttRuntime();
    mqttClient_.disconnect();
    requestPairCode(false);
    return;
  }

  const BootstrapPayload bootstrap = apiClient_.bootstrap(model_.settings);
  if (bootstrap.ok && bootstrap.paired) {
    model_.pairing.paired = true;
    model_.pairing.statusLabel = "Bound";
    model_.pairing.errorDetail = "";
    model_.petPackId = bootstrap.petPackId;
    model_.petName = bootstrap.petName;
    model_.assetVersion = bootstrap.assetVersion;
    applyBootstrapToNetwork(bootstrap);
    mqttClient_.configure(model_.network);
  } else {
    clearMqttRuntime();
    mqttClient_.disconnect();
    Serial.println("[buddy][pair] bootstrap says paired=false, requesting fresh pair code");
    requestPairCode(true);
  }
}

void BuddyApp::updateSystemState() {
  const uint32_t now = millis();

  const bool idleSleep =
      now - lastInteractionAt_ > kScreenSleepMs && model_.overlay == OverlayState::None;
  if (model_.faceDownSleepActive || idleSleep) {
    model_.systemState = SystemState::ScreenSleep;
  }

  if (model_.network.wifiConnected) {
    model_.wifiState = WifiUiState::Ready;
    model_.network.wifiStatusLabel = "Ready";
  } else if (model_.network.portalActive || model_.network.portalHasCredentials || !model_.settings.wifiSsid.isEmpty()) {
    model_.wifiState = WifiUiState::Connecting;
    model_.network.wifiStatusLabel = model_.network.portalStatus;
  } else {
    model_.wifiState = WifiUiState::Missing;
    model_.network.wifiStatusLabel = "Missing";
  }

  if (model_.pairing.paired) {
    model_.pairState = PairUiState::Ready;
  } else if (!model_.settings.deviceToken.isEmpty()) {
    model_.pairState = PairUiState::Pending;
  } else {
    model_.pairState = PairUiState::Missing;
  }

  const bool snapshotStale =
      model_.network.lastSnapshotMs != 0 && millis() - model_.network.lastSnapshotMs > kStaleSnapshotMs;
  model_.runtimeState = (model_.pairState == PairUiState::Ready && (!model_.network.mqttConnected || snapshotStale))
                            ? RuntimeUiState::Degraded
                            : RuntimeUiState::Normal;

  GuideScreen desiredGuide = GuideScreen::None;
  if (model_.wifiState != WifiUiState::Ready) {
    desiredGuide = GuideScreen::WifiSetup;
  } else if (model_.pairState != PairUiState::Ready) {
    desiredGuide = GuideScreen::PairSetup;
  }

  if (model_.guidePinned && model_.guide != GuideScreen::None) {
    model_.userDismissedGuide = false;
  } else {
    if (desiredGuide == GuideScreen::None) {
      model_.guide = GuideScreen::None;
      model_.userDismissedGuide = false;
    } else {
      if (model_.guide != desiredGuide) {
        model_.userDismissedGuide = false;
      }
      model_.guide = desiredGuide;
    }
  }

  if (model_.faceDownSleepActive || idleSleep) {
    return;
  }

  if (model_.wifiState == WifiUiState::Missing) {
    model_.systemState = SystemState::NoWifi;
  } else if (model_.wifiState == WifiUiState::Connecting) {
    model_.systemState = SystemState::ConnectingWifi;
  } else if (model_.pairState != PairUiState::Ready) {
    model_.systemState = model_.pairState == PairUiState::Pending ? SystemState::Pairing : SystemState::NoBinding;
  } else if (model_.runtimeState == RuntimeUiState::Degraded) {
    model_.systemState = SystemState::OfflineDegraded;
  } else {
    model_.systemState = SystemState::Running;
  }
}

void BuddyApp::handleUiActions() {
  if (model_.wifiSetupRequested) {
    if (!model_.network.portalActive) {
      wifiManager_.startPortal(model_.network);
    }
    model_.wifiSetupRequested = false;
  }

  if (model_.pairSetupRequested) {
    if (model_.network.wifiConnected) {
      requestPairCode(true);
    }
    model_.pairSetupRequested = false;
  }

  if (model_.restartRequested) {
    ESP.restart();
  }
}

void BuddyApp::processButton() {
  const ButtonEvent event = input_.poll();
  if (!event.isValid()) {
    return;
  }

  lastInteractionAt_ = millis();
  if (model_.systemState == SystemState::ScreenSleep) {
    model_.systemState = SystemState::Running;
    model_.faceDownSleepActive = false;
    model_.motionDizzyUntilMs = 0;
    return;
  }

  const uint8_t previousBrightness = model_.settings.brightness;
  const uint8_t previousVolume = model_.settings.volume;
  const BaseScreen previousBaseScreen = model_.currentBaseScreen;
  const bool previousDemoMode = model_.demoMode;

  uiRouter_.handleButton(event, model_);
  handleUiActions();
  model_.settings.preferredBaseScreen = model_.currentBaseScreen;
  model_.settings.demoMode = model_.demoMode;

  if (model_.demoMode != previousDemoMode) {
    if (model_.demoMode) {
      mqttClient_.disconnect();
      applyDemoSnapshot(0);
      lastDemoStepAt_ = millis();
      demoScenarioIndex_ = 0;
      Serial.println("[buddy][demo] enabled");
    } else {
      model_.network.mqttConnected = false;
      model_.network.mqttStatus = "Idle";
      Serial.println("[buddy][demo] disabled");
    }
  }

  if (model_.settings.brightness != previousBrightness) {
    applyBacklight(model_.settings.brightness);
    settingsStore_.save(model_.settings);
  } else if (model_.settings.volume != previousVolume || model_.currentBaseScreen != previousBaseScreen ||
             model_.demoMode != previousDemoMode) {
    settingsStore_.save(model_.settings);
  }
}

void BuddyApp::processMotion() {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) {
    return;
  }

  const uint32_t now = millis();
  const float absX = fabsf(ax);
  const float absY = fabsf(ay);
  const float absZ = fabsf(az);

  if (!imuOrientationCalibrated_ && absZ >= kDominantZThreshold && absX < 0.7f && absY < 0.7f) {
    screenUpZSign_ = az >= 0.0f ? 1.0f : -1.0f;
    imuOrientationCalibrated_ = true;
    Serial.printf("[buddy][imu] calibrated screenUpZSign=%.0f\n", screenUpZSign_);
  }

  if (accelValid_) {
    const float deltaX = ax - lastAccelX_;
    const float deltaY = ay - lastAccelY_;
    const float deltaZ = az - lastAccelZ_;
    const float shakeDelta = sqrtf(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
    if (!model_.faceDownSleepActive && now - lastShakeAt_ >= kShakeCooldownMs && shakeDelta >= kShakeDeltaThreshold) {
      lastShakeAt_ = now;
      model_.motionDizzyUntilMs = now + kMotionDizzyMs;
      lastInteractionAt_ = now;
      Serial.printf("[buddy][imu] shake detected delta=%.2f dizzy_until=%lu\n", shakeDelta,
                    static_cast<unsigned long>(model_.motionDizzyUntilMs));
    }
  }

  lastAccelX_ = ax;
  lastAccelY_ = ay;
  lastAccelZ_ = az;
  accelValid_ = true;

  if (!imuOrientationCalibrated_) {
    return;
  }

  const bool faceDownNow =
      (az * screenUpZSign_) <= -kDominantZThreshold && absX <= kFlatAxisThreshold && absY <= kFlatAxisThreshold;
  const bool faceUpNow = (az * screenUpZSign_) >= -kWakeZThreshold;

  if (!model_.faceDownSleepActive) {
    if (faceDownNow) {
      if (faceDownCandidateAt_ == 0) {
        faceDownCandidateAt_ = now;
      } else if (now - faceDownCandidateAt_ >= kFaceDownHoldMs) {
        model_.faceDownSleepActive = true;
        model_.motionDizzyUntilMs = 0;
        faceDownCandidateAt_ = 0;
        faceUpCandidateAt_ = 0;
        Serial.println("[buddy][imu] face-down sleep active");
      }
    } else {
      faceDownCandidateAt_ = 0;
    }
    return;
  }

  if (faceUpNow) {
    if (faceUpCandidateAt_ == 0) {
      faceUpCandidateAt_ = now;
    } else if (now - faceUpCandidateAt_ >= 180) {
      model_.faceDownSleepActive = false;
      model_.systemState = SystemState::Running;
      lastInteractionAt_ = now;
      faceUpCandidateAt_ = 0;
      faceDownCandidateAt_ = 0;
      Serial.println("[buddy][imu] face-down sleep cleared");
    }
  } else {
    faceUpCandidateAt_ = 0;
  }
}

void BuddyApp::updateDisplaySleep() {
  const bool shouldSleep = model_.systemState == SystemState::ScreenSleep;
  if (shouldSleep == displaySleepApplied_) {
    return;
  }

  displaySleepApplied_ = shouldSleep;
  applyBacklight(shouldSleep ? 0 : model_.settings.brightness);
}

void BuddyApp::pollPairingBootstrap() {
  if (!model_.network.wifiConnected || model_.settings.deviceToken.isEmpty() || model_.pairing.paired) {
    return;
  }

  if (millis() - lastPairBootstrapPollAt_ < kPairBootstrapPollMs) {
    return;
  }

  if (lastPairCodeRequestedAt_ != 0 &&
      millis() - lastPairCodeRequestedAt_ >= (kPairCodeLifetimeMs - kPairCodeRefreshLeadMs)) {
    Serial.println("[buddy][pair] pair code expired or near expiry, requesting refresh");
    requestPairCode(true);
    return;
  }

  lastPairBootstrapPollAt_ = millis();
  const BootstrapPayload bootstrap = apiClient_.bootstrap(model_.settings);
  if (!bootstrap.ok) {
    model_.pairing.statusLabel = "Bootstrap failed";
    model_.pairing.errorDetail = bootstrap.error;
    return;
  }

  if (!bootstrap.paired) {
    model_.pairing.statusLabel = "Pending";
    Serial.println("[buddy][pair] bootstrap says paired=false");
    return;
  }

  Serial.printf("[buddy][pair] paired=true mqttTopic=%s pet=%s version=%s\n", bootstrap.mqttTopic.c_str(),
                bootstrap.petPackId.c_str(), bootstrap.assetVersion.c_str());
  model_.pairing.paired = true;
  model_.pairing.statusLabel = "Bound";
  model_.pairing.errorDetail = "";
  model_.petPackId = bootstrap.petPackId;
  model_.petName = bootstrap.petName;
  model_.assetVersion = bootstrap.assetVersion;
  applyBootstrapToNetwork(bootstrap);
  mqttClient_.configure(model_.network);
}

void BuddyApp::tickDemoMode() {
  model_.network.mqttConnected = true;
  model_.network.mqttStatus = "Demo";
  model_.network.lastSnapshotMs = millis();

  if (lastDemoStepAt_ == 0 || millis() - lastDemoStepAt_ >= kDemoStepIntervalMs) {
    demoScenarioIndex_ = static_cast<uint8_t>((demoScenarioIndex_ + 1U) % 5U);
    applyDemoSnapshot(demoScenarioIndex_);
    lastDemoStepAt_ = millis();
  }
}

void BuddyApp::applyDemoSnapshot(uint8_t scenarioIndex) {
  Snapshot snapshot = model_.snapshot;
  snapshot.version = 1;
  snapshot.updatedAt = millis() / 1000U;
  snapshot.totalTokens += 1200;
  snapshot.todayTokens += 240;

  switch (scenarioIndex % 5U) {
    case 0:
      snapshot.petState = PetState::Idle;
      snapshot.focusTitle = "Buddy is ready.";
      snapshot.requiresUser = false;
      snapshot.sessionCount = 0;
      snapshot.sessionStates = {SessionState::Thinking, SessionState::Thinking, SessionState::Thinking,
                                SessionState::Thinking};
      break;
    case 1:
      snapshot.petState = PetState::Thinking;
      snapshot.focusTitle = "Planning next coding step";
      snapshot.requiresUser = false;
      snapshot.sessionCount = 1;
      snapshot.sessionStates = {SessionState::Thinking, SessionState::Thinking, SessionState::Thinking,
                                SessionState::Thinking};
      break;
    case 2:
      snapshot.petState = PetState::Busy;
      snapshot.focusTitle = "Running project checks";
      snapshot.requiresUser = false;
      snapshot.sessionCount = 2;
      snapshot.sessionStates = {SessionState::Busy, SessionState::Thinking, SessionState::Thinking,
                                SessionState::Thinking};
      break;
    case 3:
      snapshot.petState = PetState::Waiting;
      snapshot.focusTitle = "Waiting for your input";
      snapshot.requiresUser = true;
      snapshot.sessionCount = 3;
      snapshot.sessionStates = {SessionState::Waiting, SessionState::Busy, SessionState::Thinking,
                                SessionState::Thinking};
      break;
    case 4:
    default:
      snapshot.petState = PetState::Done;
      snapshot.focusTitle = "Last task completed";
      snapshot.requiresUser = false;
      snapshot.sessionCount = 4;
      snapshot.sessionStates = {SessionState::Done, SessionState::Done, SessionState::Busy, SessionState::Waiting};
      break;
  }

  model_.snapshot = snapshot;
  model_.network.lastSnapshotMs = millis();
  snapshotRuntime_.onSnapshot(model_);
}

}  // namespace buddy
