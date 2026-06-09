// web_ui.cpp (Multi-page version)

#include "web_ui.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Yleinen osoitin WebServeriin
static WebServer* g_srv = nullptr;

// ---------- Page handlers (LITE: single page) ----------

static void handleHome() {
  g_srv->send(200, "text/html", buildSinglePage());
}

// ---------- HTTP-käsittelijät ----------

// LITE Multi: Display API for 3 displays
static void handleDisplayAPI() {
  if (!g_srv->hasArg("num")) {
    g_srv->send(400, "text/plain", "Missing num parameter");
    return;
  }
  
  int displayNum = g_srv->arg("num").toInt();
  if (displayNum < 1 || displayNum > 3) {
    g_srv->send(400, "text/plain", "Invalid display number (1-3)");
    return;
  }
  
  int idx = displayNum - 1;  // Convert to 0-based index
  String action = g_srv->arg("action");
  
  if (g_srv->method() == HTTP_GET) {
    // Return display configuration as JSON
    String json = "{";
    json += "\"enabled\":" + String(speedPulses[idx].enabled ? "true" : "false");
    json += ",\"type\":\"" + String(speedPulses[idx].instrumentType) + "\"";
    json += ",\"speedSource\":" + String(speedPulses[idx].speedSource);
    json += ",\"sumlogK\":" + String(speedPulses[idx].pulsesPerKnot);
    json += ",\"sumlogFmax\":" + String(speedPulses[idx].maxFrequency);
    json += ",\"pulseDuty\":" + String(speedPulses[idx].dutyCycle);
    json += ",\"pulsePin\":" + String(speedPulses[idx].pulsePin);
    json += "}";
    g_srv->send(200, "application/json", json);
  } else if (g_srv->method() == HTTP_POST && action == "save") {
    // Save display settings
    if (g_srv->hasArg("enabled")) speedPulses[idx].enabled = g_srv->arg("enabled").toInt() != 0;
    if (g_srv->hasArg("type")) {
      String typeStr = g_srv->arg("type");
      strncpy(speedPulses[idx].instrumentType, typeStr.c_str(), sizeof(speedPulses[idx].instrumentType) - 1);
      speedPulses[idx].instrumentType[sizeof(speedPulses[idx].instrumentType) - 1] = '\0';
    }
    if (g_srv->hasArg("speedSource")) {
      int src = g_srv->arg("speedSource").toInt();
      if (src >= 0 && src <= 3) {
        speedPulses[idx].speedSource = (uint8_t)src;
      }
    }
    if (g_srv->hasArg("pulsesPerKnot")) speedPulses[idx].pulsesPerKnot = g_srv->arg("pulsesPerKnot").toFloat();
    if (g_srv->hasArg("maxFrequency")) speedPulses[idx].maxFrequency = g_srv->arg("maxFrequency").toInt();
    if (g_srv->hasArg("dutyCycle")) speedPulses[idx].dutyCycle = g_srv->arg("dutyCycle").toInt();
    if (g_srv->hasArg("pulsePin")) speedPulses[idx].pulsePin = g_srv->arg("pulsePin").toInt();
    
    saveSpeedPulseConfig(idx);
    
    // Restart display with new settings
    if (speedPulses[idx].enabled) {
      stopSpeedPulse(idx);
      startSpeedPulse(idx);
      updateSpeedPulse(idx);
    } else {
      stopSpeedPulse(idx);
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
// LITE Multi: Simplified handlers
static void handleTrim(){
  // DEPRECATED in LITE Multi (no per-display offset)
  g_srv->send(200, "text/plain", "offset=0 (deprecated in LITE Multi)");
}
static void handleGoto(){
  if (g_srv->hasArg("deg")){
    int v = g_srv->arg("deg").toInt();
    if (v<0) v=0; if (v>359) v=359;
    
    // Set global angle manually for testing
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    directionAngle = v;
    xSemaphoreGive(dataMutex);
    
    setDirectionOutput(v);
  }
  
  int currentAngle;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  currentAngle = directionAngle;
  xSemaphoreGive(dataMutex);
  
  g_srv->send(200,"text/plain",String("angle=")+currentAngle);
}
static void handleSaveCfg(){ // POST: ssid, pass, ap_pass, p1_name, p1_proto, p1_host, p1_port, p2_name, p2_proto, p2_host, p2_port, wifi_mode, w1_ssid, w1_pass, w2_ssid, w2_pass
  if (g_srv->method() != HTTP_POST){
    g_srv->send(405, "text/plain", "Method Not Allowed");
    return;
  }
  // Support both old (ssid/pass) and new (sta_ssid/sta_pass) parameter names
  String ssid = g_srv->arg("sta_ssid");
  if (ssid.length() == 0) ssid = g_srv->arg("ssid");
  String pass = g_srv->arg("sta_pass");
  if (pass.length() == 0) pass = g_srv->arg("pass");
  String ap_pass = g_srv->arg("ap_pass");
  String wifiModeStr = g_srv->arg("wifi_mode");

  // WiFi settings
  if (ssid.length() > 0 && pass.length() >= 0) {
    saveNetworkConfig(ssid.c_str(), pass.c_str());
  }

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

  // WiFi Settings (single profile only)
  String w1_ssid = g_srv->arg("w1_ssid");
  String w1_pass = g_srv->arg("w1_pass");

  // UDP forward settings
  String fwdEnable = g_srv->arg("fwd_enable");
  String fwdBroadcast = g_srv->arg("fwd_broadcast");
  String fwdHost = g_srv->arg("fwd_host");
  String fwdPort = g_srv->arg("fwd_port");
  String fwdRate = g_srv->arg("fwd_rate");
  String fwdMwv = g_srv->arg("fwd_mwv");
  String fwdVwr = g_srv->arg("fwd_vwr");
  String fwdVwt = g_srv->arg("fwd_vwt");
  String fwdRmc = g_srv->arg("fwd_rmc");
  String fwdVtg = g_srv->arg("fwd_vtg");
  String fwdHdt = g_srv->arg("fwd_hdt");
  String fwdHdm = g_srv->arg("fwd_hdm");

  // Pause NMEA polling task to prevent race condition
  extern volatile bool pauseNmeaPoll;
  pauseNmeaPoll = true;
  vTaskDelay(pdMS_TO_TICKS(150));  // Wait for nmeaPollTask to pause
  
  // Save to NVS
  prefs.begin(NVS_NAMESPACE, false);
  
  
  if (ap_pass.length() > 7) prefs.putString("ap_pass", ap_pass);
  prefs.putUChar("wifi_mode", wifiModeStr.toInt());
  
  // Profile 1 (TCP)
  if (p1_name.length() > 0) prefs.putString("p1_name", p1_name);
  if (p1_proto.length() > 0) {
    uint8_t proto = (p1_proto.equalsIgnoreCase("tcp") ? PROTO_TCP : 
                     p1_proto.equalsIgnoreCase("http") ? PROTO_HTTP : PROTO_UDP);
    prefs.putUChar("p1_proto", proto);
  }
  if (p1_host.length() > 0) prefs.putString("p1_host", p1_host);
  if (p1_port.length() > 0) prefs.putUShort("p1_port", (uint16_t)p1_port.toInt());

  // Profile 2 (UDP)
  if (p2_name.length() > 0) prefs.putString("p2_name", p2_name);
  if (p2_proto.length() > 0) {
    uint8_t proto = (p2_proto.equalsIgnoreCase("tcp") ? PROTO_TCP : 
                     p2_proto.equalsIgnoreCase("http") ? PROTO_HTTP : PROTO_UDP);
    prefs.putUChar("p2_proto", proto);
  }
  if (p2_host.length() > 0) prefs.putString("p2_host", p2_host);
  if (p2_port.length() > 0) prefs.putUShort("p2_port", (uint16_t)p2_port.toInt());

  // WiFi Settings (single profile only)
  if (w1_ssid.length() > 0) prefs.putString("w1_ssid", w1_ssid);
  if (w1_pass.length() > 0) prefs.putString("w1_pass", w1_pass);

  // UDP forward settings
  prefs.putBool("fwd_en", fwdEnable.toInt() != 0);
  prefs.putBool("fwd_bcast", fwdBroadcast.toInt() != 0);
  if (fwdHost.length() > 0) prefs.putString("fwd_host", fwdHost);

  uint16_t outPort = (uint16_t)fwdPort.toInt();
  if (outPort > 0) prefs.putUShort("fwd_port", outPort);

  uint16_t rateMs = (uint16_t)fwdRate.toInt();
  if (rateMs > 2000) rateMs = 2000;
  prefs.putUShort("fwd_rate", rateMs);

  uint32_t fwdMask = 0;
  if (fwdMwv.toInt() != 0) fwdMask |= FWD_MWV;
  if (fwdVwr.toInt() != 0) fwdMask |= FWD_VWR;
  if (fwdVwt.toInt() != 0) fwdMask |= FWD_VWT;
  if (fwdRmc.toInt() != 0) fwdMask |= FWD_RMC;
  if (fwdVtg.toInt() != 0) fwdMask |= FWD_VTG;
  if (fwdHdt.toInt() != 0) fwdMask |= FWD_HDT;
  if (fwdHdm.toInt() != 0) fwdMask |= FWD_HDM;
  prefs.putUInt("fwd_mask", fwdMask);

  // Add to connection history if P1 changed
  if (p1_host.length() > 0 && p1_port.length() > 0) {
    String historyEntry = p1_host + ":" + p1_port;
    String existing = prefs.getString("history_0", "");
    
    // Only add if different from current history_0
    if (existing != historyEntry) {
      // Shift history down: 3->4, 2->3, 1->2, 0->1, new->0
      for (int i = 3; i >= 0; i--) {
        String key = "history_" + String(i);
        String nextKey = "history_" + String(i + 1);
        String val = prefs.getString(key.c_str(), "");
        if (val.length() > 0) {
          prefs.putString(nextKey.c_str(), val);
        }
      }
      prefs.putString("history_0", historyEntry);
    }
  }

  prefs.end();
  
  // Resume NMEA polling
  pauseNmeaPoll = false;
  Serial.println("[handleSaveCfg] NMEA polling resumed");

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
  
  // LITE Multi: Read global angle and display data
  int globalAngle;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  globalAngle = directionAngle;
  xSemaphoreGive(dataMutex);
  
  String j; j.reserve(600);
  j += "{";
  j += "\"globalAngle\":";  j += globalAngle;
  j += ",\"globalDirSource\":"; j += directionSource;
  j += ",\"displays\":[";
  for (int i = 0; i < 3; i++) {
    if (i > 0) j += ",";
    j += "{\"enabled\":"; j += (speedPulses[i].enabled ? "true" : "false");
    j += ",\"type\":\""; j += speedPulses[i].instrumentType; j += "\"";
    j += ",\"speedSource\":"; j += speedPulses[i].speedSource;
    j += ",\"sumlogK\":"; j += speedPulses[i].pulsesPerKnot;
    j += ",\"sumlogFmax\":"; j += speedPulses[i].maxFrequency;
    j += ",\"pulseDuty\":"; j += speedPulses[i].dutyCycle;
    j += ",\"pulsePin\":"; j += speedPulses[i].pulsePin;
    j += "}";
  }
  j += "]";
  j += ",\"src\":\"";      j += lastSentenceType; j += "\"";
  j += ",\"raw\":\"";      j += rawEsc;  j += "\"";
  j += ",\"has_mwv_r\":"; j += (hasMwvR ? "true" : "false");
  j += ",\"has_mwv_t\":"; j += (hasMwvT ? "true" : "false");
  j += ",\"has_vwr\":"; j += (hasVwr ? "true" : "false");
  j += ",\"has_vwt\":"; j += (hasVwt ? "true" : "false");
  j += ",\"tcp_connected\":"; j += (tcpConnected ? "true" : "false");
  j += ",\"udp_connected\":"; j += (udpConnected ? "true" : "false");
  j += ",\"tcp_host\":\""; j += nmeaHost; j += "\"";
  j += ",\"tcp_port\":"; j += nmeaPort;
  j += ",\"udp_port\":"; j += prefs.getUShort("p2_port", 10110);
  j += ",\"last_nmea\":\""; j += rawEsc; j += "\"";
  j += ",\"last_nmea_time\":"; j += lastNmeaDataMs;
  
  // Separate TCP and UDP NMEA sentences
  String tcpEsc = String(lastTcpSentence); tcpEsc.replace("\"","\\\"");
  String udpEsc = String(lastUdpSentence); udpEsc.replace("\"","\\\"");
  uint32_t now = millis();
  j += ",\"last_tcp_nmea\":\""; j += tcpEsc; j += "\"";
  j += ",\"last_tcp_age\":"; j += lastTcpDataMs ? String(now - lastTcpDataMs) : "999999";
  j += ",\"last_udp_nmea\":\""; j += udpEsc; j += "\"";
  j += ",\"last_udp_age\":"; j += lastUdpDataMs ? String(now - lastUdpDataMs) : "999999";
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
  // Show CURRENT values if P1 is active, otherwise show stored values
  uint8_t activeProfile = prefs.getUChar("conn_mode", 0);
  if (activeProfile == 0) {
    j += ",\"p1_host\":\""; j += nmeaHost; j += "\"";
    j += ",\"p1_port\":"; j += nmeaPort;
  } else {
    j += ",\"p1_host\":\""; j += prefs.getString("p1_host", "192.168.68.145"); j += "\"";
    j += ",\"p1_port\":"; j += prefs.getUShort("p1_port", 6666);
  }
  // Always include stored values for editing (separate from display values)
  j += ",\"p1_host_stored\":\""; j += prefs.getString("p1_host", "192.168.68.145"); j += "\"";
  j += ",\"p1_port_stored\":"; j += prefs.getUShort("p1_port", 6666);
  j += ",\"p1_proto_stored\":\"";
  j += (proto1==PROTO_TCP?"tcp":proto1==PROTO_HTTP?"http":"udp");
  j += "\"";
  
  j += ",\"p2_name\":\""; j += prefs.getString("p2_name", "OpenPlotter"); j += "\"";
  j += ",\"p2_proto\":\"";
  uint8_t proto2 = prefs.getUChar("p2_proto", PROTO_UDP);  // P2 defaults to UDP
  j += (proto2==PROTO_TCP?"tcp":proto2==PROTO_HTTP?"http":"udp");
  j += "\"";
  // Show CURRENT values if P2 is active, otherwise show stored values
  if (activeProfile == 1) {
    j += ",\"p2_host\":\""; j += nmeaHost; j += "\"";
    j += ",\"p2_port\":"; j += nmeaPort;
  } else {
    j += ",\"p2_host\":\""; j += prefs.getString("p2_host", ""); j += "\"";
    j += ",\"p2_port\":"; j += prefs.getUShort("p2_port", 10110);
  }
  // Always include stored values for editing (separate from display values)
  j += ",\"p2_host_stored\":\""; j += prefs.getString("p2_host", ""); j += "\"";
  j += ",\"p2_port_stored\":"; j += prefs.getUShort("p2_port", 10110);
  j += ",\"p2_proto_stored\":\"";
  j += (proto2==PROTO_TCP?"tcp":proto2==PROTO_HTTP?"http":"udp");
  j += "\"";
  
  // Connection history (last 5 connections)
  j += ",\"connection_history\":[";
  for (int i = 0; i < 5; i++) {
    String histKey = "history_" + String(i);
    String histVal = prefs.getString(histKey.c_str(), "");
    if (histVal.length() > 0) {
      if (i > 0) j += ",";
      j += "\""; j += histVal; j += "\"";
    }
  }
  j += "]";
  
  j += ",\"tcp_connected\":"; j += (tcpConnected?"true":"false");
  j += ",\"udp_connected\":"; j += (udpConnected?"true":"false");
  j += ",\"sta_ip\":\"";   j += WiFi.localIP().toString(); j += "\"";
  j += ",\"sta_ssid\":\""; j += staSsidEsc; j += "\"";
  j += ",\"sta_connected\":"; j += (WiFi.status() == WL_CONNECTED ? "true" : "false");
  j += ",\"ap_ssid\":\"";  j += WiFi.softAPSSID(); j += "\"";
  j += ",\"ap_ip\":\"";    j += WiFi.softAPIP().toString(); j += "\"";
  j += ",\"ap_clients\":"; j += apClientCount;
  j += ",\"w1_ssid\":\""; j += prefs.getString("w1_ssid", "Kontu"); j += "\"";
  j += ",\"w1_pass\":\""; j += prefs.getString("w1_pass", "8765432A1"); j += "\"";
  j += ",\"w2_ssid\":\""; j += prefs.getString("w2_ssid", ""); j += "\"";
  j += ",\"w2_pass\":\""; j += prefs.getString("w2_pass", ""); j += "\"";
  j += ",\"ap_pass\":\""; j += prefs.getString("ap_pass", "wind12345"); j += "\"";
  j += ",\"nmea_data_age\":"; j += (millis() - lastNmeaDataMs);
  j += ",\"fwd_enable\":"; j += (udpForwardEnabled ? "true" : "false");
  j += ",\"fwd_broadcast\":"; j += (udpForwardBroadcast ? "true" : "false");
  j += ",\"fwd_host\":\""; j += udpForwardHost; j += "\"";
  j += ",\"fwd_port\":"; j += udpForwardPort;
  j += ",\"fwd_rate\":"; j += udpForwardMinIntervalMs;
  j += ",\"fwd_mask\":"; j += udpForwardMask;
  j += ",\"fwd_count\":"; j += udpForwardCount;
  j += ",\"fwd_drop_rate\":"; j += udpForwardDropRate;
  j += ",\"fwd_drop_dup\":"; j += udpForwardDropDup;
  j += ",\"fwd_drop_filter\":"; j += udpForwardDropFilter;
  j += "}";
  g_srv->send(200, "application/json", j);
}

// LITE Plus: Data flow API with multiple sources
static void handleDataFlow() {
  String j;
  j.reserve(800);
  
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    uint32_t now = millis();
    
    j += "{";
    
    // Apparent Wind
    j += "\"apparent\":{";
    j += "\"speed\":"; j += apparent_hasData ? String(apparent_speed_kn, 1) : "0";
    j += ",\"angle\":"; j += apparent_hasData ? String((int)apparent_angle_deg) : "0";
    j += ",\"source\":\""; j += apparent_source; j += "\"";
    j += ",\"hasData\":"; j += apparent_hasData ? "true" : "false";
    j += ",\"age\":"; j += apparent_hasData ? String(now - apparent_lastUpdate_ms) : "999999";
    j += ",\"connection\":\"TCP\"";  // TODO: track actual connection
    j += "}";
    
    // True Wind
    j += ",\"true\":{";
    j += "\"speed\":"; j += true_hasData ? String(true_speed_kn, 1) : "0";
    j += ",\"angle\":"; j += true_hasData ? String((int)true_angle_deg) : "0";
    j += ",\"source\":\""; j += true_source; j += "\"";
    j += ",\"hasData\":"; j += true_hasData ? "true" : "false";
    j += ",\"age\":"; j += true_hasData ? String(now - true_lastUpdate_ms) : "999999";
    j += "}";
    
    // GPS Data (combined)
    j += ",\"gps\":{";
    j += "\"sog\":"; j += gps_hasSOG ? String(gps_sog_kn, 1) : "0";
    j += ",\"cog\":"; j += gps_hasCOG ? String((int)gps_cog_deg) : "0";
    j += ",\"heading\":"; j += gps_hasHeading ? String((int)gps_heading_deg) : "0";
    j += ",\"hasSOG\":"; j += gps_hasSOG ? "true" : "false";
    j += ",\"hasCOG\":"; j += gps_hasCOG ? "true" : "false";
    j += ",\"hasHeading\":"; j += gps_hasHeading ? "true" : "false";
    j += ",\"age\":"; j += (gps_hasSOG || gps_hasCOG) ? String(now - gps_lastUpdate_ms) : "999999";
    j += ",\"connection\":\"UDP\"";  // TODO: track actual connection
    j += "}";
    
    // VMG
    j += ",\"vmg\":{";
    j += "\"speed\":"; j += vmg_hasData ? String(vmg_kn, 1) : "0";
    j += ",\"hasData\":"; j += vmg_hasData ? "true" : "false";
    j += ",\"age\":"; j += vmg_hasData ? String(now - vmg_lastUpdate_ms) : "999999";
    j += "}";
    
    // Display info (LITE Multi)
    j += ",\"globalDirSource\":"; j += directionSource;
    j += ",\"displays\":[";
    for (int i = 0; i < 3; i++) {
      if (i > 0) j += ",";
      j += "{\"enabled\":"; j += speedPulses[i].enabled ? "true" : "false";
      j += ",\"speedSource\":"; j += speedPulses[i].speedSource;
      j += "}";
    }
    j += "]";
    
    j += "}";
    
    xSemaphoreGive(dataMutex);
  } else {
    j = "{\"error\":\"mutex timeout\"}";
  }
  
  g_srv->send(200, "application/json", j);
}

// LITE Multi: Direction source API
static void handleDirectionAPI() {
  if (g_srv->method() == HTTP_POST) {
    bool updated = false;
    
    if (g_srv->hasArg("source")) {
      int src = g_srv->arg("source").toInt();
      if (src >= 0 && src <= 4) {  // 0-4 valid (including VMG)
        directionSource = (uint8_t)src;
        updated = true;
      }
    }
    
    if (g_srv->hasArg("offset")) {
      int offset = g_srv->arg("offset").toInt();
      if (offset >= -180 && offset <= 180) {
        directionOffset = offset;
        updated = true;
      }
    }
    
    if (updated) {
      // Save to NVS
      xSemaphoreTake(nvsMutex, portMAX_DELAY);
      prefs.begin(NVS_NAMESPACE, false);
      prefs.putUChar("global_dir", directionSource);
      prefs.putInt("dir_offset", directionOffset);
      prefs.end();
      xSemaphoreGive(nvsMutex);
      
      g_srv->send(200, "text/plain", "OK");
    } else {
      g_srv->send(400, "text/plain", "Invalid parameters");
    }
  } else {
    // GET: return current direction source and offset
    String json = "{";
    json += "\"source\":" + String(directionSource);
    json += ",\"offset\":" + String(directionOffset);
    json += ",\"angle\":" + String(directionAngle);
    json += "}";
    g_srv->send(200, "application/json", json);
  }
}

void setupWebUI(WebServer& server){
  g_srv = &server;
  
  // LITE Multi: Single-page handler
  server.on("/",            HTTP_GET,  handleHome);
  
  // Speed Pulse API
  server.on("/api/display", HTTP_GET,  handleDisplayAPI);
  server.on("/api/display", HTTP_POST, handleDisplayAPI);
  
  // Direction API
  server.on("/api/direction", HTTP_GET,  handleDirectionAPI);
  server.on("/api/direction", HTTP_POST, handleDirectionAPI);
  
  // Configuration endpoints
  server.on("/trim",        HTTP_GET,  handleTrim);
  server.on("/goto",        HTTP_GET,  handleGoto);
  server.on("/savecfg",     HTTP_POST, handleSaveCfg);
  server.on("/reconnect",   HTTP_GET,  handleReconnect);
  server.on("/status",      HTTP_GET,  handleStatus);
  server.on("/api/dataflow", HTTP_GET,  handleDataFlow);
}
