#pragma once

// ==========================================
// WINDDRAGONS NETWORK & CLOUD CONFIGURATION
// ==========================================

// Wi-Fi Credentials for Base Station
#define WIFI_SSID             "WIFI_NETWORK_NAME"
#define WIFI_PASSWORD         "WIFI_PASSWORD"

// Station Configuration
#define BASE_STATION_ID       "BASE-ALCOCHETE-01"

// WindDragons Telemetry & Command API
#define WINDDRAGONS_API_URL   "https://winddragons.app/api/circuits/station/telemetry"

// Interval to sync with Cloud (milliseconds)
#define TELEMETRY_SYNC_INTERVAL_MS  3000

// Maximum number of rovers tracked in Base fleet memory
#define MAX_FLEET_ROVERS      8

// Timeout after which a rover is considered offline (ms)
#define ROVER_OFFLINE_TIMEOUT_MS    15000
