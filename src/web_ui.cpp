// web_ui.cpp (Multi-page version)

#include "web_ui.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Yleinen osoitin WebServeriin
static WebServer* g_srv = nullptr;

// ---------- Page handlers (now using web_pages.cpp) ----------

// Page handlers
static void handleHome() {
  g_srv->send(200, "text/html", buildStatusPage());
}

static void handleNetwork() {
  g_srv->send(200, "text/html", buildNetworkPage());
}

static void handleDisplay1() {
  g_srv->send(200, "text/html", buildDisplayPage(1));
}

static void handleDisplay2() {
  g_srv->send(200, "text/html", buildDisplayPage(2));
}

static void handleDisplay3() {
  g_srv->send(200, "text/html", buildDisplayPage(3));
}

// ---------- HTTP-käsittelijät ----------

// API: Unified display endpoint
static void handleDisplayAPI() {
  if (!g_srv->hasArg("num")) {
    g_srv->send(400, "text/plain", "Missing num parameter");
    return;
  }
  
  int displayNum = g_srv->arg("num").toInt();
  if (displayNum < 1 || displayNum > 3) {
    g_srv->send(400, "text/plain", "Invalid display number");
    return;
  }
  
  int arrayIndex = displayNum - 1; // Convert to 0-based array index
  String action = g_srv->arg("action");
  
  if (g_srv->method() == HTTP_GET) {
    if (action == "enabled") {
      // Set enabled state
      if (g_srv->hasArg("val")) {
        bool newEnabled = g_srv->arg("val").toInt() != 0;
        displays[arrayIndex].enabled = newEnabled;
        saveDisplayConfig(arrayIndex);
        
        if (newEnabled) {
          startDisplay(arrayIndex);
        } else {
          stopDisplay(arrayIndex);
        }
      }
      g_srv->send(200, "text/plain", String("enabled=") + (displays[arrayIndex].enabled ? "1" : "0"));
    } else {
      // Return display configuration as JSON
      String json = "{";
      json += "\"enabled\":" + String(displays[arrayIndex].enabled ? "true" : "false");
      json += ",\"type\":\"" + displays[arrayIndex].type + "\"";
      json += ",\"sentence\":\"" + displays[arrayIndex].sentence + "\"";
      json += ",\"offsetDeg\":" + String(displays[arrayIndex].offsetDeg);
      json += ",\"sumlogK\":" + String(displays[arrayIndex].sumlogK);
      json += ",\"sumlogFmax\":" + String(displays[arrayIndex].sumlogFmax);
      json += ",\"pulseDuty\":" + String(displays[arrayIndex].pulseDuty);
      json += ",\"pulsePin\":" + String(displays[arrayIndex].pulsePin);
      json += ",\"gotoAngle\":" + String(displays[arrayIndex].gotoAngle);
      json += ",\"freeze\":" + String(displays[arrayIndex].freeze ? "true" : "false");
      json += ",\"speedFilterAlpha\":" + String(displays[arrayIndex].speedFilterAlpha);
      json += ",\"freqDeadband\":" + String(displays[arrayIndex].freqDeadband);
      json += ",\"maxStepPercent\":" + String(displays[arrayIndex].maxStepPercent);
      json += "}";
      g_srv->send(200, "application/json", json);
    }
  } else if (g_srv->method() == HTTP_POST && action == "save") {
    // Save all display settings
    if (g_srv->hasArg("enabled")) displays[arrayIndex].enabled = g_srv->arg("enabled").toInt() != 0;
    if (g_srv->hasArg("type")) displays[arrayIndex].type = g_srv->arg("type");
    if (g_srv->hasArg("sentence")) displays[arrayIndex].sentence = g_srv->arg("sentence");
    if (g_srv->hasArg("offsetDeg")) displays[arrayIndex].offsetDeg = g_srv->arg("offsetDeg").toInt();
    if (g_srv->hasArg("sumlogK")) displays[arrayIndex].sumlogK = g_srv->arg("sumlogK").toFloat();
    if (g_srv->hasArg("sumlogFmax")) displays[arrayIndex].sumlogFmax = g_srv->arg("sumlogFmax").toInt();
    if (g_srv->hasArg("pulseDuty")) displays[arrayIndex].pulseDuty = g_srv->arg("pulseDuty").toInt();
    if (g_srv->hasArg("pulsePin")) displays[arrayIndex].pulsePin = g_srv->arg("pulsePin").toInt();
    if (g_srv->hasArg("gotoAngle")) displays[arrayIndex].gotoAngle = g_srv->arg("gotoAngle").toInt();
    if (g_srv->hasArg("freeze")) displays[arrayIndex].freeze = g_srv->arg("freeze").toInt() != 0;
    if (g_srv->hasArg("speedFilterAlpha")) displays[arrayIndex].speedFilterAlpha = g_srv->arg("speedFilterAlpha").toFloat();
    if (g_srv->hasArg("freqDeadband")) displays[arrayIndex].freqDeadband = g_srv->arg("freqDeadband").toFloat();
    if (g_srv->hasArg("maxStepPercent")) displays[arrayIndex].maxStepPercent = g_srv->arg("maxStepPercent").toFloat();
    
    saveDisplayConfig(arrayIndex);
    
    // Restart display with new settings and update pulses
    if (displays[arrayIndex].enabled) {
      stopDisplay(arrayIndex);
      startDisplay(arrayIndex);
      updateDisplayPulse(arrayIndex);
    } else {
      stopDisplay(arrayIndex);
    }
    
    g_srv->send(200, "text/plain", "OK");
  }
}

// Legacy handlers removed - use unified Display API instead
static void handlePulsePin1(){
  g_srv->send(200, "text/plain", "Legacy handler - use /api/display?display=0&pulsePin=X");
}
static void handlePulsePin2(){
  g_srv->send(200, "text/plain", "Legacy handler - use /api/display?display=1&pulsePin=X");
}
static void handleSumlogK(){
  g_srv->send(200, "text/plain", "Legacy handler - use /api/display?display=0&sumlogK=X");
}
static void handleSumlogK2(){
  g_srv->send(200, "text/plain", "Legacy handler - use /api/display?display=1&sumlogK=X");
}
static void handleSumlogFmax(){
  g_srv->send(200, "text/plain", "Legacy handler - use /api/display?display=0&sumlogFmax=X");
}
static void handleSumlogFmax2(){
  g_srv->send(200, "text/plain", "Legacy handler - use /api/display?display=1&sumlogFmax=X");
}
static void handlePulseDuty(){
  g_srv->send(200, "text/plain", "Legacy handler - use /api/display?display=0&pulseDuty=X");
}
static void handlePulseDuty2(){
  g_srv->send(200, "text/plain", "Legacy handler - use /api/display?display=1&pulseDuty=X");
}
static void handleTrim(){
  if (g_srv->hasArg("offset")){
    int v = g_srv->arg("offset").toInt();
    if (v<-180) v=-180; if (v>180) v=180;
    offsetDeg = v;
    prefs.putInt("offset", offsetDeg);
  setOutputsDeg(0, angleDeg); // TODO: käytä oikeaa displayNum:ia
  }
  g_srv->send(200, "text/plain", String("offset=")+offsetDeg);
}
static void handleGoto(){
  if (g_srv->hasArg("deg")){
    int v = g_srv->arg("deg").toInt();
    if (v<0) v=0; if (v>359) v=359;
    angleDeg = v;
  setOutputsDeg(0, angleDeg); // TODO: käytä oikeaa displayNum:ia
  }
  g_srv->send(200,"text/plain",String("angle=")+angleDeg);
}
static void handleFreeze(){
  if (g_srv->hasArg("on")){
    int v = g_srv->arg("on").toInt();
    freezeNMEA = (v!=0);
  }
  g_srv->send(200,"text/plain",String("freeze=")+(freezeNMEA?"1":"0"));
}
static void handleSaveCfg(){ // POST: ssid, pass, port, proto, host
  if (g_srv->method() != HTTP_POST){
    g_srv->send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String ssid = g_srv->arg("ssid");
  String pass = g_srv->arg("pass");
  String ap_pass = g_srv->arg("ap_pass");
  String portStr = g_srv->arg("port");
  String protoStr= g_srv->arg("proto");
  String hostStr = g_srv->arg("host");

  uint16_t newPort = nmeaPort;
  if (portStr.length()) {
    long p = portStr.toInt();
    if (p>=1 && p<=65535) newPort = (uint16_t)p;
  }
  uint8_t newProto = (protoStr.equalsIgnoreCase("tcp") ? PROTO_TCP : PROTO_UDP);
  if (hostStr.length()==0) hostStr = nmeaHost; // jätä ennalleen jos tyhjä

  // talleen NVS:ään käyttäen uutta SSID-kohtaista järjestelmää
  if (ssid.length() > 0 && pass.length() >= 0) {
    saveNetworkConfig(ssid.c_str(), pass.c_str());
  }
  
  // Tallenna muut asetukset
  prefs.begin("cfg", false);
  if (ap_pass.length() > 7) prefs.putString("ap_pass", ap_pass);
  prefs.putUShort("udp_port", newPort);
  prefs.putUChar("proto",    newProto);
  prefs.putString("host",    hostStr);
  prefs.end();

  // päivitä RAM-kopiot
  if (ssid.length()) ssid.toCharArray(sta_ssid, 33);
  bool portChanged  = (newPort  != nmeaPort);
  bool protoChanged = (newProto != nmeaProto);
  bool hostChanged  = (hostStr  != nmeaHost);
  nmeaPort  = newPort;
  nmeaProto = newProto;
  nmeaHost  = hostStr;

  if (portChanged || protoChanged || hostChanged) {
    bindTransport(); // ota heti käyttöön
  }

  g_srv->send(200, "text/plain", "OK");
}
static void handleReconnect(){
  WiFi.disconnect(true, false); // pudota STA
  delay(200);
  
  // Varmista että WiFi on dual-mode tilassa
  WiFi.mode(WIFI_AP_STA);
  
  // Käynnistä AP uudelleen
  WiFi.softAP(AP_SSID, ap_pass);
  
  // Yhdistä STA
  connectSTA();
  g_srv->send(200, "text/plain", "reconnecting");
}
static void handleReconnectTCP(){
  tcpClient.stop();
  ensureTCPConnected();
  g_srv->send(200, "text/plain", tcpClient.connected() ? "connected" : "connecting");
}
static void handleStatus(){
  String rawEsc = lastSentenceRaw; rawEsc.replace("\"","\\\"");
  String staSsidEsc = String(sta_ssid); staSsidEsc.replace("\"","\\\"");
  
  // Get AP client count
  uint8_t apClientCount = WiFi.softAPgetStationNum();
  
  String j; j.reserve(600);
  j += "{";
  j += "\"angle\":";      j += lastAngleSent;
  j += ",\"offset\":";      j += offsetDeg;
  j += ",\"speed_kn\":";    j += sumlog_speed_kn;
  j += ",\"displays\":[";
  for (int i = 0; i < 3; i++) {
    if (i > 0) j += ",";
    j += "{\"enabled\":"; j += (displays[i].enabled ? "true" : "false");
    j += ",\"type\":\""; j += displays[i].type; j += "\"";
    j += ",\"sumlogK\":"; j += displays[i].sumlogK;
    j += ",\"sumlogFmax\":"; j += displays[i].sumlogFmax;
    j += ",\"pulseDuty\":"; j += displays[i].pulseDuty;
    j += ",\"pulsePin\":"; j += displays[i].pulsePin;
    j += "}";
  }
  j += "]";
  j += ",\"src\":\"";      j += lastSentenceType; j += "\"";
  j += ",\"raw\":\"";      j += rawEsc;  j += "\"";
  j += ",\"port\":";      j += nmeaPort;
  j += ",\"proto\":\"";      j += (nmeaProto==PROTO_TCP?"TCP":"UDP"); j += "\"";
  j += ",\"host\":\"";      j += nmeaHost; j += "\"";
  j += ",\"tcp_connected\":"; j += (tcpClient.connected()?"true":"false");
  j += ",\"sta_ip\":\"";   j += WiFi.localIP().toString(); j += "\"";
  j += ",\"sta_ssid\":\""; j += staSsidEsc; j += "\"";
  j += ",\"sta_connected\":"; j += (WiFi.status() == WL_CONNECTED ? "true" : "false");
  j += ",\"ap_ssid\":\"";  j += WiFi.softAPSSID(); j += "\"";
  j += ",\"ap_ip\":\"";    j += WiFi.softAPIP().toString(); j += "\"";
  j += ",\"ap_clients\":"; j += apClientCount;
  j += ",\"nmea_data_age\":"; j += (millis() - lastNmeaDataMs);
  j += ",\"freeze\":";      j += (freezeNMEA?"true":"false");
  j += "}";
  g_srv->send(200, "application/json", j);
}

void setupWebUI(WebServer& server){
  g_srv = &server;
  
  // Multi-page handlers
  server.on("/",            HTTP_GET,  handleHome);
  server.on("/network",     HTTP_GET,  handleNetwork);
  server.on("/display1",    HTTP_GET,  handleDisplay1);
  server.on("/display2",    HTTP_GET,  handleDisplay2);
  server.on("/display3",    HTTP_GET,  handleDisplay3);
  
  // Unified display API
  server.on("/api/display", HTTP_GET,  handleDisplayAPI);
  server.on("/api/display", HTTP_POST, handleDisplayAPI);
  
  // Legacy endpoints (keep for backward compatibility)
  server.on("/trim",        HTTP_GET,  handleTrim);
  server.on("/goto",        HTTP_GET,  handleGoto);
  server.on("/freeze",      HTTP_GET,  handleFreeze);
  server.on("/sumlogk",     HTTP_GET,  handleSumlogK);
  server.on("/sumlogk2",    HTTP_GET,  handleSumlogK2);
  server.on("/sumlogfmax",  HTTP_GET,  handleSumlogFmax);
  server.on("/sumlogfmax2", HTTP_GET,  handleSumlogFmax2);
  server.on("/pulseduty",   HTTP_GET,  handlePulseDuty);
  server.on("/pulseduty2",  HTTP_GET,  handlePulseDuty2);
  server.on("/pulsepin",    HTTP_GET,  handlePulsePin1);
  server.on("/pulsepin2",   HTTP_GET,  handlePulsePin2);
  server.on("/savecfg",     HTTP_POST, handleSaveCfg);
  server.on("/reconnect",   HTTP_GET,  handleReconnect);
  server.on("/reconnecttcp",HTTP_GET,  handleReconnectTCP);
  server.on("/status",      HTTP_GET,  handleStatus);
  
  // Legacy display2 endpoints
  server.on("/display2enabled", HTTP_GET, [](void){
    if (g_srv->hasArg("val")) {
      int v = g_srv->arg("val").toInt();
      bool newEnabled = (v != 0);
      
      if (newEnabled != displays[1].enabled) {
        displays[1].enabled = newEnabled;
        saveDisplayConfig(1);
        
        if (newEnabled) {
          startDisplay(1); // Start unified display 1
        } else {
          stopDisplay(1);  // Stop unified display 1
        }
      }
    }
    g_srv->send(200, "text/plain", String("display2_enabled=") + (displays[1].enabled ? "1" : "0"));
  });
  server.on("/display2type", HTTP_GET, [](void){
    if (g_srv->hasArg("val")) {
      String type = g_srv->arg("val");
      if (type == "logicwind" || type == "sumlog") {
        displays[1].type = type;
        saveDisplayConfig(1);
        updateDisplayPulse(1); // Update pulse settings
      }
    }
    g_srv->send(200, "text/plain", String("display2_type=") + displays[1].type);
  });
}
