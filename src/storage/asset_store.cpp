#include "storage/asset_store.h"

#include <Arduino.h>

namespace buddy {

namespace {
constexpr const char* kDemoManifest = R"json({
  "id": "demo-pack",
  "name": "Buddy Demo",
  "version": "demo-1.0.0",
  "frame_width": 16,
  "frame_height": 16,
  "frame_count": 10,
  "display_scale": 5,
  "palette_size": 6,
  "default_fps": 8,
  "states": {
    "idle": [0, 1],
    "thinking": [0, 2],
    "busy": [3, 4],
    "waiting": [5, 2],
    "done": [6, 1],
    "error": [7, 8],
    "sleep": [9],
    "offline": [9, 7]
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
