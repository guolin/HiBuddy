#pragma once

#include <WebServer.h>

#include "app_types.h"

namespace buddy {

class WifiManager {
 public:
  void begin(const DeviceSettings& settings);
  void tick(NetworkState& network, DeviceSettings& settings);
  void startPortal(NetworkState& network);
  void stopPortal(NetworkState& network);
  void reconnect(const DeviceSettings& settings, NetworkState& network);

 private:
  WebServer portalServer_{80};
  uint32_t connectStartedAt_{0};
  bool portalServerStarted_{false};
  bool stationConnectRequested_{false};
  bool credentialsApplied_{false};
  String pendingSsid_;
  String pendingPassword_;
  wl_status_t lastLoggedStatus_{WL_IDLE_STATUS};

  void ensurePortalServer();
  void beginStationConnect(NetworkState& network);
  const char* statusLabel(wl_status_t status) const;
  void logStatusChange(wl_status_t status) const;
};

}  // namespace buddy
