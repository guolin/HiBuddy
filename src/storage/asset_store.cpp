#include "storage/asset_store.h"

#include <Arduino.h>

namespace buddy {

namespace {
constexpr const char* kDemoManifest = R"json({
  "id": "demo-pack",
  "name": "Buddy Demo",
  "version": "demo-1.0.0",
  "frame_width": 32,
  "frame_height": 32,
  "frame_count": 26,
  "display_scale": 4,
  "palette_size": 9,
  "default_fps": 8,
  "states": {
    "sleep": [0, 1, 2],
    "idle": [3, 4, 5, 6, 7],
    "thinking": [8, 9, 10, 11],
    "busy": [8, 9, 10, 11],
    "waiting": [12, 13, 14, 15],
    "done": [16, 17, 18, 19],
    "error": [20, 21, 22],
    "offline": [20, 21, 22],
    "heart": [23, 24, 25]
  }
})json";
}

bool AssetStore::begin() {
  return LittleFS.begin(true);
}

bool AssetStore::ensureDemoPack() {
  if (LittleFS.exists(kDefaultManifestPath)) {
    return true;
  }

  LittleFS.mkdir("/pet-pack");
  LittleFS.mkdir("/pet-pack/demo");

  File manifest = LittleFS.open(kDefaultManifestPath, FILE_WRITE);
  if (!manifest) {
    return false;
  }

  manifest.print(kDemoManifest);
  manifest.close();
  return true;
}

String AssetStore::activeManifestPath() const {
  return activeManifestPath_;
}

String AssetStore::activeAssetVersion() const {
  return activeAssetVersion_;
}

}  // namespace buddy
