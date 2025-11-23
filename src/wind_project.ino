#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "DFRobot_GP8403.h"
#include "web_ui.h"
#include "nmea_parser.h"
#include "display_controller.h"
#include "wind_calculations.h"  // LITE Multi: True Wind and VMG calculations

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

// Timing constants (milliseconds) - DAC and DATA_TIMEOUT moved to web_ui.h
constexpr uint32_t GPS_TIMEOUT_MS = 5000;   // GPS data timeout
constexpr uint32_t SENTENCE_WINDOW_MS = 5000; // Sentence type tracking window

/* ========= Global Settings and Variables ========= */

// FreeRTOS synchronization primitives for thread safety
SemaphoreHandle_t dataMutex = NULL;      // Protects wind data (speed, angle)
SemaphoreHandle_t wifiMutex = NULL;      // Protects WiFi operations
SemaphoreHandle_t nvsMutex = NULL;       // Protects NVS (Preferences) access
SemaphoreHandle_t pauseAckSemaphore = NULL;  // For Core 1 pause acknowledgment

Preferences prefs;

// LITE Multi: 3 speed pulse outputs
SpeedPulseConfig speedPulses[3];

// LITE Multi: Global direction (shared by all Logic Wind instruments)
uint8_t directionSource = DATA_APPARENT_WIND;  // Default: Apparent Wind angle
int directionAngle = 0;
int directionOffset = 0;  // Calibration offset (-180 to +180)

// LEDC channels for 3 displays
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

// Separate NMEA tracking for TCP and UDP
char lastTcpSentence[256] = "-";
uint32_t lastTcpDataMs = 0;
char lastUdpSentence[256] = "-";
uint32_t lastUdpDataMs = 0;

// LITE Plus: Multiple data sources - protected by dataMutex

// Apparent Wind data
float apparent_speed_kn = 0.0;
float apparent_angle_deg = 0.0;
bool apparent_hasData = false;
uint32_t apparent_lastUpdate_ms = 0;
char apparent_source[16] = "-";  // "MWV(R)", "VWR", etc.

// True Wind data
float true_speed_kn = 0.0;
float true_angle_deg = 0.0;
bool true_hasData = false;
uint32_t true_lastUpdate_ms = 0;
char true_source[16] = "-";  // "MWV(T)", "VWT", "Calculated"

// VMG data (calculated)
float vmg_kn = 0.0;
bool vmg_hasData = false;
uint32_t vmg_lastUpdate_ms = 0;

// GPS data
float gps_sog_kn = 0.0;        // Speed Over Ground (knots)
float gps_cog_deg = 0.0;       // Course Over Ground (degrees)
float gps_heading_deg = 0.0;   // True Heading (from HDT/HDM)
bool gps_hasSOG = false;
bool gps_hasCOG = false;
bool gps_hasHeading = false;
uint32_t gps_lastUpdate_ms = 0;

#define AP_SSID           "VDO-Cal"
#define AP_PASS           "wind12345"
uint8_t  nmeaProto = PROTO_HTTP;
uint16_t nmeaPort  = 80;
char nmeaHost[64] = "192.168.4.1";

// Persistent TCP client for real-time wind data
WiFiClient tcpClient;
uint32_t lastTcpAttempt = 0;
uint8_t tcpFailCount = 0;  // Count consecutive TCP failures to back off

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

int offsetDeg = 0;
char connProfileName[64] = "Yachta";
bool freezeNMEA = false;

// NMEA sentence type tracking (5s window) - LITE Plus
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
void saveSpeedPulseConfig(int displayNum) {
  xSemaphoreTake(nvsMutex, portMAX_DELAY);
  prefs.begin(NVS_NAMESPACE, false);
  
  if (displayNum == -1) {
    // Save all displays + global direction + offset
    prefs.putUChar("global_dir", directionSource);
    prefs.putInt("dir_offset", directionOffset);
    
    for (int i = 0; i < 3; i++) {
      char key[16];
      snprintf(key, sizeof(key), "d%d_enabled", i);
      prefs.putBool(key, speedPulses[i].enabled);
      snprintf(key, sizeof(key), "d%d_type", i);
      prefs.putString(key, speedPulses[i].instrumentType);
      snprintf(key, sizeof(key), "d%d_speedSrc", i);
      prefs.putUChar(key, speedPulses[i].speedSource);
      snprintf(key, sizeof(key), "d%d_sumlogK", i);
      prefs.putFloat(key, speedPulses[i].pulsesPerKnot);
      snprintf(key, sizeof(key), "d%d_fmax", i);
      prefs.putInt(key, speedPulses[i].maxFrequency);
      snprintf(key, sizeof(key), "d%d_duty", i);
      prefs.putInt(key, speedPulses[i].dutyCycle);
      snprintf(key, sizeof(key), "d%d_pin", i);
      prefs.putInt(key, speedPulses[i].pulsePin);
    }
  } else if (displayNum >= 0 && displayNum < 3) {
    // Save single display
    char key[16];
    snprintf(key, sizeof(key), "d%d_enabled", displayNum);
    prefs.putBool(key, speedPulses[displayNum].enabled);
    snprintf(key, sizeof(key), "d%d_type", displayNum);
    prefs.putString(key, speedPulses[displayNum].instrumentType);
    snprintf(key, sizeof(key), "d%d_speedSrc", displayNum);
    prefs.putUChar(key, speedPulses[displayNum].speedSource);
    snprintf(key, sizeof(key), "d%d_sumlogK", displayNum);
    prefs.putFloat(key, speedPulses[displayNum].pulsesPerKnot);
    snprintf(key, sizeof(key), "d%d_fmax", displayNum);
    prefs.putInt(key, speedPulses[displayNum].maxFrequency);
    snprintf(key, sizeof(key), "d%d_duty", displayNum);
    prefs.putInt(key, speedPulses[displayNum].dutyCycle);
    snprintf(key, sizeof(key), "d%d_pin", displayNum);
    prefs.putInt(key, speedPulses[displayNum].pulsePin);
  }
  
  prefs.end();
  xSemaphoreGive(nvsMutex);
}

void loadConfig(){
  prefs.begin(NVS_NAMESPACE, false);
  
  // LITE Multi: Load global direction source and offset
  directionSource = prefs.getUChar("global_dir", DATA_APPARENT_WIND);
  directionOffset = prefs.getInt("dir_offset", 0);
  
  // LITE Multi: Load 3 displays
  for (int i = 0; i < 3; i++) {
    char key[16];
    
    snprintf(key, sizeof(key), "d%d_enabled", i);
    speedPulses[i].enabled = prefs.getBool(key, i == 0);  // Only display 0 enabled by default
    
    snprintf(key, sizeof(key), "d%d_type", i);
    String typeStr = prefs.getString(key, "sumlog");
    strncpy(speedPulses[i].instrumentType, typeStr.c_str(), sizeof(speedPulses[i].instrumentType) - 1);
    speedPulses[i].instrumentType[sizeof(speedPulses[i].instrumentType) - 1] = '\0';
    
    snprintf(key, sizeof(key), "d%d_speedSrc", i);
    speedPulses[i].speedSource = prefs.getUChar(key, DATA_APPARENT_WIND);
    
    snprintf(key, sizeof(key), "d%d_sumlogK", i);
    speedPulses[i].pulsesPerKnot = prefs.getFloat(key, 1.0f);
    
    snprintf(key, sizeof(key), "d%d_fmax", i);
    speedPulses[i].maxFrequency = prefs.getInt(key, 150);
    
    snprintf(key, sizeof(key), "d%d_duty", i);
    speedPulses[i].dutyCycle = prefs.getInt(key, 10);
    
    snprintf(key, sizeof(key), "d%d_pin", i);
    speedPulses[i].pulsePin = prefs.getInt(key, 12 + i * 2);  // Default: 12, 14, 16
    
    
    // Initialize runtime data
    speedPulses[i].currentSpeed_kn = 0.0f;
    speedPulses[i].lastUpdate_ms = 0;
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

/* ========= UDP/TCP BIND & POLL ========= */
void ensureTCPConnected(WiFiClient& client){
  if (client.connected()) {
    tcpFailCount = 0;  // Reset failure count on successful connection
    return;
  }
  
  uint32_t now = millis();
  if (now < lastTcpAttempt) return;
  
  // Exponential backoff: 5s, 10s, 20s, 30s max
  uint8_t backoffMultiplier = min((uint8_t)tcpFailCount, (uint8_t)2);
  uint32_t backoff = 5000 * (1 << backoffMultiplier);  // 5s, 10s, 20s
  if (backoff > 30000) backoff = 30000;  // Cap at 30s
  lastTcpAttempt = now + backoff;
  
  // Skip TCP connection if host is empty or invalid
  if (strlen(nmeaHost) == 0 || nmeaPort == 0) {
    return;
  }
  
  // Only log first few attempts to avoid spam
  if (tcpFailCount < 3) {
    Serial.printf("TCP connect to %s:%u (attempt %d, next retry in %lus)...\n", 
                  nmeaHost, nmeaPort, tcpFailCount + 1, backoff / 1000);
  }
  
  client.stop();
  
  // Use minimal timeout to prevent blocking
  client.setTimeout(50);  // 50ms timeout for quick failure
  
  // Attempt connection - this may still block briefly
  uint32_t connectStart = millis();
  bool connected = client.connect(nmeaHost, nmeaPort);
  uint32_t connectTime = millis() - connectStart;
  
  if(connected) {
    Serial.printf("TCP connected in %lums! Setting non-blocking mode...\n", connectTime);
    client.setTimeout(0);  // Non-blocking mode for data reading
    tcpFailCount = 0;  // Reset failure count
  } else {
    if (tcpFailCount < 3) {
      Serial.printf("TCP connect failed after %lums\n", connectTime);
    }
    tcpFailCount++;
    if (tcpFailCount > 10) tcpFailCount = 10;  // Cap at 10 to prevent overflow
  }
}


void pollTCP(WiFiClient& client){
  if(!client.connected()) return;

  // Reset sentence flags every 5 seconds (LITE Plus)
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
            strncpy(lastTcpSentence, nmeaLineBuf, sizeof(lastTcpSentence) - 1);
            lastTcpSentence[sizeof(lastTcpSentence) - 1] = '\0';
            xSemaphoreGive(dataMutex);
            lastNmeaDataMs = millis();
            lastTcpDataMs = millis();
            if(parseNMEALine(nmeaLineBuf)) {
              // LITE Multi: Update outputs immediately when new NMEA data arrives
              // This provides faster response than waiting for 50ms loop update
              updateDirectionOutput();
              
              // Update speed pulse outputs for all enabled displays
              for (int i = 0; i < 3; i++) {
                if (speedPulses[i].enabled) {
                  updateSpeedPulseSpeed(i);   // Update speed from selected source
                  updateSpeedPulse(i);         // Update pulse frequency
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
  
  // Reset sentence flags every 5 seconds (LITE: Apparent Wind only)
  if (millis() - lastFlagReset > 5000) {
    hasMwvR = false;
    hasVwr = false;
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
            strncpy(lastUdpSentence, nmeaLineBuf, sizeof(lastUdpSentence) - 1);
            lastUdpSentence[sizeof(lastUdpSentence) - 1] = '\0';
            xSemaphoreGive(dataMutex);
            
            lastNmeaDataMs = millis();
            lastUdpDataMs = millis();
            if (parseNMEALine(nmeaLineBuf)) {
              // LITE Multi: Update outputs immediately when new NMEA data arrives
              // This provides faster response than waiting for 50ms loop update
              updateDirectionOutput();
              
              // Update speed pulse outputs for all enabled displays
              for (int i = 0; i < 3; i++) {
                if (speedPulses[i].enabled) {
                  updateSpeedPulseSpeed(i);   // Update speed from selected source
                  updateSpeedPulse(i);         // Update pulse frequency
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
  while (WiFi.status()!=WL_CONNECTED && millis()-t0<10000){  // Reduced from 20s to 10s
    Serial.print(".");
    delay(250);
  }
  Serial.println();
  if(WiFi.status()==WL_CONNECTED){
    Serial.printf("STA connected in %lums - IP: %s\n", millis()-t0, WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("STA connection failed after %lums (AP still available)\n", millis()-t0);
  }
}

/* ========= Setup & loop ========= */
void setup() {
  Serial.begin(115200);
  delay(500);
  uint32_t setupStart = millis();
  Serial.println("\n\n=== VDO Wind Adapter LITE Multi - Starting ===");

  // Initialize FreeRTOS synchronization primitives
  Serial.printf("[1/8] Initializing mutexes... (t=%lums)\n", millis() - setupStart);
  dataMutex = xSemaphoreCreateMutex();
  wifiMutex = xSemaphoreCreateMutex();
  nvsMutex = xSemaphoreCreateMutex();
  pauseAckSemaphore = xSemaphoreCreateBinary();
  
  if (!dataMutex || !wifiMutex || !nvsMutex || !pauseAckSemaphore) {
    Serial.println("FATAL: Failed to create mutexes!");
    while(1) delay(1000);
  }
  Serial.printf("    Mutexes OK (t=%lums)\n", millis() - setupStart);

  Serial.printf("[2/8] Loading configuration from NVS... (t=%lums)\n", millis() - setupStart);
  loadConfig();
  Serial.printf("    Config loaded (t=%lums)\n", millis() - setupStart);

  // Initialize WiFi FIRST to reduce power draw during DAC init
  Serial.printf("[3/8] Starting WiFi AP... (t=%lums)\n", millis() - setupStart);
  WiFi.mode(WIFI_AP_STA);
  
  // Reduce WiFi TX power during startup to prevent brownout
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Reduce from default 20dBm to 8.5dBm
  delay(100);
  
  // Varmista että ap_pass ei ole tyhjä
  if (strlen(ap_pass) < 8) {
    strcpy(ap_pass, AP_PASS);
    Serial.printf("    AP password was empty, using default: %s\n", ap_pass);
  }
  
  // Start AP early with delay to stabilize
  WiFi.softAP(AP_SSID, ap_pass);
  Serial.printf("    AP started: %s (password: %s) (t=%lums)\n", AP_SSID, ap_pass, millis() - setupStart);
  delay(200);  // Let WiFi stack stabilize
  
  // Now initialize DAC after WiFi is stable
  Serial.printf("[4/8] Initializing I2C and DAC... (t=%lums)\n", millis() - setupStart);
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
    Serial.println("    GP8403 init FAILED - continuing without DAC");
  } else {
    dac.setDACOutRange(dac.eOutputRange10V);
    dacReady = true;
    Serial.println("    GP8403 init OK");
  }
  
  // Add delay after DAC init to stabilize power before continuing
  delay(100);
  
  if (dacReady) {
    // LITE Multi: Initialize DAC to 0 degrees
    setDirectionOutput(0);
  }

  // LITE Multi: Initialize enabled displays
  Serial.printf("[5/8] Initializing speed pulse outputs... (t=%lums)\n", millis() - setupStart);
  for (int i = 0; i < 3; i++) {
    if (speedPulses[i].enabled) {
      startSpeedPulse(i);
      Serial.printf("    Pulse %d started on pin %d\n", i+1, speedPulses[i].pulsePin);
    }
  }

  // Käynnistä STA after AP and DAC
  Serial.printf("[6/8] Connecting to WiFi station... (t=%lums)\n", millis() - setupStart);
  connectSTA();
  Serial.printf("    WiFi STA complete (t=%lums)\n", millis() - setupStart);
  
  Serial.printf("[7/8] Binding network transports (TCP/UDP)... (t=%lums)\n", millis() - setupStart);
  bindTransport();
  Serial.printf("    Transports bound (t=%lums)\n", millis() - setupStart);

  Serial.printf("[8/8] Starting web server... (t=%lums)\n", millis() - setupStart);
  setupWebUI(server);
  Serial.printf("    Web server started (t=%lums)\n", millis() - setupStart);
  
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
  Serial.printf("    NMEA polling task created on Core 1 (t=%lums)\n", millis() - setupStart);
  
  // Restore normal WiFi power after startup (brownout risk is lower now)
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Restore to near-max power for better range
  Serial.println("    WiFi power restored to normal");
  
  Serial.printf("\n=== Setup complete - System ready (total: %lums) ===\n\n", millis() - setupStart);
  
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
  static uint32_t loopCount = 0;
  uint32_t now = millis();
  loopCount++;
  
  // Check for data timeout every 50ms (ensures speed/direction zero when connection is lost)
  // Faster update rate (20 Hz) for more responsive wind direction display
  if (now - lastTimeoutCheck > 50) {
    lastTimeoutCheck = now;
    updateCalculations();         // LITE Multi: calculate True Wind and VMG
    updateDirectionOutput();      // LITE Multi: update global direction
    for (int i = 0; i < 3; i++) {
      if (speedPulses[i].enabled) {
        updateSpeedPulseSpeed(i);   // Update speed from selected source
        updateSpeedPulse(i);         // Update pulse output
      }
    }
  }
  
  // Heartbeat every 10 seconds
  if (now - lastDebug > 10000) {
    Serial.printf("Loop: %u ms (iterations: %u, rate: %.1f Hz)\n", now, loopCount, loopCount / 10.0);
    loopCount = 0;
    lastDebug = now;
  }
  
  server.handleClient();
  
  // Small delay to prevent watchdog and allow other tasks
  delay(1);
}
