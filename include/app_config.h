#pragma once

#include <Arduino.h>

#ifndef BUDDY_API_BASE_URL
#define BUDDY_API_BASE_URL "https://hgktrupgorgseylgekzw.supabase.co/functions/v1"
#endif

namespace buddy {

constexpr const char* kFirmwareVersion = "0.1.0";
constexpr const char* kApiBaseUrl = BUDDY_API_BASE_URL;
inline String defaultDeviceId() {
  const uint64_t mac = ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL;
  char buffer[13];
  snprintf(buffer, sizeof(buffer), "%012llX", mac);
  return String(buffer);
}
constexpr const char* kDefaultPetName = "Buddy";
constexpr const char* kDefaultAssetVersion = "demo-1.0.0";
constexpr const char* kDefaultManifestPath = "/pet-pack/demo/manifest.json";
constexpr const char* kDefaultPortalHost = "192.168.4.1";
constexpr const char* kDefaultPortalPassphrase = "";
constexpr const char* kEmqxRootCa = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)PEM";
constexpr uint32_t kLongPressMs = 700;
constexpr uint32_t kReminderDurationMs = 4500;
constexpr uint32_t kScreenSleepMs = 90000;
constexpr uint32_t kStaleSnapshotMs = 45000;
constexpr uint32_t kPortalRetryIntervalMs = 12000;
constexpr uint32_t kPairBootstrapPollMs = 5000;
constexpr uint32_t kPairCodeLifetimeMs = 5 * 60 * 1000;
constexpr uint32_t kPairCodeRefreshLeadMs = 10 * 1000;
constexpr uint16_t kPetSpriteSize = 32;
constexpr uint16_t kPetScale = 4;
constexpr uint16_t kSessionLightCount = 4;

}  // namespace buddy
