#pragma once
/** WiFiManager: saved credentials live in NVS (never in secrets.h).
 *  First boot (or no saved network) opens the captive portal "GrokBot-Round". */
void wifiStubInit();
/** Pump the non-blocking portal / reconnect logic. Call every loop(). */
void wifiStubLoop();
bool wifiStubConnected();
/** Re-open the portal on demand (Z_WIFI). */
void wifiStubStartPortal();
