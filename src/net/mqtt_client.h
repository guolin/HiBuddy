#pragma once

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "app_types.h"

namespace buddy {

class MqttClient {
 public:
  void begin();
  void configure(const NetworkState& network);
  void disconnect();
  bool tick(NetworkState& network, Snapshot& snapshot);

 private:
  struct BrokerConfig {
    String brokerUrl;
    String username;
    String token;
    String clientId;
    String topic;
    String host;
    uint16_t port{8883};
    bool tls{true};
  };

  static MqttClient* activeInstance_;

  WiFiClientSecure secureClient_;
  PubSubClient client_;
  BrokerConfig config_;
  Snapshot pendingSnapshot_;
  uint32_t lastConnectAttemptAt_{0};
  bool subscribed_{false};
  bool receivedSnapshot_{false};
  bool snapshotDirty_{false};

  static void onMessageThunk(char* topic, uint8_t* payload, unsigned int length);
  void onMessage(char* topic, uint8_t* payload, unsigned int length);
  bool parseBrokerUrl(const String& brokerUrl, BrokerConfig& config) const;
  bool ensureConnected(NetworkState& network);
  bool parseSnapshotMessage(const String& payload, Snapshot& snapshot) const;
  SessionState parseSessionState(uint8_t raw) const;
  PetState parsePetState(uint8_t raw) const;
};

}  // namespace buddy
