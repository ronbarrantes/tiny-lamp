#include "wifi_setup.h"

void setup() {
  Serial.begin(115200);
  wifiSetupBegin();
}

void loop() {
  wifiSetupLoop();
  delay(2);
}
