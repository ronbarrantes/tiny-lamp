#pragma once

// Call once at boot, then service Wi-Fi and HTTP on every loop iteration.
void wifiSetupBegin();
void wifiSetupLoop();
