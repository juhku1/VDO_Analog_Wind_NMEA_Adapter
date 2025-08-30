#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include "DFRobot_GP8403.h"
#include "web_ui.h"

/* ========= 1) VERKOT (STA + AP) ========= */
#define DEFAULT_STA_SSID  "Kontu"
#define DEFAULT_STA_PASS  "8765432A1"
#define AP_SSID           "VDO-Cal"
#define AP_PASS           "wind12345"

/* ========= 2) PROTOKOLLA & ETÄOSOITE =========
   PROTO_UDP: kuunnellaan paikallista UDP-porttia
   PROTO_TCP: toimitaan TCP-ASIAKKAANA (client) ja luetaan NMEA etäpalvelimelta
*/
enum { PROTO_UDP = 0, PROTO_TCP = 1 };
uint8_t  nmeaProto = PROTO_UDP;         // tallennetaan NVS:ään
uint16_t nmeaPort  = 10110;             // tallennetaan NVS:ään
String   nmeaHost  = "192.168.4.2";     // TCP-clientille (NVS)

/* ========= 3) UDP / TCP ========= */
WiFiUDP udp;
WiFiClient tcpClient;
char netBuf[1472];
uint32_t nextTcpAttemptMs = 0;          // uudelleenyritys-ajastus

/* ========= 4) GP8403 / DAC ========= */
#define SDA_PIN   21
#define SCL_PIN   22
#define I2C_ADDR  0x5F
#define I2C_HZ    100000

DFRobot_GP8403 dac(&Wire, I2C_ADDR);

const uint8_t CH_SIN = 0;     // VOUT0 = SIN
const uint8_t CH_COS = 1;     // VOUT1 = COS
const int VMIN = 2000, VCEN = 4000, VAMP_BASE = 2000, VMAX = 6000; // 2..6 V (mV)

/* ========= 5) TILA & TRIMMIT ========= */
Preferences prefs;
volatile int  offsetDeg = 0;      // -180..+180
const int     gainPct  = 100;
const bool    invertSin = false;

int     angleDeg = 0;               // viimeisin kulma (AWA 0..359)
int     lastAngleSent = 0;          // VDO:lle ajettu kulma (offset huomioitu)
String lastSentenceType = "-";    // MWV/VWR/VWT/-
String lastSentenceRaw  = "-";    // viimeisin vastaanotettu rivi
volatile bool freezeNMEA = false;

/* ========= 6) STA-kredut ========= */
char sta_ssid[33] = {0};
char sta_pass[65] = {0};

/* ========= 7) WEB ========= */
WebServer server(80);

/* ---------- apufunktiot ---------- */
static inline int wrap360(int d){ d%=360; if(d<0) d+=360; return d; }
static inline int mvClamp(int mv){ if(mv<VMIN) return VMIN; if(mv>VMAX) return VMAX; return mv; }

void setOutputsDeg(int deg){
  int adj = wrap360(deg + offsetDeg);
  float r = adj * DEG_TO_RAD;
  float s = sinf(r), c = cosf(r);
  if (invertSin) s = -s;
  float amp = VAMP_BASE * (gainPct / 100.0f);
  int sin_mV = mvClamp(VCEN + (int)lroundf(amp * s));
  int cos_mV = mvClamp(VCEN + (int)lroundf(amp * c));
  dac.setDACOutVoltage(sin_mV, CH_SIN);
  dac.setDACOutVoltage(cos_mV, CH_COS);
  lastAngleSent = adj;
}

/* ========= 8) NMEA APURIT ========= */
bool nmeaChecksumOK(const char* s){
  const char* star = strrchr(s, '*');
  if(!star) return true; // sallitaan puuttuva cs
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

/* ========= 9) NMEA PARSINTA ========= */
bool parseMWV(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<3) return false;
  if(!hasFormatter(line,"MWV")) return false;
  if(!nmeaChecksumOK(line)) return false;
  float ang = atof(f[1]); char ref = toupper((unsigned char)f[2][0]);
  if(ref!='R' && ref!='T') return false;
  if(!(ang>=0 && ang<=360)) return false;
  angleDeg = wrap360((int)lroundf(ang));
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

/* ========= 10) UDP/TCP BIND & POLL ========= */
void bindUDP(){
  udp.stop();
  if(udp.begin(nmeaPort)){
    Serial.printf("UDP listening on *:%u\n", nmeaPort);
  } else {
    Serial.println("UDP begin failed!");
  }
}
void ensureTCPConnected(){
  // Yritä yhdistää jos ei yhteyttä ja on aika yrittää
  if (tcpClient.connected()) return;
  uint32_t now = millis();
  if (now < nextTcpAttemptMs) return;
  nextTcpAttemptMs = now + 3000; // uusi yritys 3 s välein
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
    nextTcpAttemptMs = 0; // yritä heti
    ensureTCPConnected();
  }
}

/* ========= 11) ASETUSTEN LATAUS/TALLENNUS ========= */
void loadConfig(){
  prefs.begin("vdo_cal", false);
  offsetDeg = prefs.getInt("offset", 0);
  nmeaPort   = (uint16_t)prefs.getUShort("udp_port", 10110);   // taaksepäin yhteensopiva avain
  nmeaProto  = (uint8_t)prefs.getUChar("proto", PROTO_UDP);
  nmeaHost   = prefs.getString("host", "192.168.4.2");
  String s   = prefs.getString("sta_ssid", DEFAULT_STA_SSID);
  String p   = prefs.getString("sta_pass", DEFAULT_STA_PASS);
  s.toCharArray(sta_ssid, sizeof(sta_ssid));
  p.toCharArray(sta_pass, sizeof(sta_pass));
}
void saveNetConfig(const String& ssid, const String& pass, uint16_t port, uint8_t proto, const String& host){
  prefs.putString("sta_ssid", ssid);
  prefs.putString("sta_pass", pass);
  prefs.putUShort("udp_port", port);
  prefs.putUChar("proto",    proto);
  prefs.putString("host",    host);
}

/* ========= 12) STA-YHTEYS ========= */
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

/* ========= 13) SETUP & LOOP ========= */
void setup() {
  Serial.begin(115200);

  loadConfig();

  // DAC
  Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ);
  while (dac.begin() != 0) { Serial.println("GP8403 init error"); delay(800); }
  dac.setDACOutRange(dac.eOutputRange10V);
  setOutputsDeg(angleDeg);

  // Wi-Fi AP+STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP SSID: %s  IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  connectSTA();

  // Bind valittuun sisääntuloon
  bindTransport();

  // Web-reitit ulkoisesta moduulista
  setupWebUI(server);
  server.begin();

  Serial.printf("HTTP STA http://%s  AP http://%s  %s %s:%u\n",
    WiFi.localIP().toString().c_str(),
    WiFi.softAPIP().toString().c_str(),
    (nmeaProto==PROTO_TCP?"TCP client ->":"UDP listen *:"), 
    (nmeaProto==PROTO_TCP?nmeaHost.c_str():""),
    nmeaPort);
}

void loop() {
  server.handleClient();
  if(!freezeNMEA){
    if(nmeaProto==PROTO_UDP) pollUDP();
    else                    pollTCP();
  }
}
