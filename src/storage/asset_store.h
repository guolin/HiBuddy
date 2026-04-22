#pragma once

#include <LittleFS.h>

#include "app_types.h"

namespace buddy {

class AssetStore {
 public:
  bool begin();
  bool ensureDemoPack();
  String activeManifestPath() const;
  String activeAssetVersion() const;

 private:
  String activeManifestPath_{kDefaultManifestPath};
  String activeAssetVersion_{kDefaultAssetVersion};
};

}  // namespace buddy
