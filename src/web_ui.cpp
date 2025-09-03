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
extern int sumlog_fmax; // määritelty wind_project.ino:ssa
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
body {
  font-family: system-ui,Segoe UI,Arial;
  margin: 0;
  background: #eaf6fb;
}
.card {
  max-width: 780px;
  margin: 18px auto;
  padding: 8px;
  background: none;
}
fieldset {
  background: #fff;
  border: 1px solid #ddd;
  border-radius: 14px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.04);
  padding: 10px 8px;
  margin-bottom: 18px;
}
legend {
  padding: 0 10px;
  color: #444;
  font-weight: 500;
  font-size: 20px;
}
.row, .kv {
  display: flex;
  gap: 6px;
  align-items: center;
  margin: 6px 0;
  flex-wrap: wrap;
}
input,button,select {
  padding: 6px 8px;
  font-size: 15px;
}
label {
  min-width: 120px;
}
.small {
  font-size: 12px;
  color: #666;
}
.badge {
  display: inline-block;
  background: #eaf6fb;
  border: 1px solid #ddd;
  border-radius: 8px;
  padding: 4px 8px;
  margin-left: 8px;
}
.mono {
  font-family: ui-monospace,Consolas,Monaco,monospace;
}
footer {
  position: fixed;
  left: 0;
  bottom: 0;
  width: 100%;
  background: none;
  text-align: center;
  font-size: 13px;
  color: #888;
  padding: 8px 0;
  z-index: 10;
}
@media (max-width: 600px) {
  .card {
    margin-left: 12px;
    margin-right: 12px;
    padding: 2px;
  }
  fieldset {padding: 6px 2px; margin-bottom: 12px;}
  .row, .kv {gap: 4px; margin: 4px 0;}
  label {min-width: 90px;}
  input,button,select {font-size: 14px; padding: 5px 6px;}
}
</style>
<div class="card">


<h1 style="text-align:center; margin-bottom:2px;">VDO Wind Adapter</h1>
<div style="text-align:center; margin-top:-8px; margin-bottom:12px;"><span class="small">Bring your analog sensor into the digital age</span></div>


<!-- Incoming & Outgoing Data Info -->
<fieldset>
  <legend>Incoming & Outgoing Data</legend>
  <div class=row>
    <label>Incoming NMEA:</label>
    <span id=raw class="mono"></span>
  </div>
  <div class=row>
    <label>Outgoing angle:</label>
    <span id=ang></span>°
  </div>
</fieldset>

<!-- Display 1 Settings -->
<fieldset>
  <legend>Display 1</legend>
  <div class=row>
    <label>Meter type</label>
    <select id=display1_type onchange="updateDisplay1Fields()">
      <option value="logicwind">Logic Wind</option>
      <option value="sumlog">Sumlog</option>
    </select>
  </div>
  <div class=row>
    <label>NMEA sentence</label>
    <select id=display1_sentence>
      <option value="MWV">MWV</option>
      <option value="VWR">VWR</option>
      <option value="VWT">VWT</option>
    </select>
  </div>
  <div id="display1_logicwind_fields">
    <div class=row>
      <label>Adjustment (deg)</label>
      <input id=o type=number min=-180 max=180 step=1>
      <button onclick="setOffset()">Set</button>
    </div>
    <div class=row>
      <label>Pulse per knot</label>
      <input id=sumlogK type=number min=0.1 max=10 step=0.01>
      <button onclick="setSumlogK()">Set</button>
    </div>
    <div class=row>
      <label>Max pulse frequency (Hz)</span></label>
      <input id=sumlogFmax type=number min=10 max=500 step=1>
      <button onclick="setSumlogFmax()">Set</button>
    </div>
    <div class=row>
      <label>Pulse on-time (%)</label>
      <input id=pulseDuty type=number min=1 max=99 step=1>
      <button onclick="setPulseDuty()">Set</button> <br|
  <label>Pulse GPIO</label>
  <input id=pulsePin type=number min=0 max=33 step=1>
  <span style="color:red;font-size:12px;">Vain GPIO 0–33 ovat ulostuloja. Älä käytä 34–39.</span>
  <button onclick="setPulsePin()">Set</button>
    </div>
    <div class=row>
      <label>Go to angle</label>
      <input id=a type=number min=0 max=359 step=1>
      <button onclick="goAngle()">Go</button><br>
      <label><input type=checkbox id=freeze onchange="toggleFreeze()"> Freeze NMEA</label>
    </div>
  </div>
  <div id="display1_sumlog_fields" style="display:none;">
    <div class=row>
      <label>Pulse duty<br><span class="small">Pulse ON time (%)</span></label>
      <input id=pulseDuty type=number min=1 max=99 step=1>
      <button onclick="setPulseDuty()">Set</button>
    </div>
    <div class=row>
      <label>Sumlog fmax<br><span class="small">Max pulse frequency (Hz)</span></label>
      <input id=sumlogFmax type=number min=10 max=500 step=1>
      <button onclick="setSumlogFmax()">Set</button>
    </div>
  </div>
</fieldset>

<!-- Display 2 -->
<fieldset>
  <legend>Display 2</legend>
  <div class=row>
    <label><input type=checkbox id=display2_enabled onchange="updateDisplay2Fields()"> Display 2 enabled</label>
  </div>
  <div class=row>
    <label>Meter type</label>
    <select id=display2_type onchange="updateDisplay2Fields()">
      <option value="logicwind">Logic Wind</option>
      <option value="sumlog">Sumlog</option>
    </select>
  </div>
  <div class=row>
    <label>NMEA sentence</label>
    <select id=display2_sentence>
      <option value="MWV">MWV</option>
      <option value="VWR">VWR</option>
      <option value="VWT">VWT</option>
    </select>
  </div>
  <div id="display2_logicwind_fields">
    <div class=row>
      <label>Adjustment (deg)</label>
      <input id=o2 type=number min=-180 max=180 step=1>
      <button onclick="setOffset()">Set</button>
    </div>
    <div class=row>
      <label>Pulse per knot</label>
      <input id=sumlogK2 type=number min=0.1 max=10 step=0.01>
      <button onclick="setSumlogK()">Set</button>
    </div>
    <div class=row>
      <label>Max pulse frequency (Hz)</label>
      <input id=sumlogFmax2 type=number min=10 max=500 step=1>
      <button onclick="setSumlogFmax()">Set</button>
    </div>
    <div class=row>
      <label>Pulse on-time (%)</label>
      <input id=pulseDuty2 type=number min=1 max=99 step=1>
      <button onclick="setPulseDuty()">Set</button>
      <label>Pulse GPIO</label>
      <input id=pulsePin2 type=number min=0 max=39 step=1>
      <button onclick="setPulsePin2()">Set</button>
    </div>
    <div class=row>
      <label>Go to angle</label>
      <input id=a2 type=number min=0 max=359 step=1>
      <button onclick="goAngle()">Go</button>
      <label><input type=checkbox id=freeze2 onchange="toggleFreeze()"> Freeze NMEA</label>
    </div>
  </div>
  <div id="display2_sumlog_fields" style="display:none;">
    <div class=row>
      <label>Pulse on-time (%)</label>
      <input id=pulseDuty2 type=number min=1 max=99 step=1>
      <button onclick="setPulseDuty()">Set</button>
    </div>
    <div class=row>
      <label>Max pulse frequency (Hz)</label>
      <input id=sumlogFmax2 type=number min=10 max=500 step=1>
      <button onclick="setSumlogFmax()">Set</button>
    </div>
  </div>
</fieldset>

<!-- Network Settings -->
<fieldset>
  <legend>Network & NMEA Input</legend>
  <div class=kv>
    <label>Wifi ID</label>
    <input id=ssid type=text maxlength=32 style="min-width:220px">
  </div>
  <div class=kv>
    <label>Wifi password<br></label>
    <input id=pass type=password maxlength=64 style="min-width:220px">
  </div>
  <div class=kv>
    <label>AP Password<br><span class="small">VDO-Cal Access Point</span></label>
    <input id=ap_pass type=password maxlength=64 style="min-width:220px">
  </div>
  <div class=kv>
    <label>Protocol</label>
    <select id=proto>
      <option value="udp">UDP (listen)</option>
      <option value="tcp">TCP client</option>
    </select>
  </div>
  <div class=kv>
    <label>Host address<br></label>
    <input id=host type=text maxlength=64 placeholder="e.g. 192.168.68.50 or hostname" style="min-width:240px">
  </div>
    <div class=kv id="host_tcp_row" style="display:none;">
      <label>Host (TCP)<br><span class="small">Server address</span></label>
      <input id=host type=text maxlength=64 placeholder="e.g. 192.168.68.50 or hostname" style="min-width:240px">
    </div>
  <div class=kv>
    <label>Port</label>
    <input id=portIn type=number min=1 max=65535 step=1 style="width:140px">
  </div>
    <div class=row>
      <button onclick="saveAndReconnect()">Save & Apply</button>
    </div>
  <div style="margin:18px 0 8px 0; font-size:18px; line-height:1.7; display:flex; flex-direction:column; align-items:flex-start;">
    <div style="display:flex; min-width:320px; justify-content:space-between;">
      <span style="display:inline-block; min-width:120px; text-align:left;">Wifi IP:</span>
      <span style="display:inline-block; min-width:180px; text-align:right;" id=sta_ip></span>
    </div>
    <div style="display:flex; min-width:320px; justify-content:space-between;">
      <span style="display:inline-block; min-width:120px; text-align:left;">Wifi ID:</span>
      <span style="display:inline-block; min-width:180px; text-align:right;" id=ap_ssid></span>
    </div>
    <div style="display:flex; min-width:320px; justify-content:space-between;">
      <span style="display:inline-block; min-width:120px; text-align:left;">AP IP:</span>
      <span style="display:inline-block; min-width:180px; text-align:right;" id=ap_ip></span>
    </div>
    <div style="display:flex; min-width:320px; justify-content:space-between;">
      <span style="display:inline-block; min-width:120px; text-align:left;">TCP status:</span>
      <span style="display:inline-block; min-width:180px; text-align:right;" id=tcp_state></span>
    </div>
  </div>
</fieldset>

<footer style="position:fixed;left:0;bottom:0;width:100%;background:none;text-align:center;font-size:13px;color:#888;padding:8px 0;z-index:10;">
  © 2025 Juha-Matti Mäntylä &mdash; Licensed under MIT
</footer>
</div>

<script>
let typing = false;
function updateDisplay1Fields() {
  const type = document.getElementById('display1_type').value;
  document.getElementById('display1_logicwind_fields').style.display = (type === 'logicwind') ? '' : 'none';
  document.getElementById('display1_sumlog_fields').style.display = (type === 'sumlog') ? '' : 'none';
}
function updateDisplay2Fields() {
  const enabled = document.getElementById('display2_enabled').checked;
  const type = document.getElementById('display2_type').value;
  document.getElementById('display2_logicwind_fields').style.display = (enabled && type === 'logicwind') ? '' : 'none';
  document.getElementById('display2_sumlog_fields').style.display = (enabled && type === 'sumlog') ? '' : 'none';
}
function updateHostTCPVisibility() {
  const proto = document.getElementById('proto').value;
  document.getElementById('host_tcp_row').style.display = (proto === 'tcp') ? '' : 'none';
}
async function refresh(){
  try{
    const r = await fetch('/status'); const j = await r.json();
    if (!typing && document.activeElement !== document.getElementById('o')) {
      document.getElementById('o').value = j.offset;
    }
    if (!typing && document.activeElement !== document.getElementById('sumlogK')) {
      document.getElementById('sumlogK').value = j.sumlogK;
    }
    if (!typing && document.activeElement !== document.getElementById('sumlogFmax')) {
      document.getElementById('sumlogFmax').value = j.sumlogFmax;
    }
    if (!typing && document.activeElement !== document.getElementById('pulseDuty')) {
      document.getElementById('pulseDuty').value = j.pulseDuty;
    }
    if (!typing && document.activeElement !== document.getElementById('pulsePin')) {
      document.getElementById('pulsePin').value = j.pulsePin || 0;
    }
    if (!typing && document.activeElement !== document.getElementById('pulsePin2')) {
      document.getElementById('pulsePin2').value = j.pulsePin2 || 0;
    }
    if (!typing && document.activeElement !== document.getElementById('a')) {
      document.getElementById('a').value = j.angle;
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
async function setPulsePin(){ const v = document.getElementById('pulsePin').value;
  if (v >= 0 && v <= 33) {
    await fetch('/pulsepin?val='+encodeURIComponent(v));
  } else {
    alert('GPIO 34–39 ovat vain input-käyttöön. Valitse GPIO 0–33.');
  }
  setTimeout(refresh,150); }
async function setPulsePin2(){ const v = document.getElementById('pulsePin2').value;
  await fetch('/pulsepin2?val='+encodeURIComponent(v)); setTimeout(refresh,150); }
async function setOffset(){ const v = document.getElementById('o').value;
  await fetch('/trim?offset='+encodeURIComponent(v)); typing=false; setTimeout(refresh,150); }
async function setSumlogK(){ const v = document.getElementById('sumlogK').value;
  await fetch('/sumlogk?val='+encodeURIComponent(v)); setTimeout(refresh,150); }
async function setSumlogFmax(){ const v = document.getElementById('sumlogFmax').value;
  await fetch('/sumlogfmax?val='+encodeURIComponent(v)); setTimeout(refresh,150); }
async function setPulseDuty(){ const v = document.getElementById('pulseDuty').value;
  await fetch('/pulseduty?val='+encodeURIComponent(v)); setTimeout(refresh,150); }
async function goAngle(){ const v = document.getElementById('a').value;
  await fetch('/goto?deg='+encodeURIComponent(v)); setTimeout(refresh,150); }
async function toggleFreeze(){ const f = document.getElementById('freeze').checked;
  await fetch('/freeze?on='+(f?1:0)); setTimeout(refresh,150); }
async function saveCfg(){
  const ssid  = document.getElementById('ssid').value;
  const pass  = document.getElementById('pass').value;
  const ap_pass = document.getElementById('ap_pass').value;
  const port  = document.getElementById('portIn').value;
  const proto = document.getElementById('proto').value;
  const host  = document.getElementById('host').value;
  const body = new URLSearchParams({ssid, pass, ap_pass, port, proto, host});
  await fetch('/savecfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
  setTimeout(refresh,200);
}
async function saveAndReconnect(){
  const ssid  = document.getElementById('ssid').value;
  const pass  = document.getElementById('pass').value;
  const ap_pass = document.getElementById('ap_pass').value;
  const port  = document.getElementById('portIn').value;
  const proto = document.getElementById('proto').value;
  const host  = document.getElementById('host').value;
  const body = new URLSearchParams({ssid, pass, ap_pass, port, proto, host});
  await fetch('/savecfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
  await fetch('/reconnect');
  await fetch('/reconnecttcp');
  setTimeout(refresh,1000);
}

window.addEventListener('load', () => {
  const o = document.getElementById('o');
  o.addEventListener('input', () => typing = true);
  o.addEventListener('focus', () => typing = true);
  o.addEventListener('blur',  () => typing = false);
  const k = document.getElementById('sumlogK');
  k.addEventListener('input', () => typing = true);
  k.addEventListener('focus', () => typing = true);
  k.addEventListener('blur',  () => typing = false);
  const fmax = document.getElementById('sumlogFmax');
  fmax.addEventListener('input', () => typing = true);
  fmax.addEventListener('focus', () => typing = true);
  fmax.addEventListener('blur',  () => typing = false);
  const duty = document.getElementById('pulseDuty');
  duty.addEventListener('input', () => typing = true);
  duty.addEventListener('focus', () => typing = true);
  duty.addEventListener('blur',  () => typing = false);

  document.getElementById('proto').addEventListener('change', updateHostTCPVisibility);
  updateHostTCPVisibility();
  updateDisplay1Fields();
  updateDisplay2Fields();
  refresh(); setInterval(refresh,800);
});
</script>
</body></html>
)HTML";

// ---------- HTTP-käsittelijät ----------
// Sumlog-kerroin, pääohjelmasta
extern float sumlog_K;
// Pulse pin -muuttujat (pääohjelmassa määriteltävä)
extern int pulsePin1;
extern int pulsePin2;

static void handlePulsePin1(){
  if (g_srv->hasArg("val")) {
    int v = g_srv->arg("val").toInt();
    if (v >= 0 && v <= 33) {
      // Aseta vanha pinni LOW ja INPUT-tilaan
      pinMode(pulsePin1, INPUT);
      digitalWrite(pulsePin1, LOW);
      pulsePin1 = v;
      pinMode(pulsePin1, OUTPUT);
      digitalWrite(pulsePin1, LOW);
      prefs.putInt("pulsePin1", pulsePin1);
    }
  }
  g_srv->send(200, "text/plain", String("pulsePin1=") + pulsePin1);
}
static void handlePulsePin2(){
  if (g_srv->hasArg("val")) {
    int v = g_srv->arg("val").toInt();
    if (v >= 0 && v <= 33) {
      pinMode(pulsePin2, INPUT);
      digitalWrite(pulsePin2, LOW);
      pulsePin2 = v;
      pinMode(pulsePin2, OUTPUT);
      digitalWrite(pulsePin2, LOW);
      prefs.putInt("pulsePin2", pulsePin2);
    }
  }
  g_srv->send(200, "text/plain", String("pulsePin2=") + pulsePin2);
}

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

static void handleSumlogFmax(){
  if (g_srv->hasArg("val")) {
    int v = g_srv->arg("val").toInt();
    if (v >= 10 && v <= 500) {
      sumlog_fmax = v;
      prefs.putInt("sumlog_fmax", sumlog_fmax);
    }
  }
  g_srv->send(200, "text/plain", String("sumlogFmax=") + sumlog_fmax);
}

static void handlePulseDuty(){
  if (g_srv->hasArg("val")) {
    int v = g_srv->arg("val").toInt();
    if (v >= 1 && v <= 99) {
      pulseDuty = v;
      prefs.putInt("pulseDuty", pulseDuty);
    }
  }
  g_srv->send(200, "text/plain", String("pulseDuty=") + pulseDuty);
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

  // talteen NVS:ään
  prefs.putString("sta_ssid", ssid);
  prefs.putString("sta_pass", pass);
  if (ap_pass.length() > 7) prefs.putString("ap_pass", ap_pass);
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
  j += ",\"sumlogFmax\":"; j += sumlog_fmax;
  j += ",\"pulseDuty\":";  j += pulseDuty;
    j += ",\"pulsePin\":";   j += pulsePin1;
    j += ",\"pulsePin2\":";  j += pulsePin2;
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
  server.on("/sumlogfmax",  HTTP_GET,  handleSumlogFmax);
  server.on("/pulseduty",   HTTP_GET,  handlePulseDuty);
    server.on("/pulsepin",    HTTP_GET,  handlePulsePin1);
    server.on("/pulsepin2",   HTTP_GET,  handlePulsePin2);
  server.on("/savecfg",     HTTP_POST, handleSaveCfg);
  server.on("/reconnect",   HTTP_GET,  handleReconnect);
  server.on("/reconnecttcp",HTTP_GET,  handleReconnectTCP);
  server.on("/status",      HTTP_GET,  handleStatus);
}
