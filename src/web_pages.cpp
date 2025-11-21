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
<meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate">
<meta http-equiv="Pragma" content="no-cache">
<meta http-equiv="Expires" content="0">
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
    const spdEl = document.getElementById('spd');
    const sentenceTypesEl = document.getElementById('sentence_types');
    
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
    if (spdEl) spdEl.textContent = j.speed_kn.toFixed(2);
    
    // Build sentence types list
    if (sentenceTypesEl) {
      let types = [];
      if (j.has_mwv_r) types.push("MWV(R)");
      if (j.has_mwv_t) types.push("MWV(T)");
      if (j.has_vwr) types.push("VWR");
      if (j.has_vwt) types.push("VWT");
      sentenceTypesEl.textContent = types.length > 0 ? types.join(", ") : "waiting...";
    }
    
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
    
    if (nmeaProtocolEl) {
      if (j.proto === "TCP") {
        nmeaProtocolEl.textContent = "TCP (client)";
      } else if (j.proto === "HTTP") {
        nmeaProtocolEl.textContent = "HTTP (client)";
      } else {
        nmeaProtocolEl.textContent = "UDP (listening)";
      }
    }
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
<style>
.edit-btn {
  background: #007bff;
  color: white;
  border: none;
  padding: 4px 10px;
  border-radius: 3px;
  cursor: pointer;
  font-size: 12px;
  margin-left: 8px;
}
.edit-btn:hover {
  background: #0056b3;
}
.modal {
  display: none;
  position: fixed;
  z-index: 1000;
  left: 0;
  top: 0;
  width: 100%;
  height: 100%;
  background: rgba(0,0,0,0.5);
}
.modal.show {
  display: flex;
  align-items: center;
  justify-content: center;
}
.modal-content {
  background: white;
  padding: 20px;
  border-radius: 8px;
  width: 90%;
  max-width: 500px;
  box-shadow: 0 4px 20px rgba(0,0,0,0.3);
  max-height: 80vh;
  overflow-y: auto;
}
.modal-header {
  font-size: 18px;
  font-weight: bold;
  margin-bottom: 15px;
  color: #333;
}
.modal-close {
  float: right;
  font-size: 24px;
  cursor: pointer;
  color: #999;
}
.modal-close:hover {
  color: #333;
}
</style>

<!-- Page Header -->
<div class="section-header">
  <h3>System Status</h3>
  <p>Real-time wind data and system information</p>
</div>

<!-- Incoming & Outgoing Data Info -->
<fieldset>
  <legend>Incoming & Outgoing Data</legend>
  <div class=row>
    <label>Incoming NMEA:</label>
    <span id=raw class="mono"></span>
  </div>
  <div class=row>
    <label>Received Types (5s window):</label>
    <span id=sentence_types style="font-family:monospace;"></span>
  </div>
  <div class=row>
    <label>Outgoing angle:</label>
    <span id=ang></span>°
  </div>
  <div class=row>
    <label>Outgoing speed:</label>
    <span id=spd></span> kn
  </div>
</fieldset>

<!-- Status Overview with Edit Buttons -->
<fieldset>
  <legend>System Status</legend>
  
  <!-- WiFi Station (Client) -->
  <div style="margin:12px 0; padding:12px; background:#f8f9fa; border-left:4px solid #007bff; border-radius:3px;">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
      <h4 style="margin:0; color:#007bff;">WiFi Station (Client)</h4>
      <button class="edit-btn" onclick="openEditWiFi()">Edit</button>
    </div>
    <div style="line-height:1.8;">
      <div><strong>Network:</strong> <span id="sta_ssid_name">loading...</span></div>
      <div><strong>IP:</strong> <span id="sta_ip">loading...</span></div>
      <div><strong>Status:</strong> <span id="sta_status">loading...</span></div>
    </div>
  </div>
  
  <!-- Access Point -->
  <div style="margin:12px 0; padding:12px; background:#f8f9fa; border-left:4px solid #28a745; border-radius:3px;">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
      <h4 style="margin:0; color:#28a745;">Access Point</h4>
      <button class="edit-btn" onclick="openEditAP()">Edit</button>
    </div>
    <div style="line-height:1.8;">
      <div><strong>Network:</strong> VDO-Cal</div>
      <div><strong>IP:</strong> <span id="ap_ip">loading...</span></div>
      <div><strong>Status:</strong> <span id="ap_status">loading...</span></div>
    </div>
  </div>
  
  <!-- NMEA Input 1 -->
  <div style="margin:12px 0; padding:12px; background:#f8f9fa; border-left:4px solid #ffc107; border-radius:3px;">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
      <h4 style="margin:0; color:#e67e22;">NMEA Input 1 (TCP)</h4>
      <button class="edit-btn" onclick="openEditNMEA1()">Edit</button>
    </div>
    <div style="line-height:1.8;">
      <div><strong>Profile:</strong> <span id="nmea1_name">loading...</span></div>
      <div><strong>Protocol:</strong> <span id="nmea1_protocol">loading...</span></div>
      <div><strong>Host:</strong> <span id="nmea1_host">loading...</span></div>
      <div><strong>Port:</strong> <span id="nmea1_port">loading...</span></div>
      <div><strong>Status:</strong> <span id="nmea1_status">loading...</span></div>
    </div>
  </div>

  <!-- NMEA Input 2 -->
  <div style="margin:12px 0; padding:12px; background:#f8f9fa; border-left:4px solid #ffc107; border-radius:3px;">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
      <h4 style="margin:0; color:#e67e22;">NMEA Input 2 (UDP)</h4>
      <button class="edit-btn" onclick="openEditNMEA2()">Edit</button>
    </div>
    <div style="line-height:1.8;">
      <div><strong>Profile:</strong> <span id="nmea2_name">loading...</span></div>
      <div><strong>Protocol:</strong> <span id="nmea2_protocol">loading...</span></div>
      <div><strong>Host:</strong> <span id="nmea2_host">loading...</span></div>
      <div><strong>Port:</strong> <span id="nmea2_port">loading...</span></div>
      <div><strong>Status:</strong> <span id="nmea2_status">loading...</span></div>
    </div>
  </div>
</fieldset>

<!-- WiFi Edit Modal -->
<div id="wifiModal" class="modal">
  <div class="modal-content">
    <span class="modal-close" onclick="closeWiFiModal()">&times;</span>
    <div class="modal-header">Edit WiFi Connection</div>
    
    <div style="margin-bottom: 15px; padding: 12px; background: #fff3cd; border-left: 4px solid #ffc107; border-radius: 3px;">
      <strong style="color: #856404;">Select Profile to Use</strong>
      <div style="margin-top: 8px; display: flex; gap: 20px;">
        <label><input type="radio" id="wifi_mode_0" name="wifi_mode" value="0" checked> Profile 1</label>
        <label><input type="radio" id="wifi_mode_1" name="wifi_mode" value="1"> Profile 2</label>
      </div>
    </div>

    <div style="margin-bottom: 12px;">
      <label>WiFi Profile 1 - SSID:</label>
      <input id="w1_ssid" type="text" maxlength="32" style="width:100%; padding:6px; margin-top:4px;">
    </div>
    <div style="margin-bottom: 12px;">
      <label>WiFi Profile 1 - Password:</label>
      <input id="w1_pass" type="password" maxlength="64" style="width:100%; padding:6px; margin-top:4px;">
    </div>

    <div style="margin-bottom: 12px;">
      <label>WiFi Profile 2 - SSID:</label>
      <input id="w2_ssid" type="text" maxlength="32" style="width:100%; padding:6px; margin-top:4px;" placeholder="(leave empty to disable)">
    </div>
    <div style="margin-bottom: 12px;">
      <label>WiFi Profile 2 - Password:</label>
      <input id="w2_pass" type="password" maxlength="64" style="width:100%; padding:6px; margin-top:4px;">
    </div>

    <div style="display: flex; gap: 8px; margin-top: 20px;">
      <button onclick="saveWiFiSettings()" style="flex:1; padding:8px; background:#28a745; color:white; border:none; border-radius:4px; cursor:pointer;">Save & Apply</button>
      <button onclick="closeWiFiModal()" style="flex:1; padding:8px; background:#999; color:white; border:none; border-radius:4px; cursor:pointer;">Cancel</button>
    </div>
  </div>
</div>

<!-- AP Edit Modal -->
<div id="apModal" class="modal">
  <div class="modal-content">
    <span class="modal-close" onclick="closeAPModal()">&times;</span>
    <div class="modal-header">Edit Access Point Settings</div>
    
    <div style="margin-bottom: 12px;">
      <label>Network Name:</label>
      <input type="text" value="VDO-Cal" disabled style="width:100%; padding:6px; margin-top:4px; background:#f0f0f0;">
    </div>

    <div style="margin-bottom: 12px;">
      <label>AP Password:</label>
      <input id="ap_pass" type="password" maxlength="64" style="width:100%; padding:6px; margin-top:4px;">
    </div>

    <div style="display: flex; gap: 8px; margin-top: 20px;">
      <button onclick="saveAPSettings()" style="flex:1; padding:8px; background:#28a745; color:white; border:none; border-radius:4px; cursor:pointer;">Save & Apply</button>
      <button onclick="closeAPModal()" style="flex:1; padding:8px; background:#999; color:white; border:none; border-radius:4px; cursor:pointer;">Cancel</button>
    </div>
  </div>
</div>

<!-- NMEA Input 1 Edit Modal -->
<div id="nmea1Modal" class="modal">
  <div class="modal-content">
    <span class="modal-close" onclick="closeNMEA1Modal()">&times;</span>
    <div class="modal-header">Edit NMEA Input 1</div>
    
    <div style="margin-bottom: 12px;">
      <label>Name:</label>
      <input id="p1_name" type="text" maxlength="32" style="width:100%; padding:6px; margin-top:4px;" placeholder="e.g. Yachta">
    </div>
    <div style="margin-bottom: 12px;">
      <label>Protocol:</label>
      <select id="p1_proto" style="width:100%; padding:6px; margin-top:4px;">
        <option value="tcp">TCP (connect to server)</option>
        <option value="udp">UDP (listen for broadcasts)</option>
        <option value="http">HTTP (poll sensor data)</option>
      </select>
    </div>
    <div style="margin-bottom: 12px;">
      <label>Host Address:</label>
      <input id="p1_host" type="text" maxlength="64" style="width:100%; padding:6px; margin-top:4px;" placeholder="e.g. 192.168.68.145">
    </div>
    <div style="margin-bottom: 12px;">
      <label>Port:</label>
      <input id="p1_port" type="number" min="1" max="65535" style="width:100%; padding:6px; margin-top:4px;" placeholder="e.g. 6666">
    </div>

    <div style="display: flex; gap: 8px; margin-top: 20px;">
      <button onclick="saveNMEA1Settings()" style="flex:1; padding:8px; background:#28a745; color:white; border:none; border-radius:4px; cursor:pointer;">Save & Apply</button>
      <button onclick="closeNMEA1Modal()" style="flex:1; padding:8px; background:#999; color:white; border:none; border-radius:4px; cursor:pointer;">Cancel</button>
    </div>
  </div>
</div>

<!-- NMEA Input 2 Edit Modal -->
<div id="nmea2Modal" class="modal">
  <div class="modal-content">
    <span class="modal-close" onclick="closeNMEA2Modal()">&times;</span>
    <div class="modal-header">Edit NMEA Input 2</div>
    
    <div style="margin-bottom: 12px;">
      <label>Name:</label>
      <input id="p2_name" type="text" maxlength="32" style="width:100%; padding:6px; margin-top:4px;" placeholder="e.g. OpenPlotter">
    </div>
    <div style="margin-bottom: 12px;">
      <label>Protocol:</label>
      <select id="p2_proto" style="width:100%; padding:6px; margin-top:4px;">
        <option value="tcp">TCP (connect to server)</option>
        <option value="udp">UDP (listen for broadcasts)</option>
        <option value="http">HTTP (poll sensor data)</option>
      </select>
    </div>
    <div style="margin-bottom: 12px;">
      <label>Host Address:</label>
      <input id="p2_host" type="text" maxlength="64" style="width:100%; padding:6px; margin-top:4px;" placeholder="e.g. 192.168.1.100">
    </div>
    <div style="margin-bottom: 12px;">
      <label>Port:</label>
      <input id="p2_port" type="number" min="1" max="65535" style="width:100%; padding:6px; margin-top:4px;" placeholder="e.g. 10110">
    </div>

    <div style="display: flex; gap: 8px; margin-top: 20px;">
      <button onclick="saveNMEA2Settings()" style="flex:1; padding:8px; background:#28a745; color:white; border:none; border-radius:4px; cursor:pointer;">Save & Apply</button>
      <button onclick="closeNMEA2Modal()" style="flex:1; padding:8px; background:#999; color:white; border:none; border-radius:4px; cursor:pointer;">Cancel</button>
    </div>
  </div>
</div>

<script>
// Modal control functions
function openEditWiFi() { document.getElementById('wifiModal').classList.add('show'); loadWiFiSettings(); }
function closeWiFiModal() { document.getElementById('wifiModal').classList.remove('show'); }
function openEditAP() { document.getElementById('apModal').classList.add('show'); loadAPSettings(); }
function closeAPModal() { document.getElementById('apModal').classList.remove('show'); }
function openEditNMEA1() { document.getElementById('nmea1Modal').classList.add('show'); loadNMEA1Settings(); }
function closeNMEA1Modal() { document.getElementById('nmea1Modal').classList.remove('show'); }
function openEditNMEA2() { document.getElementById('nmea2Modal').classList.add('show'); loadNMEA2Settings(); }
function closeNMEA2Modal() { document.getElementById('nmea2Modal').classList.remove('show'); }

// Load settings into modals
async function loadWiFiSettings() {
  try {
    const r = await fetch('/status');
    const j = await r.json();
    document.getElementById('wifi_mode_' + (j.wifi_mode || 0)).checked = true;
    document.getElementById('w1_ssid').value = j.w1_ssid || '';
    document.getElementById('w1_pass').value = j.w1_pass || '';
    document.getElementById('w2_ssid').value = j.w2_ssid || '';
    document.getElementById('w2_pass').value = j.w2_pass || '';
  } catch(e) { console.error('Load error:', e); }
}
async function loadAPSettings() {
  try {
    const r = await fetch('/status');
    const j = await r.json();
    document.getElementById('ap_pass').value = j.ap_pass || 'wind12345';
  } catch(e) { console.error('Load error:', e); }
}
async function loadNMEA1Settings() {
  try {
    const r = await fetch('/status');
    const j = await r.json();
    document.getElementById('p1_name').value = j.p1_name || 'Yachta';
    document.getElementById('p1_proto').value = (j.p1_proto || 'tcp').toLowerCase();
    document.getElementById('p1_host').value = j.p1_host || '192.168.68.145';
    document.getElementById('p1_port').value = j.p1_port || '6666';
  } catch(e) { console.error('Load error:', e); }
}
async function loadNMEA2Settings() {
  try {
    const r = await fetch('/status');
    const j = await r.json();
    document.getElementById('p2_name').value = j.p2_name || 'OpenPlotter';
    document.getElementById('p2_proto').value = (j.p2_proto || 'tcp').toLowerCase();
    document.getElementById('p2_host').value = j.p2_host || '';
    document.getElementById('p2_port').value = j.p2_port || '10110';
  } catch(e) { console.error('Load error:', e); }
}

// Save functions
async function saveWiFiSettings() {
  try {
    const r = await fetch('/status');
    const j = await r.json();
    const body = new URLSearchParams({
      wifi_mode: document.querySelector('input[name="wifi_mode"]:checked').value,
      w1_ssid: document.getElementById('w1_ssid').value,
      w1_pass: document.getElementById('w1_pass').value,
      w2_ssid: document.getElementById('w2_ssid').value,
      w2_pass: document.getElementById('w2_pass').value,
      ap_pass: j.ap_pass,
      p1_name: j.p1_name,
      p1_proto: j.p1_proto,
      p1_host: j.p1_host,
      p1_port: j.p1_port,
      p2_name: j.p2_name,
      p2_proto: j.p2_proto,
      p2_host: j.p2_host,
      p2_port: j.p2_port
    });
    await fetch('/savecfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
    await fetch('/reconnect');
    showMessage('WiFi settings saved!', 'success');
    closeWiFiModal();
    setTimeout(() => location.reload(), 1000);
  } catch(e) { showMessage('Error: ' + e.message, 'error'); }
}

async function saveAPSettings() {
  try {
    const r = await fetch('/status');
    const j = await r.json();
    const body = new URLSearchParams({
      ap_pass: document.getElementById('ap_pass').value,
      wifi_mode: j.wifi_mode,
      w1_ssid: j.w1_ssid,
      w1_pass: j.w1_pass,
      w2_ssid: j.w2_ssid,
      w2_pass: j.w2_pass,
      p1_name: j.p1_name,
      p1_proto: j.p1_proto,
      p1_host: j.p1_host,
      p1_port: j.p1_port,
      p2_name: j.p2_name,
      p2_proto: j.p2_proto,
      p2_host: j.p2_host,
      p2_port: j.p2_port
    });
    await fetch('/savecfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
    await fetch('/reconnect');
    showMessage('AP password saved!', 'success');
    closeAPModal();
    setTimeout(() => location.reload(), 1000);
  } catch(e) { showMessage('Error: ' + e.message, 'error'); }
}

async function saveNMEA1Settings() {
  try {
    const r = await fetch('/status');
    const j = await r.json();
    const body = new URLSearchParams({
      p1_name: document.getElementById('p1_name').value,
      p1_proto: document.getElementById('p1_proto').value,
      p1_host: document.getElementById('p1_host').value,
      p1_port: document.getElementById('p1_port').value,
      p2_name: j.p2_name,
      p2_proto: j.p2_proto,
      p2_host: j.p2_host,
      p2_port: j.p2_port,
      wifi_mode: j.wifi_mode,
      ap_pass: j.ap_pass,
      w1_ssid: j.w1_ssid,
      w1_pass: j.w1_pass,
      w2_ssid: j.w2_ssid,
      w2_pass: j.w2_pass
    });
    await fetch('/savecfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
    showMessage('NMEA Input 1 saved!', 'success');
    closeNMEA1Modal();
    setTimeout(() => location.reload(), 1000);
  } catch(e) { showMessage('Error: ' + e.message, 'error'); }
}

async function saveNMEA2Settings() {
  try {
    const r = await fetch('/status');
    const j = await r.json();
    const body = new URLSearchParams({
      p1_name: j.p1_name,
      p1_proto: j.p1_proto,
      p1_host: j.p1_host,
      p1_port: j.p1_port,
      p2_name: document.getElementById('p2_name').value,
      p2_proto: document.getElementById('p2_proto').value,
      p2_host: document.getElementById('p2_host').value,
      p2_port: document.getElementById('p2_port').value,
      wifi_mode: j.wifi_mode,
      ap_pass: j.ap_pass,
      w1_ssid: j.w1_ssid,
      w1_pass: j.w1_pass,
      w2_ssid: j.w2_ssid,
      w2_pass: j.w2_pass
    });
    await fetch('/savecfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
    showMessage('NMEA Input 2 saved!', 'success');
    closeNMEA2Modal();
    setTimeout(() => location.reload(), 1000);
  } catch(e) { showMessage('Error: ' + e.message, 'error'); }
}

function showMessage(text, type = 'info') {
  const existing = document.getElementById('notification');
  if (existing) existing.remove();
  const msg = document.createElement('div');
  msg.id = 'notification';
  msg.textContent = text;
  msg.style.cssText = `position: fixed; top: 20px; right: 20px; z-index: 1001; padding: 12px 16px; border-radius: 4px; color: white; font-weight: bold; background: ${type === 'success' ? '#28a745' : type === 'error' ? '#dc3545' : '#007bff'}; box-shadow: 0 4px 12px rgba(0,0,0,0.15);`;
  document.body.appendChild(msg);
  setTimeout(() => { if (msg.parentNode) msg.remove(); }, 3000);
}

// Close modals when clicking outside
window.addEventListener('click', (e) => {
  ['wifiModal', 'apModal', 'nmea1Modal', 'nmea2Modal'].forEach(id => {
    const modal = document.getElementById(id);
    if (e.target === modal) modal.classList.remove('show');
  });
});

// Load status data on page load
function loadStatusData() {
  fetch('/status')
    .then(r => r.json())
    .then(j => {
      // NMEA Input 1 (TCP)
      document.getElementById('nmea1_name').textContent = j.p1_name || 'Yachta';
      document.getElementById('nmea1_protocol').textContent = (j.p1_proto || 'tcp').toUpperCase();
      document.getElementById('nmea1_host').textContent = j.p1_host || '192.168.68.145';
      document.getElementById('nmea1_port').textContent = j.p1_port || '6666';
      document.getElementById('nmea1_status').textContent = j.tcp_connected ? '✓ Connected' : '✗ Disconnected';
      
      // NMEA Input 2 (UDP)
      document.getElementById('nmea2_name').textContent = j.p2_name || 'OpenPlotter';
      document.getElementById('nmea2_protocol').textContent = (j.p2_proto || 'udp').toUpperCase();
      document.getElementById('nmea2_host').textContent = j.p2_host || '';
      document.getElementById('nmea2_port').textContent = j.p2_port || '10110';
      document.getElementById('nmea2_status').textContent = j.udp_connected ? '✓ Listening' : '✗ Not listening';
    })
    .catch(e => console.error('Load error:', e));
}

// Load data when page loads
window.addEventListener('load', loadStatusData);
// Auto-refresh status every 2 seconds
setInterval(loadStatusData, 2000);
</script>
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
  <div style="margin-bottom: 20px; padding: 15px; background: #f0f0f0; border-radius: 4px;">
    <h4 style="margin-top: 0;">Active WiFi Profile</h4>
    <div class=kv>
      <label>Select WiFi Network</label>
      <div style="display: flex; gap: 20px;">
        <label><input type="radio" id="wifi_mode_0" name="wifi_mode" value="0" checked> 
          <strong>WiFi 1:</strong> <span id="w1_ssid_display">loading...</span></label>
        <label><input type="radio" id="wifi_mode_1" name="wifi_mode" value="1"> 
          <strong>WiFi 2:</strong> <span id="w2_ssid_display">loading...</span></label>
      </div>
    </div>
  </div>

  <!-- WiFi Profile 1 -->
  <div style="margin-top: 20px; padding: 12px; background: #e8f4f8; border-left: 4px solid #17a2b8; border-radius: 3px;">
    <h4 style="margin-top: 0; color: #17a2b8;">WiFi Profile 1 Configuration</h4>
    <div class=kv>
      <label>Network Name (SSID)</label>
      <input id=w1_ssid type=text maxlength=32 style="min-width:220px" placeholder="e.g. Kontu">
    </div>
    <div class=kv>
      <label>WiFi Password</label>
      <input id=w1_pass type=password maxlength=64 style="min-width:220px" placeholder="Enter password">
    </div>
  </div>

  <!-- WiFi Profile 2 -->
  <div style="margin-top: 20px; padding: 12px; background: #f0e8f8; border-left: 4px solid #9b59b6; border-radius: 3px;">
    <h4 style="margin-top: 0; color: #9b59b6;">WiFi Profile 2 Configuration</h4>
    <div class=kv>
      <label>Network Name (SSID)</label>
      <input id=w2_ssid type=text maxlength=32 style="min-width:220px" placeholder="e.g. OpenPlotter">
      <span class="info-icon" data-tooltip="Leave empty to disable this profile">i</span>
    </div>
    <div class=kv>
      <label>WiFi Password</label>
      <input id=w2_pass type=password maxlength=64 style="min-width:220px" placeholder="Enter password">
    </div>
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

<!-- NMEA Data Input Settings - Connection Profiles -->
<div class="section-header" style="border-left-color: #ffc107;">
  <h3 style="color: #e67e22;">NMEA Data Input - Connection Profiles</h3>
  <p>Configure two connection profiles and select which one to use</p>
</div>
<fieldset class="section-fieldset">
  <div style="margin-bottom: 20px; padding: 15px; background: #f0f0f0; border-radius: 4px;">
    <h4 style="margin-top: 0;">Active Profile</h4>
    <div class=kv>
      <label>Select Profile</label>
      <div style="display: flex; gap: 20px;">
        <label><input type="radio" id="conn_mode_0" name="conn_mode" value="0" checked onchange="updateProfileUI()"> 
          <strong>Profile 1:</strong> <span id="p1_name_display">loading...</span></label>
        <label><input type="radio" id="conn_mode_1" name="conn_mode" value="1" onchange="updateProfileUI()"> 
          <strong>Profile 2:</strong> <span id="p2_name_display">loading...</span></label>
      </div>
    </div>
  </div>

  <!-- Profile 1 -->
  <div style="margin-top: 20px; padding: 12px; background: #e8f4f8; border-left: 4px solid #17a2b8; border-radius: 3px;">
    <h4 style="margin-top: 0; color: #17a2b8;">Profile 1 Configuration</h4>
    <div class=kv>
      <label>Name</label>
      <input id=p1_name type=text maxlength=32 placeholder="e.g. Yachta" style="min-width:220px">
    </div>
    <div class=kv>
      <label>Protocol</label>
      <select id=p1_proto>
        <option value="udp">UDP (listen for broadcasts)</option>
        <option value="tcp">TCP (connect to server)</option>
        <option value="http">HTTP (poll sensor data)</option>
      </select>
    </div>
    <div class=kv>
      <label>Host Address</label>
      <input id=p1_host type=text maxlength=64 placeholder="e.g. 192.168.68.145" style="min-width:240px">
    </div>
    <div class=kv>
      <label>Port</label>
      <input id=p1_port type=number min=1 max=65535 step=1 style="width:140px" placeholder="e.g. 6666">
    </div>
  </div>

  <!-- Profile 2 -->
  <div style="margin-top: 20px; padding: 12px; background: #f0e8f8; border-left: 4px solid #9b59b6; border-radius: 3px;">
    <h4 style="margin-top: 0; color: #9b59b6;">Profile 2 Configuration</h4>
    <div class=kv>
      <label>Name</label>
      <input id=p2_name type=text maxlength=32 placeholder="e.g. OpenPlotter" style="min-width:220px">
    </div>
    <div class=kv>
      <label>Protocol</label>
      <select id=p2_proto>
        <option value="udp">UDP (listen for broadcasts)</option>
        <option value="tcp">TCP (connect to server)</option>
        <option value="http">HTTP (poll sensor data)</option>
      </select>
    </div>
    <div class=kv>
      <label>Host Address</label>
      <input id=p2_host type=text maxlength=64 placeholder="e.g. 192.168.1.100" style="min-width:240px">
      <span class="info-icon" data-tooltip="Leave empty to disable this profile">i</span>
    </div>
    <div class=kv>
      <label>Port</label>
      <input id=p2_port type=number min=1 max=65535 step=1 style="width:140px" placeholder="e.g. 10110">
    </div>
  </div>
</fieldset>

<div class=row style="margin-top: 20px;">
  <button onclick="saveAndReconnect()">Save & Apply All Settings</button>
</div>

<script>
function updateProfileUI() {
  // This can be used for future UI updates based on selected profile
}

async function saveCfg(){
  try {
    showMessage('Saving settings...', 'info');
    
    const ap_pass = document.getElementById('ap_pass').value;
    const conn_mode = document.querySelector('input[name="conn_mode"]:checked').value;
    const wifi_mode = document.querySelector('input[name="wifi_mode"]:checked').value;
    
    // WiFi profiles
    const w1_ssid = document.getElementById('w1_ssid').value;
    const w1_pass = document.getElementById('w1_pass').value;
    const w2_ssid = document.getElementById('w2_ssid').value;
    const w2_pass = document.getElementById('w2_pass').value;
    
    // Connection profiles
    const p1_name = document.getElementById('p1_name').value;
    const p1_proto = document.getElementById('p1_proto').value;
    const p1_host = document.getElementById('p1_host').value;
    const p1_port = document.getElementById('p1_port').value;
    
    const p2_name = document.getElementById('p2_name').value;
    const p2_proto = document.getElementById('p2_proto').value;
    const p2_host = document.getElementById('p2_host').value;
    const p2_port = document.getElementById('p2_port').value;
    
    const body = new URLSearchParams({
      ap_pass, conn_mode, wifi_mode,
      w1_ssid, w1_pass, w2_ssid, w2_pass,
      p1_name, p1_proto, p1_host, p1_port,
      p2_name, p2_proto, p2_host, p2_port
    });
    
    await fetch('/savecfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
    showMessage('Settings saved successfully!', 'success');
    setTimeout(() => loadNetworkValues(), 300);
  } catch(e) {
    showMessage('Error saving settings: ' + e.message, 'error');
  }
}

async function saveAndReconnect(){
  await saveCfg();
  await fetch('/reconnect');
  await fetch('/reconnecttcp');
}

// Load current values from /status API
function loadNetworkValues() {
  fetch('/status')
    .then(r => r.json())
    .then(j => {
      console.log('Status JSON:', j);
      
      // WiFi profile selection
      const wifiMode = j.wifi_mode !== undefined ? j.wifi_mode : 0;
      const wifiRadio = document.getElementById('wifi_mode_' + wifiMode);
      if (wifiRadio) wifiRadio.checked = true;
      
      // WiFi Profile 1
      const w1s = j.w1_ssid || 'Kontu';
      const d1s = document.getElementById('w1_ssid_display');
      if (d1s) d1s.textContent = w1s;
      const i1s = document.getElementById('w1_ssid');
      if (i1s) i1s.value = w1s;
      const i1p = document.getElementById('w1_pass');
      if (i1p) i1p.value = j.w1_pass || '';
      
      // WiFi Profile 2
      const w2s = j.w2_ssid || '';
      const d2s = document.getElementById('w2_ssid_display');
      if (d2s) d2s.textContent = w2s || '(empty)';
      const i2s = document.getElementById('w2_ssid');
      if (i2s) i2s.value = w2s;
      const i2p = document.getElementById('w2_pass');
      if (i2p) i2p.value = j.w2_pass || '';
      
      // Connection profile selection
      const connMode = j.conn_mode !== undefined ? j.conn_mode : 0;
      const connRadio = document.getElementById('conn_mode_' + connMode);
      if (connRadio) connRadio.checked = true;
      
      // Profile 1
      const p1n = j.p1_name || 'Yachta';
      const d1 = document.getElementById('p1_name_display');
      if (d1) d1.textContent = p1n;
      const i1 = document.getElementById('p1_name');
      if (i1) i1.value = p1n;
      const i1pp = document.getElementById('p1_proto');
      if (i1pp) i1pp.value = j.p1_proto || 'tcp';
      const i1h = document.getElementById('p1_host');
      if (i1h) i1h.value = j.p1_host || '192.168.68.145';
      const i1pt = document.getElementById('p1_port');
      if (i1pt) i1pt.value = j.p1_port || '6666';
      
      // Profile 2
      const p2n = j.p2_name || 'OpenPlotter';
      const d2 = document.getElementById('p2_name_display');
      if (d2) d2.textContent = p2n;
      const i2 = document.getElementById('p2_name');
      if (i2) i2.value = p2n;
      const i2pp = document.getElementById('p2_proto');
      if (i2pp) i2pp.value = j.p2_proto || 'tcp';
      const i2h = document.getElementById('p2_host');
      if (i2h) i2h.value = j.p2_host || '';
      const i2pt = document.getElementById('p2_port');
      if (i2pt) i2pt.value = j.p2_port || '10110';
      
      console.log('Loaded: W1=' + w1s + ', W2=' + w2s + ', P1=' + p1n + ', P2=' + p2n);
    })
    .catch(e => {
      console.error('Load error:', e);
    });
}

// Save to localStorage when changed (for UI refresh)
function saveToLocalStorage() {
  const radioVal = document.querySelector('input[name="conn_mode"]:checked');
  if (radioVal) localStorage.setItem('conn_mode', radioVal.value);
}

// Näyttää ilmoituksen käyttäjälle
function showMessage(text, type = 'info') {
  // Poista vanha ilmoitus
  const existing = document.getElementById('notification');
  if (existing) existing.remove();
  
  // Luo uusi ilmoitus
  const msg = document.createElement('div');
  msg.id = 'notification';
  msg.textContent = text;
  msg.style.cssText = `
    position: fixed; top: 20px; right: 20px; z-index: 1000;
    padding: 12px 16px; border-radius: 4px; color: white; font-weight: bold;
    background: ${type === 'success' ? '#28a745' : type === 'error' ? '#dc3545' : '#007bff'};
    box-shadow: 0 4px 12px rgba(0,0,0,0.15);
  `;
  
  document.body.appendChild(msg);
  
  // Poista automaattisesti 3 sekunnin kuluttua
  setTimeout(() => {
    if (msg.parentNode) msg.remove();
  }, 3000);
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
<fieldset id="display_basic_fields">
  <legend>)HTML" + displayTitle + R"HTML( Settings</legend>
  
  <div class=row>
    <label><input type=checkbox id=displayEnabled onchange="setDisplayEnabled()"> Enable )HTML" + displayTitle + R"HTML(</label>
  </div>
  
  <div class="row display_config_row">
    <label>Meter Type</label>
    <select id=displayType onchange="updateDisplayFields()">
      <option value="logicwind">Logic Wind</option>
      <option value="sumlog">Sumlog</option>
    </select>
    <span class="info-icon" data-tooltip="Logic Wind: Direction + speed display. Sumlog: Speed-only display">i</span>
  </div>
  
  <div class="row display_config_row">
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

<div class=row style="margin-top: 20px;">
  <button onclick="saveDisplaySettings()" style="background:#28a745;color:white;padding:8px 16px;">Save )HTML" + displayTitle + R"HTML( Settings</button>
</div>

<script>
const DISPLAY_NUM = )HTML" + String(displayNum) + R"HTML(;

function updateDisplayFields() {
  const enabled = document.getElementById('displayEnabled').checked;
  const type = document.getElementById('displayType').value;
  
  // Basic config rows (Meter Type, NMEA Sentence): show only when enabled
  const configRows = document.querySelectorAll('.display_config_row');
  configRows.forEach(row => {
    row.style.display = enabled ? '' : 'none';
  });
  
  // 1. WIND DIRECTION: Show only for Logic Wind type when enabled
  const showDirection = (enabled && type === 'logicwind');
  document.getElementById('wind_direction_header').style.display = showDirection ? '' : 'none';
  document.getElementById('logicwind_direction_fields').style.display = showDirection ? '' : 'none';
  
  // 2. PULSE: Show always when enabled
  const showPulse = enabled;
  document.getElementById('pulse_fields').style.display = showPulse ? '' : 'none';
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
  
  html += buildPageFooter();
  return html;
}
