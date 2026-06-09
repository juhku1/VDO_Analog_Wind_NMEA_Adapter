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

// UDP forward sentence mask bits
constexpr uint32_t FWD_MWV = (1UL << 0);
constexpr uint32_t FWD_VWR = (1UL << 1);
constexpr uint32_t FWD_VWT = (1UL << 2);
constexpr uint32_t FWD_RMC = (1UL << 3);
constexpr uint32_t FWD_VTG = (1UL << 4);
constexpr uint32_t FWD_HDT = (1UL << 5);
constexpr uint32_t FWD_HDM = (1UL << 6);
constexpr uint32_t FWD_DEFAULT_MASK = FWD_MWV | FWD_VWR | FWD_VWT | FWD_RMC | FWD_VTG | FWD_HDT | FWD_HDM;

// LITE Multi: Wind data types (selectable)
enum WindDataType {
  DATA_APPARENT_WIND = 0,  // Apparent Wind (näennäistuuli) - from MWV(R) or VWR
  DATA_TRUE_WIND = 1,      // True Wind (todellinen tuuli) - from MWV(T), VWT, or calculated
  DATA_SOG = 2,            // Speed Over Ground - from GPS (RMC/VTG)
  DATA_COG = 3,            // Course Over Ground - from GPS (RMC/VTG)
  DATA_VMG = 4             // Velocity Made Good - calculated (SOG × cos(TWA))
};

// LITE Multi: Speed Pulse Output configuration
struct SpeedPulseConfig {
  bool enabled;
  char instrumentType[16]; // "logicwind" | "sumlog"
  uint8_t speedSource;     // Which data source for SPEED (WindDataType)
  float pulsesPerKnot;     // Calibration: pulses per knot
  int maxFrequency;        // Max frequency (Hz)
  int dutyCycle;           // Pulse duty cycle (%)
  int pulsePin;            // GPIO output pin
  
  // Runtime data (updated automatically from speedSource)
  float currentSpeed_kn;   // Current speed for this output
  uint32_t lastUpdate_ms;  // Timestamp of last update
};

// Global variables from wind_project.ino
extern Preferences prefs;
extern SpeedPulseConfig speedPulses[3];  // LITE Multi: 3 speed pulse outputs

// LITE Multi: Global direction (shared by all Logic Wind instruments)
extern uint8_t directionSource;  // Which data source for DIRECTION
extern int directionAngle;       // Current global direction
extern int directionOffset;      // Calibration offset for direction (-180 to +180)

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
extern float gps_heading_deg;
extern bool gps_hasSOG;
extern bool gps_hasCOG;
extern bool gps_hasHeading;
extern uint32_t gps_lastUpdate_ms;

extern float vmg_kn;
extern bool vmg_hasData;
extern uint32_t vmg_lastUpdate_ms;

extern bool hasVwr;
extern bool hasMwvT;
extern bool hasVwt;
extern bool freezeNMEA;
extern uint8_t  nmeaProto;
extern uint16_t nmeaPort;
extern char nmeaHost[];
extern volatile bool tcpConnected;
extern volatile bool udpConnected;

// UDP output forwarding settings/runtime
extern bool udpForwardEnabled;
extern bool udpForwardBroadcast;
extern char udpForwardHost[];
extern uint16_t udpForwardPort;
extern uint16_t udpForwardMinIntervalMs;
extern uint32_t udpForwardMask;
extern uint32_t udpForwardCount;
extern uint32_t udpForwardDropRate;
extern uint32_t udpForwardDropDup;
extern uint32_t udpForwardDropFilter;

// Separate NMEA tracking for TCP and UDP
extern char lastTcpSentence[];
extern uint32_t lastTcpDataMs;
extern char lastUdpSentence[];
extern uint32_t lastUdpDataMs;
extern char sta_ssid[];
extern char sta_pass[];
extern char ap_pass[];
extern uint32_t lastNmeaDataMs;

// AP settings constants
#define AP_SSID "VDO-Cal"
#define AP_PASS "wind12345"

// Core funktiot (LITE Multi: speed pulse functions)
extern void loadConfig();
void nmeaPollTaskFunc(void *pvParameters);
void saveSpeedPulseConfig(int pulseNum = -1);
void saveNetworkConfig(const char* ssid, const char* pass);
void startSpeedPulse(int pulseNum);
void stopSpeedPulse(int pulseNum);
void updateSpeedPulse(int pulseNum);
void setupWebUI(WebServer& server);
void bindTransport();
void connectSTA();
void setDirectionOutput(int degrees);  // Global direction to DAC

// Page builders (web_pages.cpp) - LITE: single page
String buildSinglePage();
