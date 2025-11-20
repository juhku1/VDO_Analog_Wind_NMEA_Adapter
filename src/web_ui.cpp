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
static void handleSaveCfg(){ // POST: ssid, pass, ap_pass, conn_mode, p1_name, p1_proto, p1_host, p1_port, p2_name, p2_proto, p2_host, p2_port, wifi_mode, w1_ssid, w1_pass, w2_ssid, w2_pass
  if (g_srv->method() != HTTP_POST){
    g_srv->send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String ssid = g_srv->arg("ssid");
  String pass = g_srv->arg("pass");
  String ap_pass = g_srv->arg("ap_pass");
  String connModeStr = g_srv->arg("conn_mode");
  String wifiModeStr = g_srv->arg("wifi_mode");

  // WiFi settings
  if (ssid.length() > 0 && pass.length() >= 0) {
    saveNetworkConfig(ssid.c_str(), pass.c_str());
  }
  
  // Connection profiles
  uint8_t connMode = connModeStr.toInt();
  if (connMode > 1) connMode = 0;

  // WiFi profiles
  uint8_t wifiMode = wifiModeStr.toInt();
  if (wifiMode > 1) wifiMode = 0;

  // Profile 1
  String p1_name = g_srv->arg("p1_name");
  String p1_proto = g_srv->arg("p1_proto");
  String p1_host = g_srv->arg("p1_host");
  String p1_port = g_srv->arg("p1_port");

  // Profile 2
  String p2_name = g_srv->arg("p2_name");
  String p2_proto = g_srv->arg("p2_proto");
  String p2_host = g_srv->arg("p2_host");
  String p2_port = g_srv->arg("p2_port");

  // WiFi Profile 1
  String w1_ssid = g_srv->arg("w1_ssid");
  String w1_pass = g_srv->arg("w1_pass");

  // WiFi Profile 2
  String w2_ssid = g_srv->arg("w2_ssid");
  String w2_pass = g_srv->arg("w2_pass");

  // Save to NVS
  prefs.begin("cfg", false);
  
  if (ap_pass.length() > 7) prefs.putString("ap_pass", ap_pass);
  prefs.putUChar("conn_mode", connMode);
  prefs.putUChar("wifi_mode", wifiMode);
  
  // Profile 1
  if (p1_name.length() > 0) prefs.putString("p1_name", p1_name);
  if (p1_proto.length() > 0) {
    uint8_t proto = (p1_proto.equalsIgnoreCase("tcp") ? PROTO_TCP : 
                     p1_proto.equalsIgnoreCase("http") ? PROTO_HTTP : PROTO_UDP);
    prefs.putUChar("p1_proto", proto);
  }
  if (p1_host.length() > 0) prefs.putString("p1_host", p1_host);
  if (p1_port.length() > 0) prefs.putUShort("p1_port", (uint16_t)p1_port.toInt());

  // Profile 2
  if (p2_name.length() > 0) prefs.putString("p2_name", p2_name);
  if (p2_proto.length() > 0) {
    uint8_t proto = (p2_proto.equalsIgnoreCase("tcp") ? PROTO_TCP : 
                     p2_proto.equalsIgnoreCase("http") ? PROTO_HTTP : PROTO_UDP);
    prefs.putUChar("p2_proto", proto);
  }
  if (p2_host.length() > 0) prefs.putString("p2_host", p2_host);
  if (p2_port.length() > 0) prefs.putUShort("p2_port", (uint16_t)p2_port.toInt());

  // WiFi Profile 1
  if (w1_ssid.length() > 0) prefs.putString("w1_ssid", w1_ssid);
  if (w1_pass.length() > 0) prefs.putString("w1_pass", w1_pass);

  // WiFi Profile 2
  if (w2_ssid.length() > 0) prefs.putString("w2_ssid", w2_ssid);
  if (w2_pass.length() > 0) prefs.putString("w2_pass", w2_pass);

  prefs.end();

  // Reload configuration
  loadConfig();
  bindTransport();

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
  // TCP client is now local to Core 1 task - cannot control from Core 0
  // Just report status
  g_srv->send(200, "text/plain", tcpConnected ? "connected" : "disconnected");
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
  j += ",\"has_mwv_r\":"; j += (hasMwvR ? "true" : "false");
  j += ",\"has_mwv_t\":"; j += (hasMwvT ? "true" : "false");
  j += ",\"has_vwr\":"; j += (hasVwr ? "true" : "false");
  j += ",\"has_vwt\":"; j += (hasVwt ? "true" : "false");
  j += ",\"port\":";      j += nmeaPort;
  j += ",\"proto\":\"";      
  j += (nmeaProto==PROTO_TCP?"TCP":nmeaProto==PROTO_HTTP?"HTTP":"UDP"); 
  j += "\"";
  j += ",\"host\":\"";      j += nmeaHost; j += "\"";
  j += ",\"conn_profile\":\""; j += connProfileName; j += "\"";
  j += ",\"conn_mode\":"; j += prefs.getUChar("conn_mode", 0);
  j += ",\"p1_name\":\""; j += prefs.getString("p1_name", "Yachta"); j += "\"";
  j += ",\"p1_proto\":\"";
  uint8_t proto1 = prefs.getUChar("p1_proto", PROTO_TCP);
  j += (proto1==PROTO_TCP?"tcp":proto1==PROTO_HTTP?"http":"udp");
  j += "\"";
  j += ",\"p1_host\":\""; j += prefs.getString("p1_host", "192.168.68.145"); j += "\"";
  j += ",\"p1_port\":"; j += prefs.getUShort("p1_port", 6666);
  j += ",\"p2_name\":\""; j += prefs.getString("p2_name", "OpenPlotter"); j += "\"";
  j += ",\"p2_proto\":\"";
  uint8_t proto2 = prefs.getUChar("p2_proto", PROTO_TCP);
  j += (proto2==PROTO_TCP?"tcp":proto2==PROTO_HTTP?"http":"udp");
  j += "\"";
  j += ",\"p2_host\":\""; j += prefs.getString("p2_host", ""); j += "\"";
  j += ",\"p2_port\":"; j += prefs.getUShort("p2_port", 10110);
  j += ",\"tcp_connected\":"; j += (tcpConnected?"true":"false");
  j += ",\"sta_ip\":\"";   j += WiFi.localIP().toString(); j += "\"";
  j += ",\"sta_ssid\":\""; j += staSsidEsc; j += "\"";
  j += ",\"sta_connected\":"; j += (WiFi.status() == WL_CONNECTED ? "true" : "false");
  j += ",\"ap_ssid\":\"";  j += WiFi.softAPSSID(); j += "\"";
  j += ",\"ap_ip\":\"";    j += WiFi.softAPIP().toString(); j += "\"";
  j += ",\"ap_clients\":"; j += apClientCount;
  j += ",\"wifi_mode\":"; j += prefs.getUChar("wifi_mode", 0);
  j += ",\"w1_ssid\":\""; j += prefs.getString("w1_ssid", "Kontu"); j += "\"";
  j += ",\"w1_pass\":\""; j += prefs.getString("w1_pass", "8765432A1"); j += "\"";
  j += ",\"w2_ssid\":\""; j += prefs.getString("w2_ssid", ""); j += "\"";
  j += ",\"w2_pass\":\""; j += prefs.getString("w2_pass", ""); j += "\"";
  j += ",\"nmea_data_age\":"; j += (millis() - lastNmeaDataMs);
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
