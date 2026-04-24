#include "app_types.h"

namespace buddy {

const __FlashStringHelper* toFlashString(SystemState state) {
  switch (state) {
    case SystemState::Boot:
      return F("BOOT");
    case SystemState::NoWifi:
      return F("NO_WIFI");
    case SystemState::WifiPortal:
      return F("WIFI_PORTAL");
    case SystemState::ConnectingWifi:
      return F("CONNECTING_WIFI");
    case SystemState::NoBinding:
      return F("NO_BINDING");
    case SystemState::Pairing:
      return F("PAIRING");
    case SystemState::Syncing:
      return F("SYNCING");
    case SystemState::Running:
      return F("RUNNING");
    case SystemState::OfflineDegraded:
      return F("OFFLINE");
    case SystemState::Error:
      return F("ERROR");
    case SystemState::ScreenSleep:
      return F("SCREEN_SLEEP");
  }
  return F("UNKNOWN");
}

const __FlashStringHelper* toFlashString(BaseScreen screen) {
  switch (screen) {
    case BaseScreen::Pet:
      return F("Pet");
    case BaseScreen::Overview:
      return F("Overview");
    case BaseScreen::Info:
      return F("Info");
  }
  return F("Screen");
}

const __FlashStringHelper* toFlashString(GuideScreen screen) {
  switch (screen) {
    case GuideScreen::None:
      return F("None");
    case GuideScreen::WifiSetup:
      return F("WiFi Setup");
    case GuideScreen::PairSetup:
      return F("Pair Setup");
  }
  return F("Guide");
}

const __FlashStringHelper* toFlashString(OverlayState overlay) {
  switch (overlay) {
    case OverlayState::None:
      return F("None");
    case OverlayState::Menu:
      return F("Menu");
    case OverlayState::Brightness:
      return F("Brightness");
    case OverlayState::Volume:
      return F("Volume");
    case OverlayState::PetSelect:
      return F("PetSelect");
  }
  return F("Overlay");
}

const __FlashStringHelper* toFlashString(PetState petState) {
  switch (petState) {
    case PetState::Idle:
      return F("Idle");
    case PetState::Thinking:
      return F("Thinking");
    case PetState::Busy:
      return F("Busy");
    case PetState::Waiting:
      return F("Waiting");
    case PetState::Error:
      return F("Error");
    case PetState::Done:
      return F("Done");
    case PetState::Sleep:
      return F("Sleep");
    case PetState::Offline:
      return F("Offline");
  }
  return F("State");
}

const __FlashStringHelper* toFlashString(SessionState sessionState) {
  switch (sessionState) {
    case SessionState::Thinking:
      return F("Thinking");
    case SessionState::Busy:
      return F("Busy");
    case SessionState::Waiting:
      return F("Waiting");
    case SessionState::Error:
      return F("Error");
    case SessionState::Done:
      return F("Done");
  }
  return F("Session");
}

const __FlashStringHelper* toFlashString(ReminderType reminder) {
  switch (reminder) {
    case ReminderType::None:
      return F("None");
    case ReminderType::Waiting:
      return F("Waiting");
    case ReminderType::Error:
      return F("Error");
    case ReminderType::Done:
      return F("Done");
    case ReminderType::Stale:
      return F("Stale");
  }
  return F("Reminder");
}

const __FlashStringHelper* toFlashString(WifiUiState state) {
  switch (state) {
    case WifiUiState::Missing:
      return F("Missing");
    case WifiUiState::Connecting:
      return F("Connecting");
    case WifiUiState::Ready:
      return F("Ready");
  }
  return F("WiFi");
}

const __FlashStringHelper* toFlashString(PairUiState state) {
  switch (state) {
    case PairUiState::Missing:
      return F("Missing");
    case PairUiState::Pending:
      return F("Pending");
    case PairUiState::Ready:
      return F("Ready");
  }
  return F("Pair");
}

const __FlashStringHelper* toFlashString(RuntimeUiState state) {
  switch (state) {
    case RuntimeUiState::Normal:
      return F("Normal");
    case RuntimeUiState::Degraded:
      return F("Degraded");
  }
  return F("Runtime");
}

}  // namespace buddy
