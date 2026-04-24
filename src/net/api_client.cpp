#include "net/api_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace buddy {

namespace {

const char* wifiStatusLabel(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID";
    case WL_SCAN_COMPLETED:
      return "SCAN_DONE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

void logWifiSnapshot(const char* stage, const String& url) {
  const wl_status_t status = WiFi.status();
  const bool connected = status == WL_CONNECTED;
  const String ip = connected ? WiFi.localIP().toString() : "-";
  const String ssid = connected ? WiFi.SSID() : "";
  const long rssi = connected ? WiFi.RSSI() : 0;
  Serial.printf("[buddy][api] net %s url=%s wifi_status=%s connected=%d ip=%s ssid=%s rssi=%ld free_heap=%u\n",
                stage, url.c_str(), wifiStatusLabel(status), static_cast<int>(connected), ip.c_str(), ssid.c_str(),
                rssi, static_cast<unsigned>(ESP.getFreeHeap()));
}

}  // namespace

void ApiClient::begin(const String& baseUrl) {
  if (!baseUrl.isEmpty()) {
    baseUrl_ = baseUrl;
  }
  if (baseUrl_.indexOf("hgktrupgorgseylgekzw.supabase.co") >= 0) {
    Serial.printf("[buddy][api] warning: base URL is still placeholder: %s\n", baseUrl_.c_str());
  } else {
    Serial.printf("[buddy][api] base URL: %s\n", baseUrl_.c_str());
  }
}

ApiClient::PairCodeAsyncState& ApiClient::pairCodeState() const {
  static PairCodeAsyncState state;
  if (state.mutex == nullptr) {
    state.mutex = xSemaphoreCreateMutex();
  }
  return state;
}

ApiClient::BootstrapAsyncState& ApiClient::bootstrapState() const {
  static BootstrapAsyncState state;
  if (state.mutex == nullptr) {
    state.mutex = xSemaphoreCreateMutex();
  }
  return state;
}

PairingState ApiClient::requestPairCode(DeviceSettings& settings) const {
  PairCodeAsyncState& state = pairCodeState();
  PairingState pairing;
  pairing.statusLabel = "Requesting";
  pairing.errorDetail = "";

  if (state.mutex == nullptr) {
    pairing.statusLabel = "Request failed";
    pairing.errorDetail = "mutex";
    return pairing;
  }

  xSemaphoreTake(state.mutex, portMAX_DELAY);
  if (state.inFlight) {
    pairing.statusLabel = "Requesting";
    pairing.errorDetail = "";
    xSemaphoreGive(state.mutex);
    return pairing;
  }

  state.requestSettings = settings;
  state.resultSettings = settings;
  state.resultPairing = pairing;
  state.ready = false;
  state.inFlight = true;
  BaseType_t taskOk = xTaskCreatePinnedToCore(&ApiClient::pairCodeWorkerEntry, "buddy-pair-code", 12288,
                                              const_cast<ApiClient*>(this), 1, &state.worker, 0);
  xSemaphoreGive(state.mutex);

  if (taskOk != pdPASS) {
    xSemaphoreTake(state.mutex, portMAX_DELAY);
    state.inFlight = false;
    state.worker = nullptr;
    xSemaphoreGive(state.mutex);
    pairing.statusLabel = "Request failed";
    pairing.errorDetail = "task create";
    Serial.println("[buddy][api] failed to start async pair-code task");
    return pairing;
  }

  Serial.printf("[buddy][api] queued pair code request device_id=%s token_present=%s\n", settings.deviceId.c_str(),
                settings.deviceToken.isEmpty() ? "no" : "yes");
  return pairing;
}

bool ApiClient::consumePairCodeResult(DeviceSettings& settings, PairingState& pairing) const {
  PairCodeAsyncState& state = pairCodeState();
  if (state.mutex == nullptr) {
    return false;
  }

  xSemaphoreTake(state.mutex, portMAX_DELAY);
  const bool ready = state.ready;
  if (ready) {
    settings = state.resultSettings;
    pairing = state.resultPairing;
    state.ready = false;
  }
  xSemaphoreGive(state.mutex);
  return ready;
}

bool ApiClient::queueBootstrap(const DeviceSettings& settings) const {
  BootstrapAsyncState& state = bootstrapState();
  if (state.mutex == nullptr) {
    Serial.println("[buddy][api] bootstrap queue unavailable: mutex");
    return false;
  }

  xSemaphoreTake(state.mutex, portMAX_DELAY);
  if (state.inFlight) {
    xSemaphoreGive(state.mutex);
    return true;
  }

  state.requestSettings = settings;
  state.resultPayload = BootstrapPayload{};
  state.ready = false;
  state.inFlight = true;
  BaseType_t taskOk = xTaskCreatePinnedToCore(&ApiClient::bootstrapWorkerEntry, "buddy-bootstrap", 12288,
                                              const_cast<ApiClient*>(this), 1, &state.worker, 0);
  xSemaphoreGive(state.mutex);

  if (taskOk != pdPASS) {
    xSemaphoreTake(state.mutex, portMAX_DELAY);
    state.inFlight = false;
    state.worker = nullptr;
    xSemaphoreGive(state.mutex);
    Serial.println("[buddy][api] failed to start async bootstrap task");
    return false;
  }

  Serial.printf("[buddy][api] queued bootstrap device_id=%s token_present=%s\n", settings.deviceId.c_str(),
                settings.deviceToken.isEmpty() ? "no" : "yes");
  return true;
}

bool ApiClient::consumeBootstrapResult(BootstrapPayload& payload) const {
  BootstrapAsyncState& state = bootstrapState();
  if (state.mutex == nullptr) {
    return false;
  }

  xSemaphoreTake(state.mutex, portMAX_DELAY);
  const bool ready = state.ready;
  if (ready) {
    payload = state.resultPayload;
    state.ready = false;
  }
  xSemaphoreGive(state.mutex);
  return ready;
}

void ApiClient::pairCodeWorkerEntry(void* arg) {
  auto* client = static_cast<ApiClient*>(arg);
  PairCodeAsyncState& state = client->pairCodeState();

  DeviceSettings settings;
  xSemaphoreTake(state.mutex, portMAX_DELAY);
  settings = state.requestSettings;
  xSemaphoreGive(state.mutex);

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
  if (!client->postJson("/device-pair-code", requestBody, statusCode, responseBody)) {
    pairing.statusLabel = "Request failed";
    pairing.errorDetail = "transport";
    Serial.printf("[buddy][api] pair-code transport failed url=%s/device-pair-code\n", client->baseUrl_.c_str());
  } else {
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
    } else {
      settings.deviceToken = responseDoc["deviceToken"] | settings.deviceToken;
      pairing.pairCode = responseDoc["pairCode"] | "------";
      pairing.pairUrl = responseDoc["pairUrl"] | "";
      pairing.statusLabel = "Pending";
      pairing.errorDetail = "";
      pairing.paired = false;
    }
  }

  xSemaphoreTake(state.mutex, portMAX_DELAY);
  state.resultSettings = settings;
  state.resultPairing = pairing;
  state.ready = true;
  state.inFlight = false;
  state.worker = nullptr;
  xSemaphoreGive(state.mutex);
  vTaskDelete(nullptr);
}

void ApiClient::bootstrapWorkerEntry(void* arg) {
  auto* client = static_cast<ApiClient*>(arg);
  BootstrapAsyncState& state = client->bootstrapState();

  DeviceSettings settings;
  xSemaphoreTake(state.mutex, portMAX_DELAY);
  settings = state.requestSettings;
  xSemaphoreGive(state.mutex);

  BootstrapPayload payload = client->bootstrap(settings);

  xSemaphoreTake(state.mutex, portMAX_DELAY);
  state.resultPayload = payload;
  state.ready = true;
  state.inFlight = false;
  state.worker = nullptr;
  xSemaphoreGive(state.mutex);
  vTaskDelete(nullptr);
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
  if (postJsonOnce(path, body, statusCode, responseBody, false)) {
    return true;
  }

  const wl_status_t wifiStatus = WiFi.status();
  if (statusCode > 0 || wifiStatus != WL_CONNECTED) {
    return false;
  }

  Serial.printf("[buddy][api] retrying %s with HTTP/1.0 fallback after transport error\n", path.c_str());
  delay(150);
  return postJsonOnce(path, body, statusCode, responseBody, true);
}

bool ApiClient::postJsonOnce(const String& path, const String& body, int& statusCode, String& responseBody,
                             bool useHttp10) const {
  constexpr uint16_t kRequestTimeoutMs = 20000;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(kRequestTimeoutMs);

  HTTPClient http;
  const String url = baseUrl_ + path;
  logWifiSnapshot(useHttp10 ? "before_begin_http10" : "before_begin", url);
  if (!http.begin(client, url)) {
    Serial.printf("[buddy][api] http.begin failed url=%s use_http10=%d\n", url.c_str(), static_cast<int>(useHttp10));
    logWifiSnapshot(useHttp10 ? "begin_failed_http10" : "begin_failed", url);
    return false;
  }

  if (useHttp10) {
    http.useHTTP10(true);
  }
  http.setTimeout(kRequestTimeoutMs);
  http.setReuse(false);
  http.addHeader("Connection", "close");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  const uint32_t postStartedAt = millis();
  Serial.printf("[buddy][api] POST %s body=%s timeout_ms=%u use_http10=%d free_heap=%u\n", url.c_str(), body.c_str(),
                kRequestTimeoutMs, static_cast<int>(useHttp10), static_cast<unsigned>(ESP.getFreeHeap()));
  statusCode = http.POST(body);
  const uint32_t postElapsedMs = millis() - postStartedAt;

  if (statusCode <= 0) {
    responseBody = "";
    Serial.printf("[buddy][api] POST failed url=%s err=%s post_elapsed_ms=%lu use_http10=%d\n", url.c_str(),
                  http.errorToString(statusCode).c_str(), static_cast<unsigned long>(postElapsedMs),
                  static_cast<int>(useHttp10));
    logWifiSnapshot(useHttp10 ? "post_failed_http10" : "post_failed", url);
    http.end();
    return false;
  }

  const uint32_t readStartedAt = millis();
  responseBody = http.getString();
  const uint32_t readElapsedMs = millis() - readStartedAt;
  Serial.printf(
      "[buddy][api] POST ok url=%s http=%d post_elapsed_ms=%lu read_elapsed_ms=%lu body_len=%u use_http10=%d\n",
      url.c_str(), statusCode, static_cast<unsigned long>(postElapsedMs), static_cast<unsigned long>(readElapsedMs),
      static_cast<unsigned>(responseBody.length()), static_cast<int>(useHttp10));
  logWifiSnapshot(useHttp10 ? "post_ok_http10" : "post_ok", url);
  http.end();
  return true;
}

}  // namespace buddy
