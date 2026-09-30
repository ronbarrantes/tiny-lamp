#include "pov.h"
#include "../esp32-wifi/wifi_setup.h"

void setup() {
  Serial.begin(115200);
  povBegin();
  wifiSetupBegin(povRegisterRoutes);
}

void loop() {
  wifiSetupLoop();
  delay(2);
}
