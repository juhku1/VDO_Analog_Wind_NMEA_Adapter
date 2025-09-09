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
    
    // Legacy elements
    const staIpEl = document.getElementById('sta_ip');
    const apSsidEl = document.getElementById('ap_ssid');
    const apIpEl = document.getElementById('ap_ip');
    const tcpStateEl = document.getElementById('tcp_state');
    
    // New structured status elements
    const staSsidNameEl = document.getElementById('sta_ssid_name');
    const staStatusEl = document.getElementById('sta_status');
    const apSsidNameEl = document.getElementById('ap_ssid_name');
    const apStatusEl = document.getElementById('ap_status');
    const nmeaProtocolEl = document.getElementById('nmea_protocol');
    const nmeaPortEl = document.getElementById('nmea_port');
    const nmeaHostEl = document.getElementById('nmea_host');
    const nmeaStatusEl = document.getElementById('nmea_status');
    
    if (rawEl) rawEl.textContent = j.raw || "-";
    if (angEl) angEl.textContent = j.angle;
    
    // Legacy status (for compatibility)
    if (staIpEl) staIpEl.textContent = j.sta_ip || "-";
    if (apSsidEl) apSsidEl.textContent = j.ap_ssid || "-";
    if (apIpEl) apIpEl.textContent = j.ap_ip || "-";
    if (tcpStateEl) tcpStateEl.textContent = j.tcp_connected ? "connected" : "disconnected";
    
    // New structured status
    if (staSsidNameEl) staSsidNameEl.textContent = j.sta_ssid || "not configured";
    if (staStatusEl) {
      const connected = j.sta_connected;
      const ip = j.sta_ip;
      staStatusEl.textContent = connected ? `Connected (${ip})` : "Disconnected";
      staStatusEl.style.color = connected ? "#28a745" : "#dc3545";
    }
    
    if (apSsidNameEl) apSsidNameEl.textContent = j.ap_ssid || "VDO-Cal";
    if (apStatusEl) {
      const clients = j.ap_clients || 0;
      const clientText = clients === 0 ? "no clients" : `${clients} client${clients > 1 ? 's' : ''}`;
      apStatusEl.textContent = `Active (${clientText})`;
      apStatusEl.style.color = "#28a745";
    }
    
    if (nmeaProtocolEl) nmeaProtocolEl.textContent = j.proto === "TCP" ? "TCP (client)" : "UDP (listening)";
    if (nmeaPortEl) nmeaPortEl.textContent = j.port || "10110";
    if (nmeaHostEl) nmeaHostEl.textContent = j.host || "192.168.4.2";
    if (nmeaStatusEl) {
      if (j.proto === "TCP") {
        const connected = j.tcp_connected;
        nmeaStatusEl.textContent = connected ? "Connected" : "Disconnected";
        nmeaStatusEl.style.color = connected ? "#28a745" : "#dc3545";
      } else {
        // UDP: Check if data is actually coming in (within last 5 seconds)
        const dataAge = j.nmea_data_age || 999999;
        const dataActive = dataAge < 5000;
        nmeaStatusEl.textContent = dataActive ? "Active (data flowing)" : "Ready for data";
        nmeaStatusEl.style.color = dataActive ? "#28a745" : "#ffc107";
      }
    }
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
  
  <!-- WiFi Station (Client) -->
  <div style="margin:12px 0; padding:8px; background:#f8f9fa; border-left:4px solid #007bff;">
    <h4 style="margin:0 0 8px 0; color:#007bff;">WiFi Station (Client)</h4>
    <div style="line-height:1.5;">
      <div><strong>Network:</strong> <span id="sta_ssid_name">loading...</span></div>
      <div><strong>IP:</strong> <span id="sta_ip">loading...</span></div>
      <div><strong>Status:</strong> <span id="sta_status">loading...</span></div>
    </div>
  </div>
  
  <!-- Access Point -->
  <div style="margin:12px 0; padding:8px; background:#f8f9fa; border-left:4px solid #28a745;">
    <h4 style="margin:0 0 8px 0; color:#28a745;">Access Point</h4>
    <div style="line-height:1.5;">
      <div><strong>Network:</strong> <span id="ap_ssid_name">loading...</span></div>
      <div><strong>IP:</strong> <span id="ap_ip">loading...</span></div>
      <div><strong>Status:</strong> <span id="ap_status">loading...</span></div>
    </div>
  </div>
  
  <!-- NMEA Input -->
  <div style="margin:12px 0; padding:8px; background:#f8f9fa; border-left:4px solid #ffc107;">
    <h4 style="margin:0 0 8px 0; color:#e67e22;">NMEA Input</h4>
    <div style="line-height:1.5;">
      <div><strong>Protocol:</strong> <span id="nmea_protocol">loading...</span></div>
      <div><strong>Port:</strong> <span id="nmea_port">loading...</span></div>
      <div><strong>Host:</strong> <span id="nmea_host">loading...</span></div>
      <div><strong>Status:</strong> <span id="nmea_status">loading...</span></div>
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
<style>
.info-icon {
  display: inline-block;
  width: 16px;
  height: 16px;
  background: #007bff;
  color: white;
  border-radius: 50%;
  text-align: center;
  line-height: 16px;
  font-size: 12px;
  font-weight: bold;
  cursor: help;
  margin-left: 8px;
  position: relative;
  vertical-align: middle;
}
.info-icon:hover::after {
  content: attr(data-tooltip);
  position: absolute;
  bottom: 25px;
  left: 50%;
  transform: translateX(-50%);
  background: #333;
  color: white;
  padding: 8px 12px;
  border-radius: 4px;
  font-size: 12px;
  white-space: nowrap;
  z-index: 1000;
  box-shadow: 0 2px 8px rgba(0,0,0,0.3);
}
.info-icon:hover::before {
  content: '';
  position: absolute;
  bottom: 20px;
  left: 50%;
  transform: translateX(-50%);
  border: 5px solid transparent;
  border-top-color: #333;
  z-index: 1001;
}
.section-header {
  background: #f8f9fa;
  padding: 10px 15px;
  margin: 15px 0 0 0;
  border-left: 4px solid #007bff;
  border-radius: 3px 3px 0 0;
}
.section-header h3 {
  margin: 0;
  color: #007bff;
  font-size: 16px;
}
.section-header p {
  margin: 5px 0 0 0;
  font-size: 13px;
  color: #666;
}
.section-fieldset {
  margin-top: 0;
  border-top: none;
  border-radius: 0 0 3px 3px;
}
</style>

<!-- WiFi Station Settings -->
<div class="section-header">
  <h3>WiFi Station (Client Connection)</h3>
  <p>Connect ESP32 to existing WiFi network</p>
</div>
<fieldset class="section-fieldset">
  <div class=kv>
    <label>Network Name (SSID)</label>
    <input id=ssid type=text maxlength=32 style="min-width:220px" placeholder="loading...">
    <span class="info-icon" data-tooltip="Your home/office WiFi network name">i</span>
  </div>
  <div class=kv>
    <label>WiFi Password</label>
    <input id=pass type=password maxlength=64 style="min-width:220px" placeholder="Enter new password">
    <span class="info-icon" data-tooltip="Password for your home/office WiFi network">i</span>
  </div>
</fieldset>

<!-- Access Point Settings -->
<div class="section-header" style="border-left-color: #28a745;">
  <h3 style="color: #28a745;">Access Point (Hotspot)</h3>
  <p>ESP32 creates WiFi hotspot for direct device connection</p>
</div>
<fieldset class="section-fieldset">
  <div class=kv>
    <label>AP Password<br><span class="small">Network: VDO-Cal</span></label>
    <input id=ap_pass type=password maxlength=64 style="min-width:220px" placeholder="Enter new AP password">
    <span class="info-icon" data-tooltip="Password for VDO-Cal WiFi hotspot created by ESP32">i</span>
  </div>
</fieldset>

<!-- NMEA Data Input Settings -->
<div class="section-header" style="border-left-color: #ffc107;">
  <h3 style="color: #e67e22;">NMEA Data Input</h3>
  <p>Configure wind data reception from navigation software or instruments</p>
</div>
<fieldset class="section-fieldset">
  <div class=kv>
    <label>Protocol</label>
    <select id=proto>
      <option value="udp">UDP (listen for broadcasts)</option>
      <option value="tcp">TCP (connect to server)</option>
    </select>
    <span class="info-icon" data-tooltip="UDP: Listen for broadcast data. TCP: Connect to specific server">i</span>
  </div>
  <div class=kv>
    <label>Host Address</label>
    <input id=host type=text maxlength=64 placeholder="e.g. 192.168.68.50 or hostname" style="min-width:240px">
    <span class="info-icon" data-tooltip="For TCP: Server IP address. For UDP: not used (listens on all interfaces)">i</span>
  </div>
  <div class=kv>
    <label>Port</label>
    <input id=portIn type=number min=1 max=65535 step=1 style="width:140px" placeholder="loading...">
    <span class="info-icon" data-tooltip="Network port number for NMEA data (common: 10110, 4800, 2000)">i</span>
  </div>
</fieldset>

<div class=row style="margin-top: 20px;">
  <button onclick="saveAndReconnect()">Save & Apply All Settings</button>
</div>

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
    
    // Update visible current values in labels
    document.getElementById('ssidValue').textContent = '(current: ' + (j.sta_ssid || 'not set') + ')';
    document.getElementById('passValue').textContent = j.sta_ssid ? '(password set)' : '(no password)';
    document.getElementById('apPassValue').textContent = ' (AP password set)';
    document.getElementById('protoValue').textContent = '(current: ' + (j.proto || 'UDP') + ')';
    document.getElementById('hostValue').textContent = '(current: ' + (j.host || 'not set') + ')';
    document.getElementById('portValue').textContent = '(current: ' + (j.port || 'not set') + ')';
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
<style>
.info-icon {
  display: inline-block;
  width: 16px;
  height: 16px;
  background: #007bff;
  color: white;
  border-radius: 50%;
  text-align: center;
  line-height: 16px;
  font-size: 12px;
  font-weight: bold;
  cursor: help;
  margin-left: 8px;
  position: relative;
  vertical-align: middle;
}
.info-icon:hover::after {
  content: attr(data-tooltip);
  position: absolute;
  bottom: 25px;
  left: 50%;
  transform: translateX(-50%);
  background: #333;
  color: white;
  padding: 8px 12px;
  border-radius: 4px;
  font-size: 12px;
  white-space: nowrap;
  z-index: 1000;
  box-shadow: 0 2px 8px rgba(0,0,0,0.3);
}
.info-icon:hover::before {
  content: '';
  position: absolute;
  bottom: 20px;
  left: 50%;
  transform: translateX(-50%);
  border: 5px solid transparent;
  border-top-color: #333;
  z-index: 1001;
}
.section-header {
  background: #f8f9fa;
  padding: 10px 15px;
  margin: 15px 0 0 0;
  border-left: 4px solid #007bff;
  border-radius: 3px 3px 0 0;
}
.section-header h3 {
  margin: 0;
  color: #007bff;
  font-size: 16px;
}
.section-header p {
  margin: 5px 0 0 0;
  font-size: 13px;
  color: #666;
}
.section-fieldset {
  margin-top: 0;
  border-top: none;
  border-radius: 0 0 3px 3px;
}
</style>

<!-- Basic Display Settings -->
<fieldset>
  <legend>)HTML" + displayTitle + R"HTML( Settings</legend>
  
  <div class=row>
    <label><input type=checkbox id=displayEnabled onchange="setDisplayEnabled()"> Enable )HTML" + displayTitle + R"HTML(</label>
  </div>
  
  <div class=row>
    <label>Meter Type</label>
    <select id=displayType onchange="updateDisplayFields()">
      <option value="logicwind">Logic Wind</option>
      <option value="sumlog">Sumlog</option>
    </select>
    <span class="info-icon" data-tooltip="Logic Wind: Direction + speed display. Sumlog: Speed-only display">i</span>
  </div>
  
  <div class=row>
    <label>NMEA Sentence</label>
    <select id=displaySentence>
      <option value="MWV">MWV (Wind Speed & Angle)</option>
      <option value="VWR">VWR (Relative Wind)</option>
      <option value="VWT">VWT (True Wind)</option>
    </select>
    <span class="info-icon" data-tooltip="Which NMEA sentence to use for wind data">i</span>
  </div>
</fieldset>

<!-- Wind Direction Settings (Logic Wind only) -->
<div class="section-header" id="wind_direction_header" style="border-left-color: #28a745;">
  <h3 style="color: #28a745;">Wind Direction Settings</h3>
  <p>Configure wind direction display and calibration</p>
</div>
<fieldset class="section-fieldset" id="logicwind_direction_fields">
  
  <div class=row>
    <label>Direction Adjustment (degrees)</label>
    <input id=offsetDeg type=number min=-180 max=180 step=1 placeholder="loading...">
    <span class="info-icon" data-tooltip="Calibrate wind direction offset in degrees (-180 to +180)">i</span>
  </div>
  
  <div class=row>
    <label>Test Direction (degrees)</label>
    <input id=gotoAngle type=number min=0 max=359 step=1 placeholder="loading...">
    <button onclick="goAngle()" style="margin-left: 8px; background:#ffc107; border:none; padding:4px 8px; border-radius:3px;">Go to Angle</button>
    <span class="info-icon" data-tooltip="Test tool: Set specific wind direction for testing">i</span>
  </div>
  
  <div class=row>
    <label>Freeze NMEA Data</label>
    <input type=checkbox id=freeze>
    <span class="info-icon" data-tooltip="Stop processing new NMEA data (for testing)">i</span>
  </div>
  
</fieldset>

<!-- Pulse Generation Settings (both types when enabled) -->
<div class="section-header" style="border-left-color: #007bff;">
  <h3 style="color: #007bff;">Pulse Generation Settings</h3>
  <p>Configure speed pulse output parameters</p>
</div>
<fieldset class="section-fieldset" id="pulse_fields">
  
  <div class=row>
    <label>Pulses per Knot</label>
    <input id=sumlogK type=number min=0.1 max=10 step=0.01 placeholder="loading...">
    <span class="info-icon" data-tooltip="How many pulses to generate per knot of wind speed">i</span>
  </div>
  
  <div class=row>
    <label>Maximum Frequency (Hz)</label>
    <input id=sumlogFmax type=number min=10 max=500 step=1 placeholder="loading...">
    <span class="info-icon" data-tooltip="Maximum pulse frequency to prevent meter damage">i</span>
  </div>
  
  <div class=row>
    <label>Pulse Duty Cycle (%)</label>
    <input id=pulseDuty type=number min=1 max=99 step=1 placeholder="loading...">
    <span class="info-icon" data-tooltip="Percentage of time pulse is HIGH (1-99%)">i</span>
  </div>
  
  <div class=row>
    <label>Output GPIO Pin</label>
    <input id=pulsePin type=number min=0 max=33 step=1 placeholder="loading...">
    <span class="info-icon" data-tooltip="GPIO pin for pulse output (0-33 only, avoid 34-39)">i</span>
  </div>
  
</fieldset>

<!-- Pulse Filtering Settings (Sumlog only) -->
<div class="section-header" id="filter_header" style="border-left-color: #ffc107;">
  <h3 style="color: #e67e22;">Pulse Filtering Settings</h3>
  <p>Reduce meter needle oscillation and improve stability</p>
</div>
<fieldset class="section-fieldset" id="filter_fields">
  
  <div class=row>
    <label>Speed Filter Alpha</label>
    <input id=speedFilterAlpha type=number min=0.1 max=0.9 step=0.01 placeholder="loading...">
    <span class="info-icon" data-tooltip="Filter responsiveness: 0.1=fast response, 0.9=stable/slow">i</span>
  </div>
  
  <div class=row>
    <label>Frequency Deadband (Hz)</label>
    <input id=freqDeadband type=number min=0.1 max=5.0 step=0.1 placeholder="loading...">
    <span class="info-icon" data-tooltip="Minimum frequency change to trigger update">i</span>
  </div>
  
  <div class=row>
    <label>Max Step Percentage (%)</label>
    <input id=maxStepPercent type=number min=5 max=50 step=1 placeholder="loading...">
    <span class="info-icon" data-tooltip="Maximum frequency change per update cycle">i</span>
  </div>
  
</fieldset>

<div class=row style="margin-top: 20px;">
  <button onclick="saveDisplaySettings()" style="background:#28a745;color:white;padding:8px 16px;">Save )HTML" + displayTitle + R"HTML( Settings</button>
</div>

<script>
const DISPLAY_NUM = )HTML" + String(displayNum) + R"HTML(;

function updateDisplayFields() {
  const enabled = document.getElementById('displayEnabled').checked;
  const type = document.getElementById('displayType').value;
  
  // 1. WIND DIRECTION: Show only for Logic Wind type
  const showDirection = (enabled && type === 'logicwind');
  document.getElementById('wind_direction_header').style.display = showDirection ? '' : 'none';
  document.getElementById('logicwind_direction_fields').style.display = showDirection ? '' : 'none';
  
  // 2. PULSE: Show always when enabled
  const showPulse = enabled;
  document.getElementById('pulse_fields').style.display = showPulse ? '' : 'none';
  
  // 3. FILTERING: Show only for Sumlog type
  const showFilter = (enabled && type === 'sumlog');
  document.getElementById('filter_header').style.display = showFilter ? '' : 'none';
  document.getElementById('filter_fields').style.display = showFilter ? '' : 'none';
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
  const speedFilterAlpha = document.getElementById('speedFilterAlpha').value;
  const freqDeadband = document.getElementById('freqDeadband').value;
  const maxStepPercent = document.getElementById('maxStepPercent').value;
  
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
    pulsePin: pulsePin,
    speedFilterAlpha: speedFilterAlpha,
    freqDeadband: freqDeadband,
    maxStepPercent: maxStepPercent
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
    document.getElementById('speedFilterAlpha').value = j.speedFilterAlpha || 0.8;
    document.getElementById('freqDeadband').value = j.freqDeadband || 0.5;
    document.getElementById('maxStepPercent').value = j.maxStepPercent || 15.0;
    
    updateDisplayFields();
  } catch(e) {}
}

window.addEventListener('load', () => {
  loadDisplayValues();
});
</script>
)HTML";
  
  html += buildPageFooter();
  return html;
}
