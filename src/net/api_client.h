#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

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
  bool consumePairCodeResult(DeviceSettings& settings, PairingState& pairing) const;
  bool queueBootstrap(const DeviceSettings& settings) const;
  bool consumeBootstrapResult(BootstrapPayload& payload) const;
  BootstrapPayload bootstrap(const DeviceSettings& settings) const;

 private:
  struct PairCodeAsyncState {
    SemaphoreHandle_t mutex{nullptr};
    TaskHandle_t worker{nullptr};
    bool inFlight{false};
    bool ready{false};
    DeviceSettings requestSettings;
    DeviceSettings resultSettings;
    PairingState resultPairing;
  };

  struct BootstrapAsyncState {
    SemaphoreHandle_t mutex{nullptr};
    TaskHandle_t worker{nullptr};
    bool inFlight{false};
    bool ready{false};
    DeviceSettings requestSettings;
    BootstrapPayload resultPayload;
  };

  String baseUrl_{"https://hgktrupgorgseylgekzw.supabase.co/functions/v1"};

  bool postJson(const String& path, const String& body, int& statusCode, String& responseBody) const;
  bool postJsonOnce(const String& path, const String& body, int& statusCode, String& responseBody,
                    bool useHttp10) const;
  PairCodeAsyncState& pairCodeState() const;
  BootstrapAsyncState& bootstrapState() const;
  static void pairCodeWorkerEntry(void* arg);
  static void bootstrapWorkerEntry(void* arg);
};

}  // namespace buddy
