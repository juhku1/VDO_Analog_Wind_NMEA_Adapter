#pragma once
#include <WebServer.h>
#include <Preferences.h>
#include <WiFi.h>

// Enum protokollille
enum { PROTO_UDP = 0, PROTO_TCP = 1, PROTO_HTTP = 2 };

// Display configuration structure
struct DisplayConfig {
  bool enabled;
  char type[16];         // "logicwind" | "sumlog"
  char sentence[8];      // "MWV" | "VWR" | "VWT"
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
