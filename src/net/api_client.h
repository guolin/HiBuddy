#pragma once

#include "app_types.h"

namespace buddy {

struct BootstrapPayload {
  bool paired{false};
  bool ok{false};
  String mqttTopic;
  String mqttBrokerUrl;
  String mqttUsername;
  String mqttToken;
  String mqttClientId;
  String petPackId{"demo-pack"};
  String petName{kDefaultPetName};
  String assetVersion{kDefaultAssetVersion};
  String error;
};

class ApiClient {
 public:
  void begin(const String& baseUrl);
  PairingState requestPairCode(DeviceSettings& settings) const;
  BootstrapPayload bootstrap(const DeviceSettings& settings) const;

 private:
  String baseUrl_{"https://example.supabase.co/functions/v1"};

  bool postJson(const String& path, const String& body, int& statusCode, String& responseBody) const;
};

}  // namespace buddy
