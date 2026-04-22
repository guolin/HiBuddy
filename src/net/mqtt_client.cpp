#include "net/mqtt_client.h"

#include <ArduinoJson.h>

#include "app_config.h"

namespace buddy {

namespace {

constexpr uint32_t kReconnectIntervalMs = 3000;

String redactToken(const String& value) {
  if (value.length() <= 6) {
    return "***";
  }
  return value.substring(0, 3) + "***" + value.substring(value.length() - 3);
}

}  // namespace

MqttClient* MqttClient::activeInstance_ = nullptr;

void MqttClient::begin() {
  activeInstance_ = this;
  secureClient_.setCACert(kEmqxRootCa);
  secureClient_.setTimeout(10000);
  client_.setClient(secureClient_);
  client_.setBufferSize(2048);
  client_.setCallback(onMessageThunk);
  Serial.println("[buddy][mqtt] TLS configured with CA certificate");
}

void MqttClient::configure(const NetworkState& network) {
  BrokerConfig nextConfig;
  nextConfig.brokerUrl = network.mqttBrokerUrl;
  nextConfig.username = network.mqttUsername;
  nextConfig.token = network.mqttToken;
  nextConfig.clientId = network.mqttClientId;
  nextConfig.topic = network.mqttTopic;

  if (nextConfig.brokerUrl.isEmpty() || nextConfig.topic.isEmpty() || nextConfig.clientId.isEmpty()) {
    disconnect();
    return;
  }

  if (!parseBrokerUrl(nextConfig.brokerUrl, nextConfig)) {
    Serial.printf("[buddy][mqtt] invalid broker URL: %s\n", nextConfig.brokerUrl.c_str());
    disconnect();
    return;
  }

  const bool changed = nextConfig.brokerUrl != config_.brokerUrl || nextConfig.username != config_.username ||
                       nextConfig.token != config_.token || nextConfig.clientId != config_.clientId ||
                       nextConfig.topic != config_.topic;
  if (!changed) {
    return;
  }

  disconnect();
  config_ = nextConfig;
  client_.setServer(config_.host.c_str(), config_.port);
  Serial.printf("[buddy][mqtt] configured broker=%s:%u tls=%s client=%s topic=%s user=%s token=%s token_len=%u\n",
                config_.host.c_str(), static_cast<unsigned>(config_.port), config_.tls ? "yes" : "no",
                config_.clientId.c_str(), config_.topic.c_str(), config_.username.c_str(),
                redactToken(config_.token).c_str(), static_cast<unsigned>(config_.token.length()));
}

void MqttClient::disconnect() {
  if (client_.connected()) {
    client_.disconnect();
  }
  subscribed_ = false;
  receivedSnapshot_ = false;
  snapshotDirty_ = false;
  lastConnectAttemptAt_ = 0;
  config_ = BrokerConfig{};
}

bool MqttClient::tick(NetworkState& network, Snapshot& snapshot) {
  network.mqttConfigured = !config_.topic.isEmpty() && !config_.host.isEmpty();
  if (!network.wifiConnected || !network.mqttConfigured) {
    network.mqttConnected = false;
    if (network.mqttConfigured && !network.wifiConnected) {
      network.mqttStatus = "WiFi down";
    } else if (!network.mqttConfigured) {
      network.mqttStatus = "No config";
    }
    if (client_.connected()) {
      client_.disconnect();
    }
    subscribed_ = false;
    return false;
  }

  if (!ensureConnected(network)) {
    network.mqttConnected = false;
    return false;
  }

  if (!client_.loop()) {
    network.mqttConnected = false;
    network.mqttStatus = "Loop failed";
    Serial.printf("[buddy][mqtt] loop failed state=%d\n", client_.state());
    client_.disconnect();
    subscribed_ = false;
    return false;
  }

  network.mqttConnected = client_.connected() && subscribed_;
  network.mqttStatus = receivedSnapshot_ ? "Live" : "Waiting msg";

  if (!snapshotDirty_) {
    return false;
  }

  snapshot = pendingSnapshot_;
  snapshotDirty_ = false;
  network.lastSnapshotMs = millis();
  network.mqttStatus = "Snapshot ok";
  Serial.printf("[buddy][mqtt] snapshot applied state=%u sessions=%u focus=%s\n",
                static_cast<unsigned>(snapshot.petState), static_cast<unsigned>(snapshot.sessionCount),
                snapshot.focusTitle.c_str());
  return true;
}

void MqttClient::onMessageThunk(char* topic, uint8_t* payload, unsigned int length) {
  if (activeInstance_ != nullptr) {
    activeInstance_->onMessage(topic, payload, length);
  }
}

void MqttClient::onMessage(char* topic, uint8_t* payload, unsigned int length) {
  String payloadText;
  payloadText.reserve(length + 1U);
  for (unsigned int i = 0; i < length; ++i) {
    payloadText += static_cast<char>(payload[i]);
  }

  Serial.printf("[buddy][mqtt] message topic=%s bytes=%u\n", topic, length);
  Snapshot parsedSnapshot = pendingSnapshot_;
  if (!parseSnapshotMessage(payloadText, parsedSnapshot)) {
    String preview = payloadText.substring(0, 120);
    Serial.printf("[buddy][mqtt] snapshot parse failed payload=%s\n", preview.c_str());
    return;
  }

  pendingSnapshot_ = parsedSnapshot;
  receivedSnapshot_ = true;
  snapshotDirty_ = true;
  Serial.println("[buddy][mqtt] snapshot parsed successfully");
}

bool MqttClient::parseBrokerUrl(const String& brokerUrl, BrokerConfig& config) const {
  int schemeEnd = brokerUrl.indexOf("://");
  if (schemeEnd <= 0) {
    return false;
  }

  const String scheme = brokerUrl.substring(0, schemeEnd);
  config.tls = scheme.equalsIgnoreCase("mqtts") || scheme.equalsIgnoreCase("ssl");

  String hostPort = brokerUrl.substring(schemeEnd + 3);
  const int pathPos = hostPort.indexOf('/');
  if (pathPos >= 0) {
    hostPort = hostPort.substring(0, pathPos);
  }

  const int colonPos = hostPort.lastIndexOf(':');
  if (colonPos > 0) {
    config.host = hostPort.substring(0, colonPos);
    config.port = static_cast<uint16_t>(hostPort.substring(colonPos + 1).toInt());
  } else {
    config.host = hostPort;
    config.port = config.tls ? 8883 : 1883;
  }

  return !config.host.isEmpty() && config.port != 0;
}

bool MqttClient::ensureConnected(NetworkState& network) {
  if (client_.connected()) {
    if (!subscribed_) {
      Serial.printf("[buddy][mqtt] subscribing topic=%s\n", config_.topic.c_str());
      subscribed_ = client_.subscribe(config_.topic.c_str(), 0);
      network.mqttStatus = subscribed_ ? "Subscribed" : "Sub failed";
      if (subscribed_) {
        Serial.println("[buddy][mqtt] subscribe success");
      } else {
        Serial.printf("[buddy][mqtt] subscribe failed state=%d\n", client_.state());
      }
    }
    return subscribed_;
  }

  if (millis() - lastConnectAttemptAt_ < kReconnectIntervalMs) {
    network.mqttStatus = "Retrying";
    return false;
  }

  lastConnectAttemptAt_ = millis();
  subscribed_ = false;
  network.mqttStatus = "Connecting";
  Serial.printf("[buddy][mqtt] connecting broker=%s:%u client=%s topic=%s user_present=%s token_len=%u\n",
                config_.host.c_str(), static_cast<unsigned>(config_.port), config_.clientId.c_str(),
                config_.topic.c_str(), config_.username.isEmpty() ? "no" : "yes",
                static_cast<unsigned>(config_.token.length()));
  const bool connected = client_.connect(config_.clientId.c_str(), config_.username.c_str(), config_.token.c_str());
  if (!connected) {
    network.mqttStatus = "Connect failed";
    Serial.printf("[buddy][mqtt] connect failed state=%d host=%s port=%u\n", client_.state(), config_.host.c_str(),
                  static_cast<unsigned>(config_.port));
    return false;
  }

  Serial.println("[buddy][mqtt] connect success");
  network.mqttStatus = "Connected";
  return true;
}

bool MqttClient::parseSnapshotMessage(const String& payload, Snapshot& snapshot) const {
  JsonDocument doc;
  const auto error = deserializeJson(doc, payload);
  if (error != DeserializationError::Ok) {
    Serial.printf("[buddy][mqtt] json parse error=%s\n", error.c_str());
    return false;
  }

  snapshot.version = doc["version"] | snapshot.version;
  snapshot.updatedAt = millis() / 1000U;
  snapshot.petState = parsePetState(doc["pet_state"] | static_cast<uint8_t>(snapshot.petState));
  snapshot.focusTitle = doc["focus_title"] | snapshot.focusTitle;
  snapshot.requiresUser = doc["requires_user"] | false;
  snapshot.totalTokens = doc["total_tokens"] | snapshot.totalTokens;
  snapshot.todayTokens = doc["today_tokens"] | snapshot.todayTokens;
  snapshot.sessionCount = 0;
  snapshot.sessionStates = {SessionState::Thinking, SessionState::Thinking, SessionState::Thinking,
                            SessionState::Thinking};

  JsonArray sessionStates = doc["session_states"].as<JsonArray>();
  uint8_t index = 0;
  for (JsonVariant value : sessionStates) {
    if (index >= kSessionLightCount) {
      break;
    }
    snapshot.sessionStates[index] = parseSessionState(value.as<uint8_t>());
    ++index;
  }
  snapshot.sessionCount = index;
  return true;
}

SessionState MqttClient::parseSessionState(uint8_t raw) const {
  switch (raw) {
    case 0:
      return SessionState::Thinking;
    case 1:
      return SessionState::Busy;
    case 2:
      return SessionState::Waiting;
    case 3:
      return SessionState::Error;
    case 4:
      return SessionState::Done;
    default:
      return SessionState::Thinking;
  }
}

PetState MqttClient::parsePetState(uint8_t raw) const {
  switch (raw) {
    case 0:
      return PetState::Idle;
    case 1:
      return PetState::Thinking;
    case 2:
      return PetState::Busy;
    case 3:
      return PetState::Waiting;
    case 4:
      return PetState::Error;
    case 5:
      return PetState::Done;
    case 6:
      return PetState::Sleep;
    case 7:
      return PetState::Offline;
    default:
      return PetState::Idle;
  }
}

}  // namespace buddy
