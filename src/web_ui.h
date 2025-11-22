#pragma once
#include <WebServer.h>
#include <Preferences.h>
#include <WiFi.h>

// Enum protokollille
enum { PROTO_UDP = 0, PROTO_TCP = 1, PROTO_HTTP = 2 };

// Wind data types (user-friendly selection)
enum WindDataType {
  DATA_APPARENT_WIND = 0,  // Apparent Wind (näennäistuuli) - from MWV(R) or VWR
  DATA_TRUE_WIND = 1,      // True Wind (todellinen tuuli) - from MWV(T) or VWT
  DATA_VMG = 2,            // Velocity Made Good
  DATA_GROUND_WIND = 3,    // Ground Wind (future)
  DATA_SOG = 4,            // Speed Over Ground - from GPS (RMC/VTG)
  DATA_COG = 5             // Course Over Ground - from GPS (RMC/VTG)
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
extern DisplayConfig displays[3];

// FreeRTOS synchronization
extern SemaphoreHandle_t dataMutex;
extern SemaphoreHandle_t wifiMutex;
extern SemaphoreHandle_t nvsMutex;
extern SemaphoreHandle_t pauseAckSemaphore;

// LEDC variables
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

// Data source tracking
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

extern float vmg_kn;
extern bool vmg_hasData;
extern uint32_t vmg_lastUpdate_ms;

extern float gps_sog_kn;
extern float gps_cog_deg;
extern bool gps_hasSOG;
extern bool gps_hasCOG;
extern uint32_t gps_lastUpdate_ms;
extern bool hasVwr;
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

// Core funktiot
extern void loadConfig();
void nmeaPollTaskFunc(void *pvParameters);
void saveDisplayConfig(int displayNum);
void saveNetworkConfig(const char* ssid, const char* pass);
void startDisplay(int displayNum);
void stopDisplay(int displayNum);
void updateDisplayPulse(int displayNum);
void updateAllDisplayPulses();
void setupWebUI(WebServer& server);
void bindTransport();
void connectSTA();
void setOutputsDeg(int displayNum, int deg);

// Page builders (web_pages.cpp)
String buildPageHeader(String activeTab);
String buildPageFooter();
String buildStatusPage();
String buildNetworkPage();
String buildDisplayPage(int displayNum);
