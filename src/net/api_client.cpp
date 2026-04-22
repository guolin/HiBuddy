#include "net/api_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace buddy {

void ApiClient::begin(const String& baseUrl) {
  if (!baseUrl.isEmpty()) {
    baseUrl_ = baseUrl;
  }
  if (baseUrl_.indexOf("example.supabase.co") >= 0) {
    Serial.printf("[buddy][api] warning: base URL is still placeholder: %s\n", baseUrl_.c_str());
  } else {
    Serial.printf("[buddy][api] base URL: %s\n", baseUrl_.c_str());
  }
}

PairingState ApiClient::requestPairCode(DeviceSettings& settings) const {
  PairingState pairing;
  pairing.statusLabel = "Requesting";
  pairing.errorDetail = "";
  Serial.printf("[buddy][api] requesting pair code device_id=%s token_present=%s\n", settings.deviceId.c_str(),
                settings.deviceToken.isEmpty() ? "no" : "yes");

  JsonDocument requestDoc;
  requestDoc["device_id"] = settings.deviceId;
  if (!settings.deviceToken.isEmpty()) {
    requestDoc["device_token"] = settings.deviceToken;
  }

  String requestBody;
  serializeJson(requestDoc, requestBody);

  int statusCode = 0;
  String responseBody;
  if (!postJson("/device-pair-code", requestBody, statusCode, responseBody)) {
    pairing.statusLabel = "Request failed";
    pairing.errorDetail = "transport";
    Serial.printf("[buddy][api] pair-code transport failed url=%s/device-pair-code\n", baseUrl_.c_str());
    return pairing;
  }

  Serial.printf("[buddy][api] pair-code http=%d body=%s\n", statusCode, responseBody.c_str());

  JsonDocument responseDoc;
  const auto parseError = deserializeJson(responseDoc, responseBody);
  if (parseError != DeserializationError::Ok || statusCode < 200 || statusCode >= 300 ||
      !responseDoc["ok"].as<bool>()) {
    Serial.printf("[buddy][api] pair-code request rejected http=%d body=%s\n", statusCode, responseBody.c_str());
    pairing.statusLabel = "Request failed";
    if (parseError != DeserializationError::Ok) {
      pairing.errorDetail = "json parse";
    } else if (statusCode >= 400) {
      pairing.errorDetail = "http " + String(statusCode);
    } else {
      pairing.errorDetail = "bad response";
    }
    return pairing;
  }

  settings.deviceToken = responseDoc["deviceToken"] | settings.deviceToken;
  pairing.pairCode = responseDoc["pairCode"] | "------";
  pairing.pairUrl = responseDoc["pairUrl"] | "";
  pairing.statusLabel = "Pending";
  pairing.errorDetail = "";
  pairing.paired = false;
  return pairing;
}

BootstrapPayload ApiClient::bootstrap(const DeviceSettings& settings) const {
  BootstrapPayload payload;

  JsonDocument requestDoc;
  requestDoc["device_id"] = settings.deviceId;
  requestDoc["device_token"] = settings.deviceToken;

  String requestBody;
  serializeJson(requestDoc, requestBody);

  int statusCode = 0;
  String responseBody;
  if (!postJson("/device-bootstrap", requestBody, statusCode, responseBody)) {
    payload.error = "bootstrap transport failed";
    Serial.printf("[buddy][api] bootstrap transport failed url=%s/device-bootstrap\n", baseUrl_.c_str());
    return payload;
  }

  Serial.printf("[buddy][api] bootstrap http=%d body=%s\n", statusCode, responseBody.c_str());

  JsonDocument responseDoc;
  if (deserializeJson(responseDoc, responseBody) != DeserializationError::Ok || statusCode < 200 ||
      statusCode >= 300 || !responseDoc["ok"].as<bool>()) {
    Serial.printf("[buddy][api] bootstrap request rejected http=%d body=%s\n", statusCode, responseBody.c_str());
    payload.error = "bootstrap parse failed";
    return payload;
  }

  payload.ok = true;
  payload.paired = responseDoc["paired"] | false;
  payload.mqttBrokerUrl = responseDoc["mqtt"]["mqttBrokerUrl"] | "";
  payload.mqttUsername = responseDoc["mqtt"]["mqttUsername"] | "";
  payload.mqttToken = responseDoc["mqtt"]["mqttToken"] | "";
  payload.mqttClientId = responseDoc["mqtt"]["mqttClientId"] | "";
  payload.mqttTopic = responseDoc["mqtt"]["mqttTopic"] | "";
  payload.petName = responseDoc["config"]["petName"] | kDefaultPetName;
  payload.petPackId = responseDoc["activePet"]["petPackId"] | "demo-pack";
  payload.assetVersion = responseDoc["activePet"]["assetVersion"] | kDefaultAssetVersion;
  return payload;
}

bool ApiClient::postJson(const String& path, const String& body, int& statusCode, String& responseBody) const {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);

  HTTPClient http;
  const String url = baseUrl_ + path;
  if (!http.begin(client, url)) {
    Serial.printf("[buddy][api] http.begin failed url=%s\n", url.c_str());
    return false;
  }

  http.setTimeout(10000);
  http.setReuse(false);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  Serial.printf("[buddy][api] POST %s body=%s free_heap=%u\n", url.c_str(), body.c_str(),
                static_cast<unsigned>(ESP.getFreeHeap()));
  statusCode = http.POST(body);
  responseBody = http.getString();
  if (statusCode <= 0) {
    Serial.printf("[buddy][api] POST failed url=%s err=%s\n", url.c_str(), http.errorToString(statusCode).c_str());
  }
  http.end();
  return statusCode > 0;
}

}  // namespace buddy
