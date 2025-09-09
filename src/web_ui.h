#pragma once
#include <WebServer.h>
#include <Preferences.h>
#include <WiFi.h>

// Enum protokollille
enum { PROTO_UDP = 0, PROTO_TCP = 1 };

// Display configuration structure
struct DisplayConfig {
  bool enabled;
  String type;           // "logicwind" | "sumlog"
  String sentence;       // "MWV" | "VWR" | "VWT"
  int offsetDeg;         // Logic Wind adjustment
  float sumlogK;         // Pulse per knot
  int sumlogFmax;        // Max frequency
  int pulseDuty;         // Pulse duty %
  int pulsePin;          // GPIO pin
  int gotoAngle;         // Manual angle
  bool freeze;           // Freeze NMEA
  
  // Filter settings for stable pulse generation
  float speedFilterAlpha; // Speed filter coefficient (0.1-0.9)
  float freqDeadband;    // Frequency deadband (Hz)
  float maxStepPercent;  // Max frequency step per update (%)
};

// Globaalit muuttujat jotka sijaitsevat wind_project.ino:ssa
extern Preferences prefs;
extern DisplayConfig displays[3];

// LEDC variables
extern const uint8_t LEDC_CHANNELS[3];
extern bool ledcActive[3];

extern float sumlog_speed_kn;
extern int offsetDeg;
extern int angleDeg;
extern int lastAngleSent;
extern String lastSentenceType;
extern String lastSentenceRaw;
extern bool freezeNMEA;
extern uint8_t  nmeaProto;
extern uint16_t nmeaPort;
extern String   nmeaHost;
extern WiFiClient tcpClient;
extern char sta_ssid[];
extern char sta_pass[];
extern uint32_t lastNmeaDataMs;

// Core funktiot
void saveDisplayConfig(int displayNum);
void startDisplay(int displayNum);
void stopDisplay(int displayNum);
void updateDisplayPulse(int displayNum);
void updateAllDisplayPulses();
void setupWebUI(WebServer& server);
void bindTransport();
void connectSTA();
void ensureTCPConnected();
void setOutputsDeg(int deg);

// Page builders (web_pages.cpp)
String buildPageHeader(String activeTab);
String buildPageFooter();
String buildStatusPage();
String buildNetworkPage();
String buildDisplayPage(int displayNum);
