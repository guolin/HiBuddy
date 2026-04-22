#pragma once

#include "app_types.h"
#include "pet/pet_runtime.h"
#include "ui/status_lights.h"

namespace buddy {

class UiRouter {
 public:
  void begin();
  void handleButton(const ButtonEvent& event, AppModel& model);
  void tick(AppModel& model);
  void draw(const AppModel& model, const PetRuntime& petRuntime) const;

 private:
  StatusLights statusLights_;

  void cycleBaseScreen(AppModel& model, bool forward) const;
  void drawBase(const AppModel& model, const PetRuntime& petRuntime) const;
  void drawPet(const AppModel& model, const PetRuntime& petRuntime) const;
  void drawOverview(const AppModel& model, const PetRuntime& petRuntime) const;
  void drawInfo(const AppModel& model) const;
  void drawGuide(const AppModel& model) const;
  void drawWifiGuide(const AppModel& model) const;
  void drawPairGuide(const AppModel& model) const;
  void drawMenuOverlay(const AppModel& model) const;
  void drawBrightnessOverlay(const AppModel& model) const;
  void drawVolumeOverlay(const AppModel& model) const;
};

}  // namespace buddy
