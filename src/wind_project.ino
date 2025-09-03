#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include "DFRobot_GP8403.h"
#include "web_ui.h"

// LEDC for hardware PWM pulse generation
#define LEDC_TIMER_RESOLUTION    10
#define LEDC_BASE_FREQ           5000

/* ========= Globaalit asetukset ja muuttujat ========= */
Preferences prefs;

// Unified display array (3 displays)
DisplayConfig displays[3];

// LEDC channels for each display (0-2) with separate timers
const uint8_t LEDC_CHANNELS[3] = {0, 1, 2};
const uint8_t LEDC_TIMERS[3] = {0, 1, 2};  // Separate timer for each channel
bool ledcActive[3] = {false, false, false};
uint32_t lastFreq[3] = {0, 0, 0};  // Track last frequency to avoid unnecessary changes

float sumlog_speed_kn = 0.0;  // Nopeus solmuina

// Smooth frequency transition timer
hw_timer_t * smoothTimer = NULL;
volatile bool needSmoothUpdate = false;

#define DEFAULT_STA_SSID  "Kontu"
#define DEFAULT_STA_PASS  "8765432A1"
#define AP_SSID           "VDO-Cal"
#define AP_PASS           "wind12345"
uint8_t  nmeaProto = PROTO_UDP;
uint16_t nmeaPort  = 10110;
String   nmeaHost  = "192.168.4.2";

WiFiUDP udp;
WiFiClient tcpClient;
char netBuf[1472];
uint32_t nextTcpAttemptMs = 0;

#define SDA_PIN   21
#define SCL_PIN   22
#define I2C_ADDR  0x5F
#define I2C_HZ    100000

DFRobot_GP8403 dac(&Wire, I2C_ADDR);

const uint8_t CH_SIN = 0;
const uint8_t CH_COS = 1;
const int VMIN = 2000, VCEN = 4000, VAMP_BASE = 2000, VMAX = 6000;

int  offsetDeg = 0;
int  angleDeg = 0;
int  lastAngleSent = 0;
String lastSentenceType = "-";
String lastSentenceRaw  = "-";
bool freezeNMEA = false;

char sta_ssid[33] = {0};
char sta_pass[65] = {0};

WebServer server(80);

/* ========= Asetusten tallennus ========= */
void saveDisplayConfig(int displayNum = -1) {
  prefs.begin("cfg", false);
  
  if (displayNum == -1) {
    // Tallenna kaikki displayt
    for (int i = 0; i < 3; i++) {
      String prefix = "d" + String(i) + "_";
      prefs.putBool((prefix + "enabled").c_str(), displays[i].enabled);
      prefs.putString((prefix + "type").c_str(), displays[i].type);
      prefs.putString((prefix + "sentence").c_str(), displays[i].sentence);
      prefs.putInt((prefix + "offset").c_str(), displays[i].offsetDeg);
      prefs.putFloat((prefix + "sumlogK").c_str(), displays[i].sumlogK);
      prefs.putInt((prefix + "sumlogFmax").c_str(), displays[i].sumlogFmax);
      prefs.putInt((prefix + "pulseDuty").c_str(), displays[i].pulseDuty);
      prefs.putInt((prefix + "pulsePin").c_str(), displays[i].pulsePin);
      prefs.putInt((prefix + "gotoAngle").c_str(), displays[i].gotoAngle);
      prefs.putBool((prefix + "freeze").c_str(), displays[i].freeze);
    }
  } else if (displayNum >= 0 && displayNum < 3) {
    // Tallenna vain yksi display
    String prefix = "d" + String(displayNum) + "_";
    prefs.putBool((prefix + "enabled").c_str(), displays[displayNum].enabled);
    prefs.putString((prefix + "type").c_str(), displays[displayNum].type);
    prefs.putString((prefix + "sentence").c_str(), displays[displayNum].sentence);
    prefs.putInt((prefix + "offset").c_str(), displays[displayNum].offsetDeg);
    prefs.putFloat((prefix + "sumlogK").c_str(), displays[displayNum].sumlogK);
    prefs.putInt((prefix + "sumlogFmax").c_str(), displays[displayNum].sumlogFmax);
    prefs.putInt((prefix + "pulseDuty").c_str(), displays[displayNum].pulseDuty);
    prefs.putInt((prefix + "pulsePin").c_str(), displays[displayNum].pulsePin);
    prefs.putInt((prefix + "gotoAngle").c_str(), displays[displayNum].gotoAngle);
    prefs.putBool((prefix + "freeze").c_str(), displays[displayNum].freeze);
  }
  
  prefs.end();
}

void loadConfig(){
  prefs.begin("cfg", false);
  
  // Initialize displays with defaults
  for (int i = 0; i < 3; i++) {
    String prefix = "d" + String(i) + "_";
    displays[i].enabled = prefs.getBool((prefix + "enabled").c_str(), i == 0); // Display 0 enabled by default
    displays[i].type = prefs.getString((prefix + "type").c_str(), "sumlog");
    displays[i].sentence = prefs.getString((prefix + "sentence").c_str(), "MWV");
    displays[i].offsetDeg = prefs.getInt((prefix + "offset").c_str(), 0);
    displays[i].sumlogK = prefs.getFloat((prefix + "sumlogK").c_str(), 1.0f);
    displays[i].sumlogFmax = prefs.getInt((prefix + "sumlogFmax").c_str(), 150);
    displays[i].pulseDuty = prefs.getInt((prefix + "pulseDuty").c_str(), 10);
    displays[i].pulsePin = prefs.getInt((prefix + "pulsePin").c_str(), 12 + i * 2); // Default pins: 12, 14, 16
    displays[i].gotoAngle = prefs.getInt((prefix + "gotoAngle").c_str(), 0);
    displays[i].freeze = prefs.getBool((prefix + "freeze").c_str(), false);
  }
  
  offsetDeg        = prefs.getInt("offset", 0);
  nmeaPort         = (uint16_t)prefs.getUShort("udp_port", 10110);
  nmeaProto        = (uint8_t)prefs.getUChar("proto", PROTO_UDP);
  nmeaHost         = prefs.getString("host", "192.168.4.2");
  String s         = prefs.getString("sta_ssid", DEFAULT_STA_SSID);
  String p         = prefs.getString("sta_pass", DEFAULT_STA_PASS);
  s.toCharArray(sta_ssid, sizeof(sta_ssid));
  p.toCharArray(sta_pass, sizeof(sta_pass));
  prefs.end();
}

/* ========= DAC ulostulo ========= */
static inline int wrap360(int d){ d%=360; if(d<0) d+=360; return d; }
static inline int mvClamp(int mv){ if(mv<VMIN) return VMIN; if(mv>VMAX) return VMAX; return mv; }

void setOutputsDeg(int deg){
  int adj = wrap360(deg + offsetDeg);
  float r = adj * DEG_TO_RAD;
  float s = sinf(r), c = cosf(r);
  float amp = VAMP_BASE;
  int sin_mV = mvClamp(VCEN + (int)lroundf(amp * s));
  int cos_mV = mvClamp(VCEN + (int)lroundf(amp * c));
  dac.setDACOutVoltage(sin_mV, CH_SIN);
  dac.setDACOutVoltage(cos_mV, CH_COS);
  lastAngleSent = adj;
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
    
    Serial.println("Display " + String(displayNum) + " LEDC käynnistetty, timer=" + String(LEDC_TIMERS[displayNum]));
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
    Serial.println("Display " + String(displayNum) + " LEDC pysäytetty");
  }
}

void updateDisplayPulse(int displayNum) {
  if (displayNum < 0 || displayNum >= 3 || !ledcActive[displayNum]) return;
  
  DisplayConfig &disp = displays[displayNum];
  
  if (disp.type == "sumlog") {
    // Sumlog pulse calculation
    float freq = sumlog_speed_kn * disp.sumlogK;
    if (freq > (float)disp.sumlogFmax) freq = (float)disp.sumlogFmax;
    
    if (freq < 0.01f) {
      // Stop PWM when frequency too low
      if (lastFreq[displayNum] != 0) {
        ledcWrite(LEDC_CHANNELS[displayNum], 0);
        lastFreq[displayNum] = 0;
        Serial.println("Display " + String(displayNum) + " stopped (freq too low)");
      }
    } else {
      // Round frequency to reduce jitter
      uint32_t freqInt = (uint32_t)(freq + 0.5f);
      
      // Only update if frequency actually changed
      if (freqInt != lastFreq[displayNum]) {
        // Smooth frequency transition: step max 20% per update
        uint32_t currentFreq = lastFreq[displayNum];
        if (currentFreq > 0) {
          int32_t diff = (int32_t)freqInt - (int32_t)currentFreq;
          int32_t maxStep = max(1, (int32_t)(currentFreq * 0.2f)); // 20% step limit
          
          if (abs(diff) > maxStep) {
            freqInt = currentFreq + (diff > 0 ? maxStep : -maxStep);
          }
        }
        
        // Calculate duty cycle (0-1023 for 10-bit resolution)
        uint32_t duty = (uint32_t)((1023 * disp.pulseDuty) / 100);
        
        // Set frequency and duty cycle
        ledcChangeFrequency(LEDC_CHANNELS[displayNum], freqInt, LEDC_TIMER_RESOLUTION);
        ledcWrite(LEDC_CHANNELS[displayNum], duty);
        
        lastFreq[displayNum] = freqInt;
        Serial.println("Display " + String(displayNum) + " freq=" + String(freqInt) + "Hz, duty=" + String(disp.pulseDuty) + "%");
      }
    }
  } else {
    // Logic wind or other types - no pulse, stop PWM
    if (lastFreq[displayNum] != 0) {
      ledcWrite(LEDC_CHANNELS[displayNum], 0);
      lastFreq[displayNum] = 0;
    }
  }
}

// Timer interrupt for smooth frequency transitions
void IRAM_ATTR onSmoothTimer() {
  needSmoothUpdate = true;
}

// Update all active displays (immediate response for accurate measurement)
void updateAllDisplayPulses() {
  for (int i = 0; i < 3; i++) {
    if (ledcActive[i]) {
      updateDisplayPulse(i);
    }
  }
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
  return (p[2]==fmt3[0] && p[3]==fmt3[1] && p[4]==fmt3[2]);
}

bool parseMWV(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<3) return false;
  if(!hasFormatter(line,"MWV")) return false;
  if(!nmeaChecksumOK(line)) return false;
  float ang = atof(f[1]); char ref = toupper((unsigned char)f[2][0]);
  if(ref!='R' && ref!='T') return false;
  if(!(ang>=0 && ang<=360)) return false;
  angleDeg = wrap360((int)lroundf(ang));
  if(n>=4) {
    float spd = atof(f[3]);
    if(spd>=0 && spd<200) {
      sumlog_speed_kn = spd;
      updateAllDisplayPulses();
    }
  }
  lastSentenceType = String("MWV(")+ref+")";
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
  angleDeg = (side=='L') ? wrap360(360-awa) : awa;
  if(n>=4) {
    float spd = atof(f[3]);
    if(spd>=0 && spd<200) {
      sumlog_speed_kn = spd;
      updateAllDisplayPulses();
    }
  }
  lastSentenceType = "VWR";
  return true;
}
bool parseVWT(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<3) return false;
  if(!hasFormatter(line,"VWT")) return false;
  if(!nmeaChecksumOK(line)) return false;
  float ang = atof(f[1]); char side = toupper((unsigned char)f[2][0]);
  if(!(ang>=0 && ang<=180)) return false;
  int awa = (int)lroundf(ang);
  angleDeg = (side=='L') ? wrap360(360-awa) : awa;
  if(n>=4) {
    float spd = atof(f[3]);
    if(spd>=0 && spd<200) {
      sumlog_speed_kn = spd;
      updateAllDisplayPulses();
    }
  }
  lastSentenceType = "VWT";
  return true;
}
bool parseNMEALine(char* line){
  if(strlen(line)<6 || line[0]!='$') return false;
  static char tmp[256];
  size_t L = min(strlen(line), sizeof(tmp)-1);
  memcpy(tmp, line, L); tmp[L]=0;
  if(hasFormatter(tmp,"MWV") && parseMWV(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"VWR") && parseVWR(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"VWT") && parseVWT(tmp)) return true;
  return false;
}

/* ========= UDP/TCP BIND & POLL ========= */
void bindUDP(){
  udp.stop();
  if(udp.begin(nmeaPort)){
    Serial.printf("UDP listening on *:%u\n", nmeaPort);
  } else {
    Serial.println("UDP begin failed!");
  }
}
void ensureTCPConnected(){
  if (tcpClient.connected()) return;
  uint32_t now = millis();
  if (now < nextTcpAttemptMs) return;
  nextTcpAttemptMs = now + 3000;
  Serial.printf("TCP connect to %s:%u ...\n", nmeaHost.c_str(), nmeaPort);
  tcpClient.stop();
  tcpClient.setTimeout(1000);
  if (tcpClient.connect(nmeaHost.c_str(), nmeaPort)) {
    tcpClient.setNoDelay(true);
    Serial.println("TCP connected.");
  } else {
    Serial.println("TCP connect failed.");
  }
}

void pollUDP(){
  int pktsz = udp.parsePacket();
  if(pktsz<=0) return;
  int n = udp.read(netBuf, sizeof(netBuf)-1);
  if(n<=0) return;
  netBuf[n]=0;
  char* s = netBuf;
  while(s && *s){
    char* e = strpbrk(s, "\r\n");
    if(e) *e=0;
    if(*s){
      lastSentenceRaw = String(s);
      if(parseNMEALine(s)) setOutputsDeg(angleDeg);
    }
    if(!e) break;
    s = e+1;
  }
}

void pollTCP(){
  ensureTCPConnected();
  if(!tcpClient.connected()) return;

  while(tcpClient.available()){
    size_t n = tcpClient.readBytes(netBuf, sizeof(netBuf)-1);
    if(n==0) break;
    netBuf[n]=0;
    char* s = netBuf;
    while(s && *s){
      char* e = strpbrk(s, "\r\n");
      if(e) *e=0;
      if(*s){
        lastSentenceRaw = String(s);
        if(parseNMEALine(s)) setOutputsDeg(angleDeg);
      }
      if(!e) break;
      s = e+1;
    }
  }
}

void bindTransport(){
  if(nmeaProto == PROTO_UDP){
    bindUDP();
    tcpClient.stop();
  } else {
    udp.stop();
    nextTcpAttemptMs = 0;
    ensureTCPConnected();
  }
}

/* ========= STA-yhteys ========= */
void connectSTA(){
  WiFi.begin(sta_ssid, sta_pass);
  Serial.printf("Connecting STA to %s", sta_ssid);
  uint32_t t0=millis();
  while (WiFi.status()!=WL_CONNECTED && millis()-t0<20000){
    Serial.print(".");
    delay(500);
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

  loadConfig();

  // Setup smooth transition timer (50ms intervals)
  smoothTimer = timerBegin(0, 80, true);  // Timer 0, prescaler 80 (1MHz), count up
  timerAttachInterrupt(smoothTimer, &onSmoothTimer, true);
  timerAlarmWrite(smoothTimer, 50000, true);  // 50ms = 50000 microseconds
  timerAlarmEnable(smoothTimer);

  Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ);
  while (dac.begin() != 0) { Serial.println("GP8403 init error"); delay(800); }
  dac.setDACOutRange(dac.eOutputRange10V);
  setOutputsDeg(angleDeg);

  // Initialize enabled displays
  for (int i = 0; i < 3; i++) {
    if (displays[i].enabled) {
      startDisplay(i);
    }
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);

  connectSTA();
  bindTransport();

  setupWebUI(server);
  server.begin();

  Serial.println("Ready.");
}

void loop() {
  server.handleClient();
  if(!freezeNMEA){
    if(nmeaProto==PROTO_UDP) pollUDP();
    else                    pollTCP();
  }
  
  // Handle smooth frequency transitions
  if (needSmoothUpdate) {
    needSmoothUpdate = false;
    updateAllDisplayPulses();
  }
  
  // Pulssit tuotetaan LEDC-hardwarella
}
