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

// LITE: Wind data types (Apparent Wind only)
enum WindDataType {
  DATA_APPARENT_WIND = 0  // Apparent Wind (näennäistuuli) - from MWV(R) or VWR
};

// Display configuration structure
struct DisplayConfig {
  bool enabled;
  char type[16];         // "logicwind" | "sumlog"
  uint8_t dataType;      // WindDataType: what kind of wind data to show
  char sentence[8];      // DEPRECATED: kept for backward compatibility
  int offsetDeg;         // Logic Wind adjustment
  float sumlogK;         // Pulse per knot
  int sumlogFmax;        // Max frequency
  int pulseDuty;         // Pulse duty %
  int pulsePin;          // GPIO pin
  int gotoAngle;         // Manual angle
  
  // Per-display wind data (protected by dataMutex)
  float windSpeed_kn;    // Wind speed in knots for this display
  int windAngle_deg;     // Wind angle in degrees for this display
  uint32_t lastUpdate_ms; // Timestamp of last data update
};

// Global variables from wind_project.ino
extern Preferences prefs;
extern DisplayConfig display;  // LITE: single display only

// FreeRTOS synchronization
extern SemaphoreHandle_t dataMutex;
extern SemaphoreHandle_t wifiMutex;
extern SemaphoreHandle_t nvsMutex;
extern SemaphoreHandle_t pauseAckSemaphore;

// LEDC variables (LITE: single channel)
extern const uint8_t LEDC_CHANNEL;
extern bool ledcActive;

extern float sumlog_speed_kn;
extern int offsetDeg;
extern int angleDeg;
extern int lastAngleSent;
extern char lastSentenceType[];
extern char lastSentenceRaw[];
extern char connProfileName[];
extern bool hasMwvR;
extern bool hasMwvT;

// LITE: Apparent Wind data only
extern float apparent_speed_kn;
extern float apparent_angle_deg;
extern bool apparent_hasData;
extern uint32_t apparent_lastUpdate_ms;
extern char apparent_source[];

extern bool hasVwr;
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

// Core funktiot (LITE: removed displayNum parameters)
extern void loadConfig();
void nmeaPollTaskFunc(void *pvParameters);
void saveDisplayConfig();
void saveNetworkConfig(const char* ssid, const char* pass);
void startDisplay();
void stopDisplay();
void updateDisplayPulse();
void setupWebUI(WebServer& server);
void bindTransport();
void connectSTA();
void setOutputsDeg(int deg);

// Page builders (web_pages.cpp) - LITE: single page
String buildSinglePage();
