#pragma once
#include <WebServer.h>
#include <Preferences.h>
#include <WiFi.h>

// Constants
#define NVS_NAMESPACE "cfg"

// DAC voltage constants (millivolts)
constexpr int DAC_VMIN = 2000;      // Minimum voltage (2.0V)
constexpr int DAC_VCEN = 4000;      // Center voltage (4.0V)
constexpr int DAC_VAMP_BASE = 2000; // Base amplitude (2.0V)
constexpr int DAC_VMAX = 6000;      // Maximum voltage (6.0V)

// Timing constants (milliseconds)
constexpr uint32_t DATA_TIMEOUT_MS = 4000;  // Data considered stale after 4s

// LEDC constants
#define LEDC_TIMER_RESOLUTION    10
#define LEDC_BASE_FREQ           5000

// DAC channels
constexpr uint8_t CH_SIN = 0;
constexpr uint8_t CH_COS = 1;

// Enum protokollille
enum { PROTO_UDP = 0, PROTO_TCP = 1, PROTO_HTTP = 2 };

// LITE Plus: Wind data types (selectable)
enum WindDataType {
  DATA_APPARENT_WIND = 0,  // Apparent Wind (näennäistuuli) - from MWV(R) or VWR
  DATA_TRUE_WIND = 1,      // True Wind (todellinen tuuli) - from MWV(T) or VWT
  DATA_SOG = 2,            // Speed Over Ground - from GPS (RMC/VTG)
  DATA_COG = 3             // Course Over Ground - from GPS (RMC/VTG)
};

// LITE Multi: Simplified display configuration
struct DisplayConfig {
  bool enabled;
  char type[16];         // "logicwind" | "sumlog"
  uint8_t speedSource;   // Which data source for SPEED (WindDataType)
  int offsetDeg;         // Logic Wind adjustment (DEPRECATED - use global)
  float sumlogK;         // Pulse per knot
  int sumlogFmax;        // Max frequency
  int pulseDuty;         // Pulse duty %
  int pulsePin;          // GPIO pin
  
  // Runtime data (updated automatically from speedSource)
  float currentSpeed_kn;  // Current speed for this display
  uint32_t lastUpdate_ms; // Timestamp of last update
};

// Global variables from wind_project.ino
extern Preferences prefs;
extern DisplayConfig displays[3];  // LITE Multi: 3 displays

// LITE Multi: Global direction (shared by all Logic Wind displays)
extern uint8_t global_direction_source;  // Which data source for DIRECTION
extern int global_wind_angle;            // Current global direction

// FreeRTOS synchronization
extern SemaphoreHandle_t dataMutex;
extern SemaphoreHandle_t wifiMutex;
extern SemaphoreHandle_t nvsMutex;
extern SemaphoreHandle_t pauseAckSemaphore;

// LEDC variables (LITE Multi: 3 channels)
extern const uint8_t LEDC_CHANNELS[3];
extern bool ledcActive[3];

extern float sumlog_speed_kn;
extern int offsetDeg;
extern int angleDeg;
extern int lastAngleSent;
extern char lastSentenceType[];
extern char lastSentenceRaw[];
extern char connProfileName[];
extern bool hasMwvR;
extern bool hasMwvT;

// LITE Plus: Multiple data sources
extern float apparent_speed_kn;
extern float apparent_angle_deg;
extern bool apparent_hasData;
extern uint32_t apparent_lastUpdate_ms;
extern char apparent_source[];

extern float true_speed_kn;
extern float true_angle_deg;
extern bool true_hasData;
extern uint32_t true_lastUpdate_ms;
extern char true_source[];

extern float gps_sog_kn;
extern float gps_cog_deg;
extern bool gps_hasSOG;
extern bool gps_hasCOG;
extern uint32_t gps_lastUpdate_ms;

extern bool hasVwr;
extern bool hasMwvT;
extern bool hasVwt;
extern bool freezeNMEA;
extern uint8_t  nmeaProto;
extern uint16_t nmeaPort;
extern char nmeaHost[];
extern volatile bool tcpConnected;
extern volatile bool udpConnected;
extern char sta_ssid[];
extern char sta_pass[];
extern char ap_pass[];
extern uint32_t lastNmeaDataMs;

// AP settings constants
#define AP_SSID "VDO-Cal"
#define AP_PASS "wind12345"

// Core funktiot (LITE Multi: displayNum parameters restored)
extern void loadConfig();
void nmeaPollTaskFunc(void *pvParameters);
void saveDisplayConfig(int displayNum = -1);
void saveNetworkConfig(const char* ssid, const char* pass);
void startDisplay(int displayNum);
void stopDisplay(int displayNum);
void updateDisplayPulse(int displayNum);
void setupWebUI(WebServer& server);
void bindTransport();
void connectSTA();
void setOutputsDeg(int deg);  // Global direction

// Page builders (web_pages.cpp) - LITE: single page
String buildSinglePage();
