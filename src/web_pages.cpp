// web_pages.cpp - HTML page builders

#include "web_ui.h"
#include <Arduino.h>

// ---------- Page Templates ----------

// Common header with navigation
String buildPageHeader(String activeTab) {
  String html = R"HTML(
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
.nav {
  display: flex;
  background: #fff;
  border-radius: 8px;
  margin-bottom: 12px;
  overflow: hidden;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}
.nav a {
  flex: 1;
  padding: 12px 8px;
  text-decoration: none;
  text-align: center;
  color: #666;
  border-right: 1px solid #ddd;
  font-weight: 500;
}
.nav a:last-child {
  border-right: none;
}
.nav a.active {
  background: #28a745;
  color: white;
}
.nav a:hover:not(.active) {
  background: #f8f9fa;
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
  .nav a {
    padding: 8px 4px;
    font-size: 13px;
  }
}
</style>
</head><body>
<div class="card">

<h1 style="text-align:center; margin-bottom:2px;">VDO Wind Adapter</h1>
<div style="text-align:center; margin-top:-8px; margin-bottom:12px;"><span class="small">Bring your analog sensor into the digital age</span></div>

<nav class="nav">
  <a href="/"        class=")HTML";
  html += (activeTab == "status") ? "active" : "";
  html += R"HTML(">Status</a>
  <a href="/network" class=")HTML";
  html += (activeTab == "network") ? "active" : "";
  html += R"HTML(">Network</a>
  <a href="/display1" class=")HTML";
  html += (activeTab == "display1") ? "active" : "";
  html += R"HTML(">Display 1</a>
  <a href="/display2" class=")HTML";
  html += (activeTab == "display2") ? "active" : "";
  html += R"HTML(">Display 2</a>
  <a href="/display3" class=")HTML";
  html += (activeTab == "display3") ? "active" : "";
  html += R"HTML(">Display 3</a>
</nav>
)HTML";
  return html;
}

// Common footer
String buildPageFooter() {
  return R"HTML(
</div>

<footer>
  © 2025 Juha-Matti Mäntylä &mdash; Licensed under MIT
</footer>

<script>
let typing = false;
async function refresh(){
  try{
    const r = await fetch('/status'); const j = await r.json();
    // Update common status elements if they exist
    const rawEl = document.getElementById('raw');
    const angEl = document.getElementById('ang');
    const staIpEl = document.getElementById('sta_ip');
    const apSsidEl = document.getElementById('ap_ssid');
    const apIpEl = document.getElementById('ap_ip');
    const tcpStateEl = document.getElementById('tcp_state');
    
    if (rawEl) rawEl.textContent = j.raw || "-";
    if (angEl) angEl.textContent = j.angle;
    if (staIpEl) staIpEl.textContent = j.sta_ip || "-";
    if (apSsidEl) apSsidEl.textContent = j.ap_ssid || "-";
    if (apIpEl) apIpEl.textContent = j.ap_ip || "-";
    if (tcpStateEl) tcpStateEl.textContent = j.tcp_connected ? "connected" : "disconnected";
  }catch(e){}
}

window.addEventListener('load', () => {
  refresh(); 
  setInterval(refresh, 800);
});
</script>
</body></html>
)HTML";
}

// ---------- Individual Pages ----------

// Status page (home)
String buildStatusPage() {
  return buildPageHeader("status") + R"HTML(
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

<!-- Status Overview -->
<fieldset>
  <legend>System Status</legend>
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

<!-- Quick Actions -->
<fieldset>
  <legend>Quick Actions</legend>
  <div class=row>
    <button onclick="window.location.href='/display1'">Configure Display 1</button>
    <button onclick="window.location.href='/display2'">Configure Display 2</button>
    <button onclick="window.location.href='/display3'">Configure Display 3</button>
  </div>
  <div class=row>
    <button onclick="window.location.href='/network'">Network Settings</button>
  </div>
</fieldset>
)HTML" + buildPageFooter();
}

// Network settings page
String buildNetworkPage() {
  return buildPageHeader("network") + R"HTML(
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
  <div class=kv>
    <label>Port</label>
    <input id=portIn type=number min=1 max=65535 step=1 style="width:140px">
  </div>
  <div class=row>
    <button onclick="saveAndReconnect()">Save & Apply</button>
  </div>
</fieldset>

<script>
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

// Load current values
async function loadNetworkValues() {
  try {
    const r = await fetch('/status'); const j = await r.json();
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
  } catch(e) {}
}

window.addEventListener('load', () => {
  loadNetworkValues();
});
</script>
)HTML" + buildPageFooter();
}

// Unified display page template
String buildDisplayPage(int displayNum) {
  String activeTab = "display" + String(displayNum);
  String displayTitle = "Display " + String(displayNum);
  
  String html = buildPageHeader(activeTab);
  html += R"HTML(
<!-- Display Settings -->
<fieldset>
  <legend>)HTML" + displayTitle + R"HTML(</legend>
  <div class=row>
    <label><input type=checkbox id=displayEnabled onchange="setDisplayEnabled()"> )HTML" + displayTitle + R"HTML( enabled</label>
  </div>
  <div class=row>
    <label>Meter type</label>
    <select id=displayType onchange="updateDisplayFields()">
      <option value="logicwind">Logic Wind</option>
      <option value="sumlog">Sumlog</option>
    </select>
  </div>
  <div class=row>
    <label>NMEA sentence</label>
    <select id=displaySentence>
      <option value="MWV">MWV</option>
      <option value="VWR">VWR</option>
      <option value="VWT">VWT</option>
    </select>
  </div>
  
  <div id="logicwind_fields">
    <div class=row>
      <label>Adjustment (deg)</label>
      <input id=offsetDeg type=number min=-180 max=180 step=1>
    </div>
    <div class=row>
      <label>Go to angle</label>
      <input id=gotoAngle type=number min=0 max=359 step=1>
      <label><input type=checkbox id=freeze> Freeze NMEA</label>
    </div>
    <div class=row>
      <label>Pulse per knot</label>
      <input id=sumlogK type=number min=0.1 max=10 step=0.01>
    </div>
    <div class=row>
      <label>Max pulse frequency (Hz)</label>
      <input id=sumlogFmax type=number min=10 max=500 step=1>
    </div>
    <div class=row>
      <label>Pulse on-time (%)</label>
      <input id=pulseDuty type=number min=1 max=99 step=1>
    </div>
    <div class=row>
      <label>Pulse GPIO</label>
      <input id=pulsePin type=number min=0 max=33 step=1>
      <span style="color:red;font-size:12px;">Vain GPIO 0–33 ovat ulostuloja. Älä käytä 34–39.</span>
    </div>
  </div>
  
  <div id="sumlog_fields" style="display:none;">
    <div class=row>
      <label>Pulse per knot</label>
      <input id=sumlogK type=number min=0.1 max=10 step=0.01>
    </div>
    <div class=row>
      <label>Max pulse frequency (Hz)</label>
      <input id=sumlogFmax type=number min=10 max=500 step=1>
    </div>
    <div class=row>
      <label>Pulse on-time (%)</label>
      <input id=pulseDuty type=number min=1 max=99 step=1>
    </div>
    <div class=row>
      <label>Pulse GPIO</label>
      <input id=pulsePin type=number min=0 max=33 step=1>
      <span style="color:red;font-size:12px;">Vain GPIO 0–33 ovat ulostuloja. Älä käytä 34–39.</span>
    </div>
  </div>
  
  <div class=row>
    <button onclick="saveDisplaySettings()" style="background:#28a745;color:white;padding:8px 16px;">Save )HTML" + displayTitle + R"HTML( Settings</button>
  </div>
</fieldset>

<script>
const DISPLAY_NUM = )HTML" + String(displayNum) + R"HTML(;

function updateDisplayFields() {
  const enabled = document.getElementById('displayEnabled').checked;
  const type = document.getElementById('displayType').value;
  document.getElementById('logicwind_fields').style.display = (enabled && type === 'logicwind') ? '' : 'none';
  document.getElementById('sumlog_fields').style.display = (enabled && type === 'sumlog') ? '' : 'none';
}

async function setDisplayEnabled(){ 
  const enabled = document.getElementById('displayEnabled').checked;
  await fetch('/api/display?num=' + DISPLAY_NUM + '&action=enabled&val=' + (enabled?1:0));
  updateDisplayFields();
  setTimeout(refresh,150);
}

async function saveDisplaySettings(){
  const enabled = document.getElementById('displayEnabled').checked;
  const type = document.getElementById('displayType').value;
  const sentence = document.getElementById('displaySentence').value;
  const offsetDeg = document.getElementById('offsetDeg').value;
  const gotoAngle = document.getElementById('gotoAngle').value;
  const freeze = document.getElementById('freeze').checked;
  const sumlogK = document.getElementById('sumlogK').value;
  const sumlogFmax = document.getElementById('sumlogFmax').value;
  const pulseDuty = document.getElementById('pulseDuty').value;
  const pulsePin = document.getElementById('pulsePin').value;
  
  // Save all settings in one request
  const params = new URLSearchParams({
    enabled: enabled?1:0,
    type: type,
    sentence: sentence,
    offsetDeg: offsetDeg,
    gotoAngle: gotoAngle,
    freeze: freeze?1:0,
    sumlogK: sumlogK,
    sumlogFmax: sumlogFmax,
    pulseDuty: pulseDuty,
    pulsePin: pulsePin
  });
  
  await fetch('/api/display?num=' + DISPLAY_NUM + '&action=save', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: params
  });
  
  setTimeout(refresh,150);
}

async function goAngle(){ 
  const v = document.getElementById('gotoAngle').value;
  await fetch('/goto?deg='+encodeURIComponent(v)); 
  setTimeout(refresh,150); 
}

// Load current display values
async function loadDisplayValues() {
  try {
    const r = await fetch('/api/display?num=' + DISPLAY_NUM); 
    const j = await r.json();
    
    document.getElementById('displayEnabled').checked = j.enabled || false;
    document.getElementById('displayType').value = j.type || "sumlog";
    document.getElementById('displaySentence').value = j.sentence || "MWV";
    document.getElementById('offsetDeg').value = j.offsetDeg || 0;
    document.getElementById('gotoAngle').value = j.gotoAngle || 0;
    document.getElementById('freeze').checked = j.freeze || false;
    document.getElementById('sumlogK').value = j.sumlogK || 1.0;
    document.getElementById('sumlogFmax').value = j.sumlogFmax || 150;
    document.getElementById('pulseDuty').value = j.pulseDuty || 10;
    document.getElementById('pulsePin').value = j.pulsePin || (12 + DISPLAY_NUM * 2);
    
    updateDisplayFields();
  } catch(e) {}
}

window.addEventListener('load', () => {
  loadDisplayValues();
});
</script>
)HTML";
  
  return html + buildPageFooter();
}
