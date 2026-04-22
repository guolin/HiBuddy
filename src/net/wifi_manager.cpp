#include "net/wifi_manager.h"

#include <WiFi.h>

#include "app_config.h"

namespace buddy {

void WifiManager::begin(const DeviceSettings& settings) {
  WiFi.setAutoReconnect(false);
  if (!settings.wifiSsid.isEmpty()) {
    WiFi.mode(WIFI_STA);
    Serial.printf("[buddy][wifi] boot reconnect ssid=%s\n", settings.wifiSsid.c_str());
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    connectStartedAt_ = millis();
  }
}

void WifiManager::tick(NetworkState& network, DeviceSettings& settings) {
  if (portalServerStarted_) {
    portalServer_.handleClient();
  }

  network.portalHasCredentials = !pendingSsid_.isEmpty();
  logStatusChange(WiFi.status());

  if (network.portalActive && network.portalHasCredentials && !stationConnectRequested_) {
    beginStationConnect(network);
  }

  if (stationConnectRequested_ && WiFi.status() != WL_CONNECTED &&
      millis() - connectStartedAt_ > kPortalRetryIntervalMs) {
    stationConnectRequested_ = false;
    network.portalStatus = String("Connect failed: ") + statusLabel(WiFi.status());
    Serial.printf("[buddy][wifi] connect timeout ssid=%s status=%s\n", pendingSsid_.c_str(),
                  statusLabel(WiFi.status()));
  }

  network.wifiConnected = WiFi.status() == WL_CONNECTED;
  network.wifiRssi = network.wifiConnected ? WiFi.RSSI() : 0;
  network.wifiConfigured = !settings.wifiSsid.isEmpty() || network.portalHasCredentials;

  if (network.wifiConnected && network.portalHasCredentials && !credentialsApplied_) {
    settings.wifiSsid = pendingSsid_;
    settings.wifiPassword = pendingPassword_;
    network.portalStatus = "Connected. Finishing setup";
    Serial.printf("[buddy][wifi] connected ssid=%s ip=%s rssi=%ld\n", settings.wifiSsid.c_str(),
                  WiFi.localIP().toString().c_str(), static_cast<long>(WiFi.RSSI()));
    credentialsApplied_ = true;
  }
}

void WifiManager::startPortal(NetworkState& network) {
  WiFi.mode(WIFI_AP_STA);
  const String suffix = String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX).substring(6);
  network.portalSsid = "Buddy-" + suffix;
  WiFi.softAP(network.portalSsid.c_str(), kDefaultPortalPassphrase);
  network.portalActive = true;
  network.portalHasCredentials = false;
  network.portalStatus = "Connect to AP and open 192.168.4.1";
  pendingSsid_ = "";
  pendingPassword_ = "";
  stationConnectRequested_ = false;
  credentialsApplied_ = false;
  lastLoggedStatus_ = WL_IDLE_STATUS;
  Serial.printf("[buddy][wifi] portal started ap=%s ip=%s open=true\n", network.portalSsid.c_str(),
                WiFi.softAPIP().toString().c_str());
  ensurePortalServer();
}

void WifiManager::stopPortal(NetworkState& network) {
  WiFi.softAPdisconnect(true);
  if (portalServerStarted_) {
    portalServer_.stop();
  }
  network.portalActive = false;
  portalServerStarted_ = false;
  stationConnectRequested_ = false;
  Serial.println("[buddy][wifi] portal stopped");
}

void WifiManager::reconnect(const DeviceSettings& settings, NetworkState& network) {
  if (settings.wifiSsid.isEmpty()) {
    network.wifiConnected = false;
    return;
  }

  WiFi.mode(WIFI_STA);
  Serial.printf("[buddy][wifi] manual reconnect ssid=%s\n", settings.wifiSsid.c_str());
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
  connectStartedAt_ = millis();
}

void WifiManager::ensurePortalServer() {
  if (portalServerStarted_) {
    return;
  }

  portalServer_.on("/", HTTP_GET, [this]() {
    String html;
    html.reserve(1200);
    html += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Buddy Wi-Fi Setup</title>";
    html += "<style>body{font-family:Arial,sans-serif;background:#111;color:#fff;padding:24px;}input{width:100%;padding:12px;margin:8px 0;border-radius:8px;border:1px solid #555;background:#222;color:#fff;}button{width:100%;padding:12px;border:0;border-radius:8px;background:#ffcc33;color:#000;font-weight:bold;}small{color:#bbb;}</style>";
    html += "</head><body><h1>Buddy Wi-Fi Setup</h1><p>SSID and password will be saved to the device.</p>";
    html += "<form method='POST' action='/save'><label>Wi-Fi SSID</label><input name='ssid' maxlength='64' required>";
    html += "<label>Password</label><input name='password' maxlength='64' type='password' placeholder='Leave empty for open Wi-Fi'>";
    html += "<button type='submit'>Save and Connect</button></form><p><small>After saving, keep this page open for a few seconds.</small></p></body></html>";
    portalServer_.send(200, "text/html", html);
  });

  portalServer_.on("/save", HTTP_POST, [this]() {
    pendingSsid_ = portalServer_.arg("ssid");
    pendingPassword_ = portalServer_.arg("password");
    stationConnectRequested_ = false;
    credentialsApplied_ = false;
    Serial.printf("[buddy][wifi] credentials received ssid=%s password_len=%u\n", pendingSsid_.c_str(),
                  static_cast<unsigned>(pendingPassword_.length()));

    String html;
    html.reserve(600);
    html += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Buddy Connecting</title></head><body style='font-family:Arial,sans-serif;background:#111;color:#fff;padding:24px;'>";
    html += "<h1>Saved</h1><p>Buddy is connecting to <strong>";
    html += pendingSsid_;
    html += "</strong>.</p><p>You can return to the device screen now.</p></body></html>";
    portalServer_.send(200, "text/html", html);
    portalServer_.client().stop();
  });

  portalServer_.onNotFound([this]() {
    portalServer_.sendHeader("Location", "/", true);
    portalServer_.send(302, "text/plain", "");
  });
  portalServer_.begin();
  portalServerStarted_ = true;
}

void WifiManager::beginStationConnect(NetworkState& network) {
  if (pendingSsid_.isEmpty()) {
    return;
  }

  if (network.portalActive) {
    WiFi.softAPdisconnect(true);
    network.portalActive = false;
    portalServerStarted_ = false;
    Serial.println("[buddy][wifi] disabled portal AP before STA connect");
  }

  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(pendingSsid_.c_str(), pendingPassword_.c_str());
  connectStartedAt_ = millis();
  stationConnectRequested_ = true;
  network.portalHasCredentials = true;
  network.portalStatus = "Saved. Connecting...";
  Serial.printf("[buddy][wifi] begin connect ssid=%s\n", pendingSsid_.c_str());
}

const char* WifiManager::statusLabel(wl_status_t status) const {
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

void WifiManager::logStatusChange(wl_status_t status) const {
  if (status == lastLoggedStatus_) {
    return;
  }
  const_cast<WifiManager*>(this)->lastLoggedStatus_ = status;
  Serial.printf("[buddy][wifi] status=%s\n", statusLabel(status));
}

}  // namespace buddy
