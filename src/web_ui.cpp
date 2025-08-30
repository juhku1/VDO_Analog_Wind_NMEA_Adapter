// web_ui.cpp (korjattu)
#include "web_ui.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Sama enum arvoille kuin pääohjelmassa
enum { PROTO_UDP = 0, PROTO_TCP = 1 };

// Pääohjelmasta tarvittavat symbolit
extern Preferences prefs;

extern int pulseDuty; // määritelty wind_project.ino:ssa
// Oletusarvo testiin
// pulseDuty = 20; // Tämä rivi poistetaan, jotta käännösvirhe poistuu

extern uint8_t  nmeaProto;
extern uint16_t nmeaPort;
extern String   nmeaHost;

extern volatile int offsetDeg;
extern const int   VMIN, VCEN, VAMP_BASE, VMAX;
extern const int   gainPct;
extern const bool  invertSin;

extern int     angleDeg;
extern int     lastAngleSent;
extern String  lastSentenceType;
extern String  lastSentenceRaw;
extern volatile bool freezeNMEA;

extern char sta_ssid[33];
extern WiFiClient tcpClient;

// Pääohjelman funktiot
extern void setOutputsDeg(int deg);
extern void bindTransport();
extern void connectSTA();
extern void ensureTCPConnected();

// Yleinen osoitin WebServeriin
static WebServer* g_srv = nullptr;

// ---------- UI-sivu ----------
static const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta charset="UTF-8">
<meta name=viewport content="width=device-width,initial-scale=1">
<title>VDO Logic Wind adapter</title>
<style>
body{font-family:system-ui,Segoe UI,Arial;margin:18px}
.card{max-width:780px;margin:auto;padding:16px;border:1px solid #ddd;border-radius:12px}
h1{font-size:20px;margin:0 0 12px}
.row{display:flex;gap:8px;align-items:center;margin:8px 0;flex-wrap:wrap}
input,button,select{padding:8px 12px;font-size:16px}
label{min-width:140px}
.small{font-size:13px;color:#666}
.badge{display:inline-block;background:#f3f3f3;border:1px solid #ddd;border-radius:8px;padding:4px 8px;margin-left:8px}
.mono{font-family:ui-monospace,Consolas,Monaco,monospace}
fieldset{border:1px solid #ddd;border-radius:10px;padding:10px}
legend{padding:0 6px;color:#444}
.kv{display:flex;gap:8px;align-items:center;margin:6px 0;flex-wrap:wrap}
.kv label{min-width:140px}
footer{position:fixed;left:0;bottom:0;width:100%;background:none;text-align:center;font-size:13px;color:#888;padding:8px 0;z-index:10;}
</style></head><body>
<div class=card>
<h1>VDO Logic Wind adapter</h1>

<div style="margin:28px 0 18px 0;">
  <div style="font-size:1.5em;font-weight:600;color:#1a237e;margin-bottom:10px;">
    Incoming NMEA: <span id=raw class="mono"></span>
  </div>
  <div style="font-size:1.5em;font-weight:600;color:#00695c;">
    Outgoing VDO-angle: <span id=ang></span>°
  </div>

</div>
  <!-- .card content ends here -->
</div>

<footer style="position:fixed;left:0;bottom:0;width:100%;background:none;text-align:center;font-size:13px;color:#888;padding:8px 0;z-index:10;">
  © 2025 Juha-Matti Mäntylä &mdash; Licensed under MIT
</footer>


<div class=row>
<label>Offset (deg)<br><span class="small">If you need adjustment</span></label>
<input id=o type=number min=-180 max=180 step=1>
<button onclick="setOffset()">Set</button>
</div>

<div class=row>
<label>Sumlog K<br><span class="small">Hz per kn (speed calibration)</span></label>
<input id=sumlogK type=number min=0.1 max=10 step=0.01>
<button onclick="setSumlogK()">Set</button>
</div>

<div class=row>
<label>Go to angle<br><span class="small">Test the needle</span></label>
<input id=a type=number min=0 max=359 step=1>
<button onclick="goAngle()">Go</button>
<label><input type=checkbox id=freeze onchange="toggleFreeze()"> Freeze NMEA</label>
</div>

<fieldset>
<legend>Network & NMEA input</legend>
<div class=kv>
  <label>STA SSID<br><span class="small">Connect to WiFi</span></label>
  <input id=ssid type=text maxlength=32 style="min-width:220px">
</div>
<div class=kv>
  <label>STA Password<br><span class="small">WiFi password</span></label>
  <input id=pass type=password maxlength=64 style="min-width:220px">
</div>
<div class=kv>
  <label>Protocol<br><span class="small">UDP or TCP</span></label>
  <select id=proto>
    <option value="udp">UDP (listen)</option>
    <option value="tcp">TCP client</option>
  </select>
</div>
<div class=kv>
  <label>Host (TCP)<br><span class="small">Server address</span></label>
  <input id=host type=text maxlength=64 placeholder="e.g. 192.168.68.50 or hostname" style="min-width:240px">
</div>
<div class=kv>
  <label>Port<br><span class="small">NMEA port</span></label>
  <input id=portIn type=number min=1 max=65535 step=1 style="width:140px">
</div>
<div class=row>
  <button onclick="saveCfg()">Save</button>
  <button onclick="reconnect()">Reconnect STA</button>
  <button onclick="reconnectTCP()">Reconnect TCP</button>
</div>
<div style="margin:18px 0 8px 0;">
  STA IP: <span id=sta_ip></span> • AP SSID: <span id=ap_ssid></span> • AP IP: <span id=ap_ip></span> • TCP: <span id=tcp_state></span>
</div>
</fieldset>


</div>

<script>
let typing = false;
async function refresh(){
  try{
    const r = await fetch('/status'); const j = await r.json();
    if (!typing && document.activeElement !== document.getElementById('o')) {
      document.getElementById('o').value = j.offset;
    }
    if (!typing && document.activeElement !== document.getElementById('sumlogK')) {
      document.getElementById('sumlogK').value = j.sumlogK;
    }
    document.getElementById('raw').textContent     = j.raw || "-";
    document.getElementById('ang').textContent     = j.angle;
    document.getElementById('sta_ip').textContent = j.sta_ip || "-";
    document.getElementById('ap_ssid').textContent= j.ap_ssid || "-";
    document.getElementById('ap_ip').textContent  = j.ap_ip || "-";
    document.getElementById('tcp_state').textContent = j.tcp_connected ? "connected" : "disconnected";
    document.getElementById('freeze').checked      = j.freeze;

    if (document.activeElement !== document.getElementById('ssid'))
      document.getElementById('ssid').value = j.sta_ssid || "";
    if (document.activeElement !== document.getElementById('pass'))
      document.getElementById('pass').value = "";
    if (document.activeElement !== document.getElementById('portIn'))
      document.getElementById('portIn').value = j.port;
    if (document.activeElement !== document.getElementById('proto'))
      document.getElementById('proto').value = (j.proto || "UDP").toLowerCase();
    if (document.activeElement !== document.getElementById('host'))
      document.getElementById('host').value = j.host || "";
  }catch(e){}
}
async function setOffset(){ const v = document.getElementById('o').value;
  await fetch('/trim?offset='+encodeURIComponent(v)); typing=false; setTimeout(refresh,150); }
async function setSumlogK(){ const v = document.getElementById('sumlogK').value;
  await fetch('/sumlogk?val='+encodeURIComponent(v)); setTimeout(refresh,150); }
async function goAngle(){ const v = document.getElementById('a').value;
  await fetch('/goto?deg='+encodeURIComponent(v)); setTimeout(refresh,150); }
async function toggleFreeze(){ const f = document.getElementById('freeze').checked;
  await fetch('/freeze?on='+(f?1:0)); setTimeout(refresh,150); }
async function saveCfg(){
  const ssid  = document.getElementById('ssid').value;
  const pass  = document.getElementById('pass').value;
  const port  = document.getElementById('portIn').value;
  const proto = document.getElementById('proto').value;
  const host  = document.getElementById('host').value;
  const body = new URLSearchParams({ssid, pass, port, proto, host});
  await fetch('/savecfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
  setTimeout(refresh,200);
}
async function reconnect(){ await fetch('/reconnect'); setTimeout(refresh,1000); }
async function reconnectTCP(){ await fetch('/reconnecttcp'); setTimeout(refresh,500); }

window.addEventListener('load', () => {
  const o = document.getElementById('o');
  o.addEventListener('input', () => typing = true);
  o.addEventListener('focus', () => typing = true);
  o.addEventListener('blur',  () => typing = false);
  const k = document.getElementById('sumlogK');
  k.addEventListener('input', () => typing = true);
  k.addEventListener('focus', () => typing = true);
  k.addEventListener('blur',  () => typing = false);

  refresh(); setInterval(refresh,800);
});
</script>
</body></html>
)HTML";

// ---------- HTTP-käsittelijät ----------
// Sumlog-kerroin, pääohjelmasta
extern float sumlog_K;

static void handleRoot(){
  g_srv->send_P(200, "text/html", PAGE);
}
static void handleSumlogK(){
  if (g_srv->hasArg("val")) {
    float v = g_srv->arg("val").toFloat();
    if (v >= 0.1 && v <= 10.0) {
      sumlog_K = v;
      prefs.putFloat("sumlog_K", sumlog_K);
    }
  }
  g_srv->send(200, "text/plain", String("sumlogK=") + sumlog_K);
}
static void handleTrim(){
  if (g_srv->hasArg("offset")){
    int v = g_srv->arg("offset").toInt();
    if (v<-180) v=-180; if (v>180) v=180;
    offsetDeg = v;
    prefs.putInt("offset", offsetDeg);
    setOutputsDeg(angleDeg);
  }
  g_srv->send(200, "text/plain", String("offset=")+offsetDeg);
}
static void handleGoto(){
  if (g_srv->hasArg("deg")){
    int v = g_srv->arg("deg").toInt();
    if (v<0) v=0; if (v>359) v=359;
    angleDeg = v;
    setOutputsDeg(angleDeg);
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

  // talteen NVS:ään
  prefs.putString("sta_ssid", ssid);
  prefs.putString("sta_pass", pass);
  prefs.putUShort("udp_port", newPort);
  prefs.putUChar("proto",    newProto);
  prefs.putString("host",    hostStr);

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
  String j; j.reserve(540);
  j += "{";
  j += "\"angle\":";      j += lastAngleSent;
  j += ",\"offset\":";      j += offsetDeg;
  j += ",\"sumlogK\":";    j += sumlog_K;
  j += ",\"src\":\"";      j += lastSentenceType; j += "\"";
  j += ",\"raw\":\"";      j += rawEsc;  j += "\"";
  j += ",\"port\":";      j += nmeaPort;
  j += ",\"proto\":\"";      j += (nmeaProto==PROTO_TCP?"TCP":"UDP"); j += "\"";
  j += ",\"host\":\"";      j += nmeaHost; j += "\"";
  j += ",\"tcp_connected\":"; j += (tcpClient.connected()?"true":"false");
  j += ",\"sta_ip\":\"";   j += WiFi.localIP().toString(); j += "\"";
  j += ",\"ap_ssid\":\"";  j += WiFi.softAPSSID(); j += "\"";
  j += ",\"ap_ip\":\"";    j += WiFi.softAPIP().toString(); j += "\"";
  j += ",\"sta_ssid\":\""; j += staSsidEsc; j += "\"";
  j += ",\"freeze\":";      j += (freezeNMEA?"true":"false");
  j += "}";
  g_srv->send(200, "application/json", j);
}

void setupWebUI(WebServer& server){
  g_srv = &server;
  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/trim",        HTTP_GET,  handleTrim);
  server.on("/goto",        HTTP_GET,  handleGoto);
  server.on("/freeze",      HTTP_GET,  handleFreeze);
  server.on("/sumlogk",     HTTP_GET,  handleSumlogK);
  server.on("/savecfg",     HTTP_POST, handleSaveCfg);
  server.on("/reconnect",   HTTP_GET,  handleReconnect);
  server.on("/reconnecttcp",HTTP_GET,  handleReconnectTCP);
  server.on("/status",      HTTP_GET,  handleStatus);
}
