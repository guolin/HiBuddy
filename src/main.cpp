#include "app/app.h"

buddy::BuddyApp app;

void setup() {
  app.begin();
}

void loop() {
  app.tick();
}
