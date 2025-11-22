#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "DFRobot_GP8403.h"
#include "web_ui.h"

// LEDC for hardware PWM pulse generation
#define LEDC_TIMER_RESOLUTION    10
#define LEDC_BASE_FREQ           5000

/* ========= Constants ========= */

// NVS (Non-Volatile Storage) keys
constexpr const char* NVS_KEY_DISPLAY_ENABLED = "d%d_enabled";
constexpr const char* NVS_KEY_DISPLAY_TYPE = "d%d_type";
constexpr const char* NVS_KEY_DISPLAY_DATATYPE = "d%d_dataType";
constexpr const char* NVS_KEY_DISPLAY_OFFSET = "d%d_offset";
constexpr const char* NVS_KEY_DISPLAY_SUMLOGK = "d%d_sumlogK";
constexpr const char* NVS_KEY_DISPLAY_FMAX = "d%d_sumlogFmax";
constexpr const char* NVS_KEY_DISPLAY_DUTY = "d%d_pulseDuty";
constexpr const char* NVS_KEY_DISPLAY_PIN = "d%d_pulsePin";
constexpr const char* NVS_KEY_DISPLAY_GOTO = "d%d_gotoAngle";

// DAC voltage constants (millivolts)
constexpr int DAC_VMIN = 2000;      // Minimum voltage (2.0V)
constexpr int DAC_VCEN = 4000;      // Center voltage (4.0V)
constexpr int DAC_VAMP_BASE = 2000; // Base amplitude (2.0V)
constexpr int DAC_VMAX = 6000;      // Maximum voltage (6.0V)

// Timing constants (milliseconds)
constexpr uint32_t DATA_TIMEOUT_MS = 4000;  // Data considered stale after 4s
constexpr uint32_t GPS_TIMEOUT_MS = 5000;   // GPS data timeout
constexpr uint32_t SENTENCE_WINDOW_MS = 5000; // Sentence type tracking window

/* ========= Global Settings and Variables ========= */

// FreeRTOS synchronization primitives for thread safety
SemaphoreHandle_t dataMutex = NULL;      // Protects wind data (speed, angle)
SemaphoreHandle_t wifiMutex = NULL;      // Protects WiFi operations
SemaphoreHandle_t nvsMutex = NULL;       // Protects NVS (Preferences) access
SemaphoreHandle_t pauseAckSemaphore = NULL;  // For Core 1 pause acknowledgment

Preferences prefs;

// Unified display array (3 displays)
DisplayConfig displays[3];

// LEDC channels for each display (0-2) with separate timers
const uint8_t LEDC_CHANNELS[3] = {0, 1, 2};
const uint8_t LEDC_TIMERS[3] = {0, 1, 2};
bool ledcActive[3] = {false, false, false};
uint32_t lastFreq[3] = {0, 0, 0};

// Wind data - protected by dataMutex
float sumlog_speed_kn = 0.0;
int angleDeg = 0;
int lastAngleSent = 0;
char lastSentenceType[32] = "-";
char lastSentenceRaw[256] = "-";

// GPS data - protected by dataMutex
float gps_sog_kn = 0.0;        // Speed Over Ground (knots)
float gps_cog_deg = 0.0;       // Course Over Ground (degrees)
float gps_heading_deg = 0.0;   // True Heading (degrees)
bool gps_hasSOG = false;
bool gps_hasCOG = false;
bool gps_hasHeading = false;
uint32_t gps_lastUpdate_ms = 0;

// Apparent Wind data for calculation - protected by dataMutex
float apparent_speed_kn = 0.0;
float apparent_angle_deg = 0.0;
bool apparent_hasData = false;
uint32_t apparent_lastUpdate_ms = 0;
char apparent_source[16] = "-";  // "MWV(R)", "VWR", etc.

// True Wind data - protected by dataMutex
float true_speed_kn = 0.0;
float true_angle_deg = 0.0;
bool true_hasData = false;
uint32_t true_lastUpdate_ms = 0;
char true_source[16] = "-";  // "MWV(T)", "VWT", "Calculated"

// VMG data - protected by dataMutex
float vmg_kn = 0.0;
bool vmg_hasData = false;
uint32_t vmg_lastUpdate_ms = 0;

#define AP_SSID           "VDO-Cal"
#define AP_PASS           "wind12345"
uint8_t  nmeaProto = PROTO_HTTP;
uint16_t nmeaPort  = 80;
char nmeaHost[64] = "192.168.4.1";

// Persistent TCP client for real-time wind data
WiFiClient tcpClient;
uint32_t lastTcpAttempt = 0;

// UDP client for OpenPlotter/secondary source
WiFiUDP udpClient;
uint32_t lastUdpAttempt = 0;

// Separate connection states for TCP and UDP
volatile bool tcpConnected = false;
volatile bool udpConnected = false;

char netBuf[1472];
char udpBuf[1472];
char nmeaLineBuf[256] = {0};
size_t nmeaLineBufLen = 0;
uint32_t lastNmeaDataMs = 0;

// FreeRTOS task for NMEA polling on Core 1
TaskHandle_t nmeaPollTask = NULL;
volatile bool pauseNmeaPoll = false;

#define SDA_PIN   21
#define SCL_PIN   22
#define I2C_ADDR  0x5F
#define I2C_HZ    100000

DFRobot_GP8403 dac(&Wire, I2C_ADDR);

constexpr uint8_t CH_SIN = 0;
constexpr uint8_t CH_COS = 1;

int offsetDeg = 0;
char connProfileName[64] = "Yachta";
bool freezeNMEA = false;

// NMEA sentence type tracking (5s window)
bool hasMwvR = false;
bool hasMwvT = false;
bool hasVwr = false;
bool hasVwt = false;
uint32_t lastFlagReset = 0;

char sta_ssid[33] = {0};
char sta_pass[65] = {0};
char ap_pass[65] = {0};

WebServer server(80);

// FreeRTOS task for NMEA polling on Core 1
void nmeaPollTaskFunc(void *pvParameters) {
  Serial.println("NMEA polling task started on Core 1");
  
  // Wait for WiFi to be ready before attempting connections
  Serial.println("Waiting for WiFi to be ready...");
  uint32_t wifiWaitStart = millis();
  while ((WiFi.status() != WL_CONNECTED && WiFi.softAPgetStationNum() == 0) && 
         millis() - wifiWaitStart < 30000) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  Serial.println("WiFi ready, starting NMEA polling (TCP + UDP)");
  
  // Set TCP client to non-blocking mode
  tcpClient.setTimeout(0);
  
  while(1) {
    // Check if we should pause during config save
    if (pauseNmeaPoll) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;  // Skip this iteration until flag cleared
    }
    
    if(!freezeNMEA) {
      // Poll TCP (Profile 1)
      ensureTCPConnected(tcpClient);
      if(tcpClient.connected()) {
        pollTCP(tcpClient);
        tcpConnected = true;
      } else {
        tcpConnected = false;
      }
      
      // Poll UDP (Profile 2)
      ensureUDPBound();
      if(udpConnected) {
        pollUDP();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));  // 5ms cycle = 200Hz = responsive for wind direction
  }
}

/* ========= Asetusten tallennus ========= */
void saveDisplayConfig(int displayNum = -1) {
  xSemaphoreTake(nvsMutex, portMAX_DELAY);
  prefs.begin(NVS_NAMESPACE, false);
  
  if (displayNum == -1) {
    // Save all displays
    for (int i = 0; i < 3; i++) {
      char prefix[8];
      snprintf(prefix, sizeof(prefix), "d%d_", i);
      char key[16];
      
      snprintf(key, sizeof(key), "%senabled", prefix);
      prefs.putBool(key, displays[i].enabled);
      snprintf(key, sizeof(key), "%stype", prefix);
      prefs.putString(key, displays[i].type);
      snprintf(key, sizeof(key), "%sdataType", prefix);
      prefs.putUChar(key, displays[i].dataType);
      snprintf(key, sizeof(key), "%soffset", prefix);
      prefs.putInt(key, displays[i].offsetDeg);
      snprintf(key, sizeof(key), "%ssumlogK", prefix);
      prefs.putFloat(key, displays[i].sumlogK);
      snprintf(key, sizeof(key), "%ssumlogFmax", prefix);
      prefs.putInt(key, displays[i].sumlogFmax);
      snprintf(key, sizeof(key), "%spulseDuty", prefix);
      prefs.putInt(key, displays[i].pulseDuty);
      snprintf(key, sizeof(key), "%spulsePin", prefix);
      prefs.putInt(key, displays[i].pulsePin);
      snprintf(key, sizeof(key), "%sgotoAngle", prefix);
      prefs.putInt(key, displays[i].gotoAngle);
    }
  } else if (displayNum >= 0 && displayNum < 3) {
    // Save single display
    char prefix[8];
    snprintf(prefix, sizeof(prefix), "d%d_", displayNum);
    char key[16];
    
    snprintf(key, sizeof(key), "%senabled", prefix);
    prefs.putBool(key, displays[displayNum].enabled);
    snprintf(key, sizeof(key), "%stype", prefix);
    prefs.putString(key, displays[displayNum].type);
    snprintf(key, sizeof(key), "%sdataType", prefix);
    prefs.putUChar(key, displays[displayNum].dataType);
    snprintf(key, sizeof(key), "%soffset", prefix);
    prefs.putInt(key, displays[displayNum].offsetDeg);
    snprintf(key, sizeof(key), "%ssumlogK", prefix);
    prefs.putFloat(key, displays[displayNum].sumlogK);
    snprintf(key, sizeof(key), "%ssumlogFmax", prefix);
    prefs.putInt(key, displays[displayNum].sumlogFmax);
    snprintf(key, sizeof(key), "%spulseDuty", prefix);
    prefs.putInt(key, displays[displayNum].pulseDuty);
    snprintf(key, sizeof(key), "%spulsePin", prefix);
    prefs.putInt(key, displays[displayNum].pulsePin);
    snprintf(key, sizeof(key), "%sgotoAngle", prefix);
    prefs.putInt(key, displays[displayNum].gotoAngle);
  }
  
  prefs.end();
  xSemaphoreGive(nvsMutex);
}

void loadConfig(){
  prefs.begin(NVS_NAMESPACE, false);
  
  // Initialize displays with defaults
  for (int i = 0; i < 3; i++) {
    char prefix[8];
    snprintf(prefix, sizeof(prefix), "d%d_", i);
    char key[16];
    
    snprintf(key, sizeof(key), "%senabled", prefix);
    displays[i].enabled = prefs.getBool(key, i == 0);
    
    snprintf(key, sizeof(key), "%stype", prefix);
    String typeStr = prefs.getString(key, "sumlog");
    strncpy(displays[i].type, typeStr.c_str(), sizeof(displays[i].type) - 1);
    displays[i].type[sizeof(displays[i].type) - 1] = '\0';
    
    // Load dataType (new) or migrate from sentence (old)
    snprintf(key, sizeof(key), "%sdataType", prefix);
    if (prefs.isKey(key)) {
      // New format: dataType exists
      displays[i].dataType = prefs.getUChar(key, DATA_APPARENT_WIND);
    } else {
      // Old format: migrate from sentence
      snprintf(key, sizeof(key), "%ssentence", prefix);
      String sentStr = prefs.getString(key, "MWV");
      // Migrate: MWV -> Apparent Wind (default), keep for compatibility
      displays[i].dataType = DATA_APPARENT_WIND;
      strncpy(displays[i].sentence, sentStr.c_str(), sizeof(displays[i].sentence) - 1);
      displays[i].sentence[sizeof(displays[i].sentence) - 1] = '\0';
    }
    
    snprintf(key, sizeof(key), "%soffset", prefix);
    displays[i].offsetDeg = prefs.getInt(key, 0);
    
    snprintf(key, sizeof(key), "%ssumlogK", prefix);
    displays[i].sumlogK = prefs.getFloat(key, 1.0f);
    
    snprintf(key, sizeof(key), "%ssumlogFmax", prefix);
    displays[i].sumlogFmax = prefs.getInt(key, 150);
    
    snprintf(key, sizeof(key), "%spulseDuty", prefix);
    displays[i].pulseDuty = prefs.getInt(key, 10);
    
    snprintf(key, sizeof(key), "%spulsePin", prefix);
    displays[i].pulsePin = prefs.getInt(key, 12 + i * 2);
    
    snprintf(key, sizeof(key), "%sgotoAngle", prefix);
    displays[i].gotoAngle = prefs.getInt(key, 0);
    
    // Initialize per-display wind data
    displays[i].windSpeed_kn = 0.0f;
    displays[i].windAngle_deg = 0;
    displays[i].lastUpdate_ms = 0;
  }
  
  offsetDeg = prefs.getInt("offset", 0);
  
  // Load connection profile selection - DEPRECATED
  // Both profiles are now always active simultaneously (TCP + UDP)
  uint8_t connMode = prefs.getUChar("conn_mode", 0);
  (void)connMode;  // Suppress unused variable warning
  
  // Both connections are always active now:
  // - Profile 1 (TCP): configured host/port
  // - Profile 2 (UDP): listening on configured port
  String p1_name  = prefs.getString("p1_name", "Yachta");
  uint8_t p1_proto = prefs.getUChar("p1_proto", PROTO_TCP);
  String p1_host  = prefs.getString("p1_host", "192.168.68.145");
  uint16_t p1_port = prefs.getUShort("p1_port", 6666);
  
  // Load Profile 2 (OpenPlotter - UDP by default, both connections always active)
  String p2_name  = prefs.getString("p2_name", "OpenPlotter");
  uint8_t p2_proto = prefs.getUChar("p2_proto", PROTO_UDP);  // P2 defaults to UDP
  String p2_host  = prefs.getString("p2_host", "");
  uint16_t p2_port = prefs.getUShort("p2_port", 10110);
  
  // Configure TCP connection (Profile 1) - always active
  nmeaProto = p1_proto;
  strncpy(nmeaHost, p1_host.c_str(), sizeof(nmeaHost) - 1);
  nmeaHost[sizeof(nmeaHost) - 1] = '\0';
  nmeaPort = p1_port;
  strncpy(connProfileName, p1_name.c_str(), sizeof(connProfileName) - 1);
  connProfileName[sizeof(connProfileName) - 1] = '\0';
  
  String s         = prefs.getString("sta_ssid", "");
  String ap        = prefs.getString("ap_pass", AP_PASS);
  
  // Set YachtaServer password
  String p = prefs.getString("sta_pass", "8765432A1");  // Load from preferences or default
  
  // Load WiFi profile selection (0 or 1)
  uint8_t wifi_mode = prefs.getUChar("wifi_mode", 255);  // 255 = not set (backward compat)
  
  // Load WiFi Profile 1
  String w1_ssid = prefs.getString("w1_ssid", "");
  String w1_pass = prefs.getString("w1_pass", "");
  
  // Load WiFi Profile 2
  String w2_ssid = prefs.getString("w2_ssid", "");
  String w2_pass = prefs.getString("w2_pass", "");
  
  // Apply selected WiFi profile (with fallback to old sta_ssid)
  if (wifi_mode != 255) {
    // New WiFi profile system is active
    if (wifi_mode == 1 && w2_ssid.length() > 0) {
      s = w2_ssid;
      p = w2_pass;
    } else if (w1_ssid.length() > 0) {
      s = w1_ssid;
      p = w1_pass;
    }
    // else: use old s and p values
  }
  // else: wifi_mode never set, use old sta_ssid/sta_pass system
  
  s.toCharArray(sta_ssid, sizeof(sta_ssid));
  p.toCharArray(sta_pass, sizeof(sta_pass));
  ap.toCharArray(ap_pass, sizeof(ap_pass));
  prefs.end();
  
  Serial.printf("Network: Profile1 (TCP) %s:%u, Profile2 (UDP) port %u\n", 
    p1_host.c_str(), p1_port, p2_port);
}

void saveNetworkConfig(const char* ssid, const char* pass) {
  if (!ssid || ssid[0] == '\0') return;  // älä kirjoita tyhjää
  prefs.begin(NVS_NAMESPACE, false);              // sama namespace kuin loadConfig()
  
  // Tallenna SSID
  prefs.putString("sta_ssid", ssid);
  
  // Tallenna salasana SSID-kohtaisella avaimella
  String passKey = "pass_" + String(ssid);  // esim. "pass_openplotter"
  if (pass && pass[0] != '\0') {
    prefs.putString(passKey.c_str(), pass);
    Serial.printf("Saved password for SSID '%s' with key '%s'\n", ssid, passKey.c_str());
  }
  
  // Tallenna myös vanhan tavan mukaan yhteensopivuuden vuoksi
  prefs.putString("sta_pass", pass);
  
  // Tallenna verkko-asetukset
  prefs.putUShort("udp_port", nmeaPort);
  prefs.putUChar("proto",     nmeaProto);
  prefs.putString("host",     nmeaHost);
  prefs.end();

  Serial.printf("Saved STA SSID='%s' (len=%u)\n", ssid, (unsigned)strlen(ssid));
}

/* ========= DAC ulostulo ========= */
static inline int wrap360(int d){ d%=360; if(d<0) d+=360; return d; }
static inline int mvClamp(int mv){ if(mv<DAC_VMIN) return DAC_VMIN; if(mv>DAC_VMAX) return DAC_VMAX; return mv; }

void setOutputsDeg(int displayNum, int deg){
  if (displayNum < 0 || displayNum >= 3) return;
  
  // Read per-display angle with mutex protection
  int displayAngle;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  displayAngle = displays[displayNum].windAngle_deg;
  xSemaphoreGive(dataMutex);
  
  int adj = wrap360(displayAngle + displays[displayNum].offsetDeg);
  float r = adj * DEG_TO_RAD;
  float s = sinf(r), c = cosf(r);
  float amp = DAC_VAMP_BASE;
  int sin_mV = mvClamp(DAC_VCEN + (int)lroundf(amp * s));
  int cos_mV = mvClamp(DAC_VCEN + (int)lroundf(amp * c));
  
  // Only update DAC for display 0 (primary display)
  if (displayNum == 0) {
    dac.setDACOutVoltage(sin_mV, CH_SIN);
    dac.setDACOutVoltage(cos_mV, CH_COS);
    
    // Track direction changes
    if (adj != lastAngleSent) {
      Serial.printf("Display %d Direction: %d° (sin:%dmV cos:%dmV)\n", displayNum, adj, sin_mV, cos_mV);
    }
    lastAngleSent = adj;
  }
}

/* ========= LEDC Pulse Generation ========= */
void startDisplay(int displayNum) {
  if (displayNum < 0 || displayNum >= 3) return;
  
  if (!ledcActive[displayNum] && displays[displayNum].enabled) {
    // Setup LEDC channel with separate timer
    ledcSetup(LEDC_CHANNELS[displayNum], LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
    ledcAttachPin(displays[displayNum].pulsePin, LEDC_CHANNELS[displayNum]);
    ledcActive[displayNum] = true;
    lastFreq[displayNum] = 0; // Reset frequency tracking
    
    Serial.printf("Display %d LEDC started, timer=%d\n", displayNum, LEDC_TIMERS[displayNum]);
    updateDisplayPulse(displayNum);
  }
}

void stopDisplay(int displayNum) {
  if (displayNum < 0 || displayNum >= 3) return;
  
  if (ledcActive[displayNum]) {
    ledcWrite(LEDC_CHANNELS[displayNum], 0); // Stop PWM
    ledcDetachPin(displays[displayNum].pulsePin);
    ledcActive[displayNum] = false;
    lastFreq[displayNum] = 0; // Reset frequency tracking
    pinMode(displays[displayNum].pulsePin, INPUT);
    Serial.printf("Display %d LEDC stopped\n", displayNum);
  }
}

void updateDisplayPulse(int displayNum) {
  if (displayNum < 0 || displayNum >= 3 || !ledcActive[displayNum]) return;
  
  DisplayConfig &disp = displays[displayNum];
  
  // Read per-display speed with mutex protection
  float currentSpeed;
  uint32_t lastUpdate;
  uint8_t dataType;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  currentSpeed = disp.windSpeed_kn;
  lastUpdate = disp.lastUpdate_ms;
  dataType = disp.dataType;
  xSemaphoreGive(dataMutex);
  
  // Check for data timeout - both display data AND source data must be fresh
  uint32_t now = millis();
  uint32_t dataAge = now - lastUpdate;
  bool sourceDataStale = false;
  
  // Check if the source data for this display type is stale
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  if (dataType == DATA_APPARENT_WIND && apparent_hasData) {
    sourceDataStale = (now - apparent_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if (dataType == DATA_TRUE_WIND && true_hasData) {
    sourceDataStale = (now - true_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if (dataType == DATA_VMG && vmg_hasData) {
    sourceDataStale = (now - vmg_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if (dataType == DATA_SOG && gps_hasSOG) {
    sourceDataStale = (now - gps_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if (dataType == DATA_COG && gps_hasCOG) {
    sourceDataStale = (now - gps_lastUpdate_ms) > DATA_TIMEOUT_MS;
  }
  xSemaphoreGive(dataMutex);
  
  // Zero speed if either display data or source data is stale
  if (dataAge > DATA_TIMEOUT_MS || sourceDataStale) {
    currentSpeed = 0.0f;
    if (lastFreq[displayNum] != 0) {
      Serial.printf("Display %d timeout! Display age=%u ms, source stale=%d - zeroing speed\n", 
                    displayNum, dataAge, sourceDataStale);
    }
  }
  
  if (strcmp(disp.type, "sumlog") == 0) {
    // Stop immediately if raw speed is 0
    if (currentSpeed < 0.01f && lastFreq[displayNum] != 0) {
      ledcWrite(LEDC_CHANNELS[displayNum], 0);
      lastFreq[displayNum] = 0;
      Serial.printf("Display %d stopped (speed=0)\n", displayNum);
      return;
    }
    
    // Sumlog pulse calculation
    float freq = currentSpeed * disp.sumlogK;
    if (freq > (float)disp.sumlogFmax) freq = (float)disp.sumlogFmax;
    
    if (freq < 0.01f) {
      // Stop PWM when frequency too low
      if (lastFreq[displayNum] != 0) {
        ledcWrite(LEDC_CHANNELS[displayNum], 0);
        lastFreq[displayNum] = 0;
        Serial.printf("Display %d stopped (freq too low)\n", displayNum);
      }
    } else {
      // Round frequency to reduce jitter
      uint32_t freqInt = (uint32_t)(freq + 0.5f);
      
      // Only update if frequency actually changed
      if (freqInt != lastFreq[displayNum]) {
        // Vältä 0Hz joka aiheuttaa LEDC virheen
        if (freqInt == 0) {
          ledcWrite(LEDC_CHANNELS[displayNum], 0);
          lastFreq[displayNum] = 0;
          Serial.printf("Display %d stopped (0Hz avoided)\n", displayNum);
        } else {
          // Calculate duty cycle (0-1023 for 10-bit resolution)
          uint32_t duty = (uint32_t)((1023 * disp.pulseDuty) / 100);
          
          // Set frequency and duty cycle
          ledcChangeFrequency(LEDC_CHANNELS[displayNum], freqInt, LEDC_TIMER_RESOLUTION);
          ledcWrite(LEDC_CHANNELS[displayNum], duty);
          
          lastFreq[displayNum] = freqInt;
          Serial.printf("Display %d freq=%uHz (speed=%.1f kn)\n", displayNum, freqInt, currentSpeed);
        }
      }
    }
  } else if (strcmp(disp.type, "logicwind") == 0) {
    // Stop immediately if raw speed is 0
    if (currentSpeed < 0.01f && lastFreq[displayNum] != 0) {
      ledcWrite(LEDC_CHANNELS[displayNum], 0);
      lastFreq[displayNum] = 0;
      Serial.printf("Display %d Logic Wind stopped (speed=0)\n", displayNum);
      return;
    }
    
    // Logic Wind pulse calculation
    float freq = currentSpeed * disp.sumlogK;
    if (freq > (float)disp.sumlogFmax) freq = (float)disp.sumlogFmax;
    
    if (freq < 0.01f) {
      // Stop PWM when frequency too low
      if (lastFreq[displayNum] != 0) {
        ledcWrite(LEDC_CHANNELS[displayNum], 0);
        lastFreq[displayNum] = 0;
        Serial.printf("Display %d stopped (freq too low)\n", displayNum);
      }
    } else {
      // Round frequency to reduce jitter
      uint32_t freqInt = (uint32_t)(freq + 0.5f);
      
      // Only update if frequency actually changed
      if (freqInt != lastFreq[displayNum]) {
        // Vältä 0Hz joka aiheuttaa LEDC virheen
        if (freqInt == 0) {
          ledcWrite(LEDC_CHANNELS[displayNum], 0);
          lastFreq[displayNum] = 0;
          Serial.printf("Display %d Logic Wind stopped (0Hz avoided)\n", displayNum);
        } else {
          // Calculate duty cycle (0-1023 for 10-bit resolution)
          uint32_t duty = (uint32_t)((1023 * disp.pulseDuty) / 100);
          
          // Set frequency and duty cycle
          ledcChangeFrequency(LEDC_CHANNELS[displayNum], freqInt, LEDC_TIMER_RESOLUTION);
          ledcWrite(LEDC_CHANNELS[displayNum], duty);
          
          lastFreq[displayNum] = freqInt;
          Serial.printf("Display %d Logic Wind freq=%uHz (speed=%.1f kn)\n", displayNum, freqInt, currentSpeed);
        }
      }
    }
  } else {
    // Unknown type - no pulse, stop PWM
    if (lastFreq[displayNum] != 0) {
      ledcWrite(LEDC_CHANNELS[displayNum], 0);
      lastFreq[displayNum] = 0;
    }
  }
}

// Update all active displays (immediate response for accurate measurement)
void updateAllDisplayPulses() {
  for (int i = 0; i < 3; i++) {
    if (ledcActive[i]) {
      updateDisplayPulse(i);
    }
  }
}

// Check if NMEA sentence matches display's data type
bool sentenceMatchesDataType(const char* sentenceType, char reference, uint8_t dataType) {
  if (dataType == DATA_APPARENT_WIND) {
    // Apparent Wind: MWV(R) or VWR
    if (strcmp(sentenceType, "MWV") == 0 && reference == 'R') return true;
    if (strcmp(sentenceType, "VWR") == 0 && reference == '\0') return true;
  }
  else if (dataType == DATA_TRUE_WIND) {
    // True Wind: MWV(T) or VWT
    if (strcmp(sentenceType, "MWV") == 0 && reference == 'T') return true;
    if (strcmp(sentenceType, "VWT") == 0 && reference == '\0') return true;
  }
  // Future: DATA_VMG, DATA_GROUND_WIND
  
  return false;
}

// Update displays that match the given sentence type and reference
void updateDisplaysForSentence(const char* sentenceType, char reference, int angle, float speed, bool hasSpeed) {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  uint32_t now = millis();
  
  // Store Apparent Wind data for True Wind calculation
  if ((strcmp(sentenceType, "MWV") == 0 && reference == 'R') || strcmp(sentenceType, "VWR") == 0) {
    apparent_angle_deg = angle;
    if (hasSpeed) {
      apparent_speed_kn = speed;
    }
    apparent_hasData = true;
    apparent_lastUpdate_ms = now;
    
    // Track source
    if (strcmp(sentenceType, "MWV") == 0) {
      snprintf(apparent_source, sizeof(apparent_source), "MWV(R)");
    } else {
      snprintf(apparent_source, sizeof(apparent_source), "VWR");
    }
  }
  
  // Store True Wind data
  if ((strcmp(sentenceType, "MWV") == 0 && reference == 'T') || strcmp(sentenceType, "VWT") == 0) {
    true_angle_deg = angle;
    if (hasSpeed) {
      true_speed_kn = speed;
    }
    true_hasData = true;
    true_lastUpdate_ms = now;
    
    // Track source
    if (strcmp(sentenceType, "MWV") == 0) {
      snprintf(true_source, sizeof(true_source), "MWV(T)");
    } else {
      snprintf(true_source, sizeof(true_source), "VWT");
    }
  }
  
  // Update displays that match sentence directly
  for (int i = 0; i < 3; i++) {
    if (!displays[i].enabled) continue;
    
    // Check if this sentence matches display's data type
    if (sentenceMatchesDataType(sentenceType, reference, displays[i].dataType)) {
      displays[i].windAngle_deg = angle;
      if (hasSpeed) {
        displays[i].windSpeed_kn = speed;
      }
      displays[i].lastUpdate_ms = now;
    }
  }
  
  // Update lastSentenceType for debugging
  if (reference != '\0') {
    snprintf(lastSentenceType, sizeof(lastSentenceType), "%s(%c)", sentenceType, reference);
  } else {
    snprintf(lastSentenceType, sizeof(lastSentenceType), "%s", sentenceType);
  }
  
  xSemaphoreGive(dataMutex);
  
  // Try to calculate True Wind for displays that need it
  float trueSpeed, trueAngle;
  if (calculateTrueWind(trueSpeed, trueAngle)) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    
    // Store calculated True Wind data
    if (!true_hasData || (now - true_lastUpdate_ms) > 1000) {
      true_speed_kn = trueSpeed;
      true_angle_deg = trueAngle;
      true_hasData = true;
      true_lastUpdate_ms = now;
      snprintf(true_source, sizeof(true_source), "Calculated");
    }
    
    for (int i = 0; i < 3; i++) {
      if (!displays[i].enabled) continue;
      
      // If display wants True Wind but we don't have direct data, use calculated
      if (displays[i].dataType == DATA_TRUE_WIND) {
        // Only update if we don't have recent direct True Wind data
        if ((now - displays[i].lastUpdate_ms) > 1000) {
          displays[i].windAngle_deg = (int)trueAngle;
          displays[i].windSpeed_kn = trueSpeed;
          displays[i].lastUpdate_ms = now;
        }
      }
    }
    xSemaphoreGive(dataMutex);
  }
  
  // Calculate VMG for displays that need it
  float vmg;
  if (calculateVMG(vmg)) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    
    // Store VMG data
    vmg_kn = vmg;
    vmg_hasData = true;
    vmg_lastUpdate_ms = now;
    
    for (int i = 0; i < 3; i++) {
      if (!displays[i].enabled) continue;
      
      if (displays[i].dataType == DATA_VMG) {
        displays[i].windSpeed_kn = vmg;
        displays[i].windAngle_deg = 0; // VMG has no angle
        displays[i].lastUpdate_ms = now;
      }
    }
    xSemaphoreGive(dataMutex);
  }
  
  // Update SOG/COG for displays that need it
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  if (gps_hasSOG || gps_hasCOG) {
    for (int i = 0; i < 3; i++) {
      if (!displays[i].enabled) continue;
      
      if (displays[i].dataType == DATA_SOG && gps_hasSOG) {
        displays[i].windSpeed_kn = gps_sog_kn;
        displays[i].windAngle_deg = 0; // SOG has no angle
        displays[i].lastUpdate_ms = now;
      }
      else if (displays[i].dataType == DATA_COG && gps_hasCOG) {
        displays[i].windSpeed_kn = 0; // COG is angle only
        displays[i].windAngle_deg = (int)gps_cog_deg;
        displays[i].lastUpdate_ms = now;
      }
    }
  }
  xSemaphoreGive(dataMutex);
  
  // Update pulse outputs for all active displays
  updateAllDisplayPulses();
}

/* ========= Wind Calculations ========= */

// Calculate True Wind from Apparent Wind + GPS data
// Returns true if calculation was successful
bool calculateTrueWind(float& trueSpeed, float& trueAngle) {
  // Need: Apparent Wind + SOG + COG (or Heading)
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  bool hasData = apparent_hasData && gps_hasSOG && (gps_hasCOG || gps_hasHeading);
  if (!hasData) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  
  // Check data age (max 5 seconds old)
  uint32_t now = millis();
  if ((now - apparent_lastUpdate_ms) > 5000 || (now - gps_lastUpdate_ms) > 5000) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  
  float aws = apparent_speed_kn;  // Apparent Wind Speed
  float awa = apparent_angle_deg; // Apparent Wind Angle (relative to bow)
  float sog = gps_sog_kn;         // Speed Over Ground
  float cog = gps_hasCOG ? gps_cog_deg : gps_heading_deg; // Course/Heading
  
  xSemaphoreGive(dataMutex);
  
  // Convert to radians
  float awa_rad = awa * DEG_TO_RAD;
  
  // Apparent Wind vector components (relative to boat)
  float aws_x = aws * sin(awa_rad);  // Cross component
  float aws_y = aws * cos(awa_rad);  // Forward component
  
  // Boat velocity vector (subtract from apparent to get true)
  // Boat moves forward, so subtract from forward component
  float tws_x = aws_x;
  float tws_y = aws_y - sog;
  
  // True Wind Speed and Angle
  trueSpeed = sqrt(tws_x * tws_x + tws_y * tws_y);
  trueAngle = atan2(tws_x, tws_y) * RAD_TO_DEG;
  
  // Normalize angle to 0-360
  if (trueAngle < 0) trueAngle += 360;
  
  Serial.printf("Calculated True Wind: AWS=%.1f@%.0f° + SOG=%.1f -> TWS=%.1f@%.0f°\n",
                aws, awa, sog, trueSpeed, trueAngle);
  
  return true;
}

// Calculate VMG (Velocity Made Good)
// Returns true if calculation was successful
bool calculateVMG(float& vmg) {
  // For now, just return SOG (simple VMG without waypoint)
  // Future: VMG = SOG × cos(angle to waypoint)
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  if (!gps_hasSOG) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  
  vmg = gps_sog_kn;
  xSemaphoreGive(dataMutex);
  
  return true;
}

/* ========= NMEA-parsinta ========= */
bool nmeaChecksumOK(const char* s){
  const char* star = strrchr(s, '*');
  if(!star) return true;
  uint8_t cs=0; const char* p = s+1;
  while(p && *p && p<star){ cs ^= (uint8_t)(*p++); }
  if(*(star+1)==0 || *(star+2)==0) return true;
  char hex[3]={star[1], star[2], 0};
  uint8_t want = (uint8_t)strtoul(hex, nullptr, 16);
  return cs==want;
}
int splitCSV(char* line, char* fields[], int maxf){
  int n=0; for(char* p=line; *p && n<maxf; ){
    fields[n++]=p; char* c=strchr(p, ','); if(!c) break; *c=0; p=c+1;
  } return n;
}
bool hasFormatter(const char* s, const char* fmt3){
  const char* p=s; if(*p=='$') p++;
  if(strlen(p)<5) return false;
  // Check for formatter - can be at position 0-2 or 2-4 (for talkers like II, WI, etc)
  // Standard: $IIMWV (pos 2-4) or Yachta: $WIMWV (pos 3-5)
  if (p[2]==fmt3[0] && p[3]==fmt3[1] && p[4]==fmt3[2]) return true;
  if (strlen(p)>=6 && p[3]==fmt3[0] && p[4]==fmt3[1] && p[5]==fmt3[2]) return true;
  return false;
}

bool parseMWV(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<3) return false;
  if(!hasFormatter(line,"MWV")) return false;
  if(!nmeaChecksumOK(line)) return false;
  float ang = atof(f[1]); char ref = toupper((unsigned char)f[2][0]);
  if(ref!='R' && ref!='T') return false;
  if(!(ang>=0 && ang<=360)) return false;
  
  int newAngle = wrap360((int)lroundf(ang));
  float newSpeed = 0.0f;
  bool hasSpeed = false;
  
  if(n>=4) {
    float spd = atof(f[3]);
    if(spd>=0 && spd<200) {
      newSpeed = spd;
      hasSpeed = true;
    }
  }
  
  // Update displays with reference (R=Relative/Apparent, T=True)
  updateDisplaysForSentence("MWV", ref, newAngle, newSpeed, hasSpeed);
  
  if(ref=='R') hasMwvR = true;
  else if(ref=='T') hasMwvT = true;
  return true;
}
bool parseVWR(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<3) return false;
  if(!hasFormatter(line,"VWR")) return false;
  if(!nmeaChecksumOK(line)) return false;
  float ang = atof(f[1]); char side = toupper((unsigned char)f[2][0]);
  if(!(ang>=0 && ang<=180)) return false;
  int awa = (int)lroundf(ang);
  int newAngle = (side=='L') ? wrap360(360-awa) : awa;
  float newSpeed = 0.0f;
  bool hasSpeed = false;
  
  if(n>=4) {
    float spd = atof(f[3]);
    if(spd>=0 && spd<200) {
      newSpeed = spd;
      hasSpeed = true;
    }
  }
  
  // VWR is always Apparent Wind (no reference parameter)
  updateDisplaysForSentence("VWR", '\0', newAngle, newSpeed, hasSpeed);
  
  hasVwr = true;
  return true;
}
// Parse RMC: $GPRMC,time,status,lat,N,lon,E,speed,course,date,magvar,E*checksum
bool parseRMC(char* line){
  char* f[15]; int n = splitCSV(line, f, 15);
  if(n<9) return false;
  if(!hasFormatter(line,"RMC")) return false;
  if(!nmeaChecksumOK(line)) return false;
  
  // f[2] = status (A=valid, V=invalid)
  if(f[2][0] != 'A') return false;
  
  // f[7] = speed over ground (knots)
  // f[8] = course over ground (degrees)
  float sog = atof(f[7]);
  float cog = atof(f[8]);
  
  if(sog >= 0 && sog < 100 && cog >= 0 && cog <= 360) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    gps_sog_kn = sog;
    gps_cog_deg = cog;
    gps_hasSOG = true;
    gps_hasCOG = true;
    gps_lastUpdate_ms = millis();
    xSemaphoreGive(dataMutex);
    
    Serial.printf("GPS RMC: SOG=%.1f kn, COG=%.1f°\n", sog, cog);
    return true;
  }
  return false;
}

// Parse VTG: $GPVTG,cogt,T,cogm,M,sog,N,sogk,K*checksum
bool parseVTG(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<8) return false;
  if(!hasFormatter(line,"VTG")) return false;
  if(!nmeaChecksumOK(line)) return false;
  
  // f[1] = course over ground (true)
  // f[5] = speed over ground (knots)
  float cog = atof(f[1]);
  float sog = atof(f[5]);
  
  if(sog >= 0 && sog < 100 && cog >= 0 && cog <= 360) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    gps_sog_kn = sog;
    gps_cog_deg = cog;
    gps_hasSOG = true;
    gps_hasCOG = true;
    gps_lastUpdate_ms = millis();
    xSemaphoreGive(dataMutex);
    
    Serial.printf("GPS VTG: SOG=%.1f kn, COG=%.1f°\n", sog, cog);
    return true;
  }
  return false;
}

// Parse HDT: $GPHDT,heading,T*checksum
bool parseHDT(char* line){
  char* f[5]; int n = splitCSV(line, f, 5);
  if(n<2) return false;
  if(!hasFormatter(line,"HDT")) return false;
  if(!nmeaChecksumOK(line)) return false;
  
  // f[1] = true heading
  float heading = atof(f[1]);
  
  if(heading >= 0 && heading <= 360) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    gps_heading_deg = heading;
    gps_hasHeading = true;
    gps_lastUpdate_ms = millis();
    xSemaphoreGive(dataMutex);
    
    Serial.printf("GPS HDT: Heading=%.1f°\n", heading);
    return true;
  }
  return false;
}

// Parse HDM: $GPHDM,heading,M*checksum (magnetic heading)
bool parseHDM(char* line){
  char* f[5]; int n = splitCSV(line, f, 5);
  if(n<2) return false;
  if(!hasFormatter(line,"HDM")) return false;
  if(!nmeaChecksumOK(line)) return false;
  
  // f[1] = magnetic heading
  // Note: We store as-is, proper magnetic variation correction would need declination
  float heading = atof(f[1]);
  
  if(heading >= 0 && heading <= 360) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    gps_heading_deg = heading;
    gps_hasHeading = true;
    gps_lastUpdate_ms = millis();
    xSemaphoreGive(dataMutex);
    
    Serial.printf("GPS HDM: Heading=%.1f° (magnetic)\n", heading);
    return true;
  }
  return false;
}

bool parseVWT(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<3) return false;
  if(!hasFormatter(line,"VWT")) return false;
  if(!nmeaChecksumOK(line)) return false;
  float ang = atof(f[1]); char side = toupper((unsigned char)f[2][0]);
  if(!(ang>=0 && ang<=180)) return false;
  int awa = (int)lroundf(ang);
  int newAngle = (side=='L') ? wrap360(360-awa) : awa;
  float newSpeed = 0.0f;
  bool hasSpeed = false;
  
  if(n>=4) {
    float spd = atof(f[3]);
    if(spd>=0 && spd<200) {
      newSpeed = spd;
      hasSpeed = true;
    }
  }
  
  // VWT is always True Wind (no reference parameter)
  updateDisplaysForSentence("VWT", '\0', newAngle, newSpeed, hasSpeed);
  
  hasVwt = true;
  return true;
}
bool parseNMEALine(char* line){
  if(strlen(line)<6 || line[0]!='$') return false;
  static char tmp[256];
  size_t L = min(strlen(line), sizeof(tmp)-1);
  
  // Wind sentences
  memcpy(tmp, line, L); tmp[L]=0;
  if(hasFormatter(tmp,"MWV") && parseMWV(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"VWR") && parseVWR(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"VWT") && parseVWT(tmp)) return true;
  
  // GPS sentences
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"RMC") && parseRMC(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"VTG") && parseVTG(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"HDT") && parseHDT(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"HDM") && parseHDM(tmp)) return true;
  
  return false;
}

/* ========= UDP/TCP BIND & POLL ========= */
void ensureTCPConnected(WiFiClient& client){
  if (client.connected()) return;
  uint32_t now = millis();
  if (now < lastTcpAttempt) return;
  lastTcpAttempt = now + 3000;
  
  Serial.printf("TCP connect to %s:%u...\n", nmeaHost, nmeaPort);
  client.stop();
  client.setTimeout(1000);
  
  if(client.connect(nmeaHost, nmeaPort)) {
    Serial.println("TCP connected! Setting non-blocking mode...");
    client.setTimeout(0);
  } else {
    Serial.println("TCP connect failed");
  }
}


void pollTCP(WiFiClient& client){
  if(!client.connected()) return;

  // Reset sentence flags every 5 seconds
  if(millis() - lastFlagReset > 5000) {
    hasMwvR = false;
    hasMwvT = false;
    hasVwr = false;
    hasVwt = false;
    lastFlagReset = millis();
  }

  // Non-blocking: read only one chunk, not all available
  if(client.available()) {
    // Read one chunk
    size_t n = client.readBytes(netBuf, sizeof(netBuf)-1);
    if(n > 0) {
      netBuf[n] = 0;
      
      // Process chunk: accumulate into line buffer
      for(size_t i = 0; i < n; i++) {
        char c = netBuf[i];
        
        if(c == '\r' || c == '\n') {
          // End of line found
          if(nmeaLineBufLen > 0) {
            nmeaLineBuf[nmeaLineBufLen] = 0;
            
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            strncpy(lastSentenceRaw, nmeaLineBuf, sizeof(lastSentenceRaw) - 1);
            lastSentenceRaw[sizeof(lastSentenceRaw) - 1] = '\0';
            xSemaphoreGive(dataMutex);
            lastNmeaDataMs = millis();
            if(parseNMEALine(nmeaLineBuf)) {
              // Update DAC for all displays that have Logic Wind type
              for (int i = 0; i < 3; i++) {
                if (displays[i].enabled && strcmp(displays[i].type, "logicwind") == 0) {
                  setOutputsDeg(i, 0); // deg parameter not used anymore
                }
              }
            }
            nmeaLineBufLen = 0;
          }
        } else if(nmeaLineBufLen < sizeof(nmeaLineBuf)-1) {
          // Accumulate character
          nmeaLineBuf[nmeaLineBufLen++] = c;
        }
      }
    }
  }
}

void bindTransport(){
  Serial.printf("TCP stream: %s:%u\n", nmeaHost, nmeaPort);
  lastTcpAttempt = 0;
}

void ensureUDPBound() {
  // UDP connection: check if listening on configured UDP port (Profile 2)
  if (udpConnected) return;  // Already bound
  
  uint32_t now = millis();
  if (now < lastUdpAttempt) return;  // Don't retry too often
  lastUdpAttempt = now + 3000;  // Wait 3 seconds between bind attempts
  
  // Get Profile 2 (UDP) port from config
  Preferences p;
  p.begin("cfg", true);
  uint16_t udpPort = p.getUShort("p2_port", 10110);
  p.end();
  
  Serial.printf("UDP bind to port %u...\n", udpPort);
  
  if (udpClient.begin(udpPort)) {
    Serial.printf("UDP bound successfully on port %u\n", udpPort);
    udpConnected = true;
  } else {
    Serial.printf("UDP bind failed on port %u\n", udpPort);
    udpConnected = false;
  }
}

void pollUDP() {
  if (!udpConnected) return;
  
  // Reset sentence flags every 5 seconds (same as TCP)
  if (millis() - lastFlagReset > 5000) {
    hasMwvR = false;
    hasMwvT = false;
    hasVwr = false;
    hasVwt = false;
    lastFlagReset = millis();
  }
  
  // Check for incoming UDP packets
  int packetSize = udpClient.parsePacket();
  if (packetSize > 0) {
    // Read UDP packet
    size_t n = udpClient.read((uint8_t*)udpBuf, sizeof(udpBuf) - 1);
    if (n > 0) {
      udpBuf[n] = 0;
      
      // Process packet: accumulate into line buffer (same as TCP)
      for (size_t i = 0; i < n; i++) {
        char c = udpBuf[i];
        
        if (c == '\r' || c == '\n') {
          // End of line found
          if (nmeaLineBufLen > 0) {
            nmeaLineBuf[nmeaLineBufLen] = 0;
            
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            strncpy(lastSentenceRaw, nmeaLineBuf, sizeof(lastSentenceRaw) - 1);
            lastSentenceRaw[sizeof(lastSentenceRaw) - 1] = '\0';
            xSemaphoreGive(dataMutex);
            
            lastNmeaDataMs = millis();
            if (parseNMEALine(nmeaLineBuf)) {
              // Update DAC for all displays that have Logic Wind type
              for (int i = 0; i < 3; i++) {
                if (displays[i].enabled && strcmp(displays[i].type, "logicwind") == 0) {
                  setOutputsDeg(i, 0); // deg parameter not used anymore
                }
              }
            }
            nmeaLineBufLen = 0;
          }
        } else if (nmeaLineBufLen < sizeof(nmeaLineBuf) - 1) {
          // Accumulate character
          nmeaLineBuf[nmeaLineBufLen++] = c;
        }
      }
    }
  }
}

/* ========= STA-yhteys ========= */
void connectSTA(){
  // Yhdistä vain jos SSID on määritelty
  if (strlen(sta_ssid) == 0) {
    Serial.println("No STA SSID configured, skipping STA connection");
    return;
  }
  
  Serial.printf("Attempting STA connection to SSID='%s' with password='%s'\n", sta_ssid, sta_pass);
  WiFi.begin(sta_ssid, sta_pass);
  Serial.printf("Connecting STA to %s", sta_ssid);
  uint32_t t0=millis();
  while (WiFi.status()!=WL_CONNECTED && millis()-t0<20000){
    Serial.print(".");
    delay(250);
  }
  Serial.println();
  if(WiFi.status()==WL_CONNECTED){
    Serial.print("STA IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("STA not connected (AP still available).");
  }
}

/* ========= Setup & loop ========= */
void setup() {
  Serial.begin(115200);
  delay(500);

  // Initialize FreeRTOS synchronization primitives
  dataMutex = xSemaphoreCreateMutex();
  wifiMutex = xSemaphoreCreateMutex();
  nvsMutex = xSemaphoreCreateMutex();
  pauseAckSemaphore = xSemaphoreCreateBinary();
  
  if (!dataMutex || !wifiMutex || !nvsMutex || !pauseAckSemaphore) {
    Serial.println("FATAL: Failed to create mutexes!");
    while(1) delay(1000);
  }
  Serial.println("Mutexes initialized");

  loadConfig();

  // Initialize WiFi FIRST to reduce power draw during DAC init
  WiFi.mode(WIFI_AP_STA);
  delay(100);
  
  // Varmista että ap_pass ei ole tyhjä
  if (strlen(ap_pass) < 8) {
    strcpy(ap_pass, AP_PASS);
    Serial.printf("AP password was empty, using default: %s\n", ap_pass);
  }
  
  // Start AP early with delay to stabilize
  WiFi.softAP(AP_SSID, ap_pass);
  Serial.printf("AP started: %s with password: %s\n", AP_SSID, ap_pass);
  delay(200);  // Let WiFi stack stabilize
  
  // Now initialize DAC after WiFi is stable
  Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ);
  delay(100);  // Give I2C time to initialize
  
  // Try DAC init with timeout - don't get stuck forever if DAC missing
  int dacTries = 0;
  bool dacReady = false;
  while (dac.begin() != 0 && dacTries < 5) {
    Serial.println("GP8403 init error");
    delay(200);
    dacTries++;
  }
  
  if (dacTries >= 5) {
    Serial.println("GP8403 init FAILED - continuing without DAC");
  } else {
    dac.setDACOutRange(dac.eOutputRange10V);
    dacReady = true;
    Serial.println("GP8403 init OK");
  }
  
  if (dacReady) {
    // Initialize DAC to 0 degrees for display 0
    setOutputsDeg(0, 0);
  }

  // Initialize enabled displays
  for (int i = 0; i < 3; i++) {
    if (displays[i].enabled) {
      startDisplay(i);
    }
  }

  // Käynnistä STA after AP and DAC
  connectSTA();
  
  bindTransport();

  setupWebUI(server);
  
  // Create NMEA polling task on Core 1 BEFORE starting web server
  xTaskCreatePinnedToCore(
    nmeaPollTaskFunc,      // Task function
    "NMEA_Poll",           // Task name
    4096,                  // Stack size (bytes)
    NULL,                  // Parameters
    2,                     // Priority (higher than loop)
    &nmeaPollTask,         // Task handle
    1                      // Core 1 (0=Core 0, 1=Core 1)
  );
  Serial.println("NMEA polling task created");
  
  // Simple toggle endpoints for NMEA processing
  server.on("/unfreeze", HTTP_GET, [](){
    freezeNMEA = false;
    server.send(200, "text/plain", "NMEA processing resumed");
  });
  
  // Add a simple test route to debug web server
  server.on("/test", HTTP_GET, [](){
    Serial.println("DEBUG: /test route called");
    server.send(200, "text/plain", "ESP32 Web Server Working!");
  });
  
  server.begin();
  Serial.println("Web server started on port 80");
  
  Serial.println("Ready.");
}

void loop() {
  // Core 0: Dedicated to web server (NMEA polling now runs on Core 1)
  static uint32_t lastDebug = 0;
  static uint32_t lastTimeoutCheck = 0;
  uint32_t now = millis();
  
  // Check for data timeout every 100ms (ensures speed/direction zero when connection is lost)
  if (now - lastTimeoutCheck > 100) {
    lastTimeoutCheck = now;
    updateAllDisplayPulses();
  }
  
  // Heartbeat every 10 seconds
  if (now - lastDebug > 10000) {
    Serial.printf("Loop: %u ms\n", now);
    lastDebug = now;
  }
  
  server.handleClient();
  
  // Small delay to prevent watchdog and allow other tasks
  delay(1);
}
