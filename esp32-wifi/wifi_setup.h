#pragma once

class WebServer;

// Call once at boot, then service Wi-Fi and HTTP on every loop iteration.
void wifiSetupBegin(void (*registerRoutes)(WebServer&) = nullptr);
void wifiSetupLoop();
