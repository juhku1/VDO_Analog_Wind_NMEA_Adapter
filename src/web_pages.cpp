// web_pages_multi.cpp - LITE Multi: 3 speed pulses + global direction

#include "web_ui.h"
#include <Arduino.h>

// Helper: Format time ago
String formatTimeAgo(uint32_t lastUpdate_ms) {
  if (lastUpdate_ms == 0) return "-";
  
  uint32_t now = millis();
  uint32_t age_ms = now - lastUpdate_ms;
  
  if (age_ms < 1000) return "Just now";
  if (age_ms < 60000) return String(age_ms / 1000) + "s ago";
  if (age_ms < 3600000) return String(age_ms / 60000) + "m ago";
  return String(age_ms / 3600000) + "h ago";
}

// LITE Multi: Single-page UI with 3 speed pulses + global direction
String buildSinglePage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>VDO Wind Adapter - LITE Multi</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { 
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #eaf6fb;
      padding: 20px;
      line-height: 1.6;
    }
    .container { max-width: 1000px; margin: 0 auto; }
    .header-card {
      background: white;
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 20px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
      text-align: center;
    }
    .card {
      background: white;
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 20px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
    }
    h1 { 
      color: #2c3e50;
      margin-bottom: 5px;
      font-size: 28px;
      font-weight: 600;
    }
    .tagline {
      color: #7f8c8d;
      font-size: 14px;
      margin-top: -5px;
      margin-bottom: 10px;
    }
    h2 {
      color: #34495e;
      margin-bottom: 15px;
      font-size: 18px;
      border-bottom: 2px solid #3498db;
      padding-bottom: 5px;
    }
    h3 {
      color: #34495e;
      margin: 20px 0 10px 0;
      font-size: 16px;
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 15px;
      margin-bottom: 20px;
    }
    .status-item {
      background: #ecf0f1;
      padding: 15px;
      border-radius: 6px;
    }
    .status-label {
      font-size: 12px;
      color: #7f8c8d;
      text-transform: uppercase;
      margin-bottom: 5px;
    }
    .status-value {
      font-size: 24px;
      font-weight: bold;
      color: #2c3e50;
    }
    .status-unit {
      font-size: 14px;
      color: #95a5a6;
      margin-left: 5px;
    }
    .form-group {
      margin-bottom: 15px;
    }
    label {
      display: block;
      margin-bottom: 5px;
      color: #34495e;
      font-weight: 500;
    }
    input[type="text"],
    input[type="number"],
    input[type="password"],
    select {
      width: 100%;
      padding: 10px;
      border: 1px solid #ddd;
      border-radius: 4px;
      font-size: 14px;
    }
    input[type="checkbox"] {
      width: 20px;
      height: 20px;
      margin-right: 10px;
      vertical-align: middle;
    }
    button {
      background: #3498db;
      color: white;
      border: none;
      padding: 12px 24px;
      border-radius: 6px;
      cursor: pointer;
      font-size: 14px;
      font-weight: 500;
      transition: all 0.3s;
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    }
    button:hover { 
      background: #2980b9;
      box-shadow: 0 4px 8px rgba(0,0,0,0.15);
      transform: translateY(-1px);
    }
    button:disabled {
      background: #95a5a6;
      cursor: not-allowed;
    }
    .btn-secondary {
      background: #95a5a6;
    }
    .btn-secondary:hover {
      background: #7f8c8d;
    }
    .status-ok { color: #27ae60; }
    .status-error { color: #e74c3c; }
    .status-warning { color: #f39c12; }
    .info-text {
      font-size: 13px;
      color: #7f8c8d;
      margin-top: 5px;
    }
    .button-group {
      display: flex;
      gap: 10px;
      margin-top: 20px;
    }
    .pulse-section {
      border: 1px solid #ddd;
      border-radius: 6px;
      padding: 15px;
      margin-bottom: 15px;
      background: #f9f9f9;
    }
    .pulse-section.disabled {
      opacity: 0.6;
    }
    .tabs {
      display: flex;
      gap: 0;
      margin-bottom: 20px;
      border-bottom: 2px solid #ddd;
    }
    .tab {
      padding: 12px 24px;
      background: #ecf0f1;
      border: none;
      cursor: pointer;
      font-size: 16px;
      font-weight: 500;
      color: #7f8c8d;
      border-radius: 8px 8px 0 0;
      transition: all 0.3s;
    }
    .tab:hover {
      background: #d5dbdb;
    }
    .tab.active {
      background: white;
      color: #2c3e50;
      border-bottom: 2px solid white;
      margin-bottom: -2px;
    }
    .tab-content {
      display: none;
    }
    .tab-content.active {
      display: block;
    }
    .footer {
      text-align: center;
      padding: 20px;
      margin-top: 30px;
      color: #7f8c8d;
      font-size: 13px;
      border-top: 1px solid #ddd;
      background: white;
      border-radius: 8px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
    }
    .footer a {
      color: #3498db;
      text-decoration: none;
    }
    .footer a:hover {
      text-decoration: underline;
    }
    .data-section {
      border: 1px solid #e0e0e0;
      border-radius: 8px;
      padding: 15px;
      margin-bottom: 15px;
      background: #f9f9f9;
    }
    .data-section h3 {
      margin: 0 0 12px 0;
      color: #2c3e50;
      font-size: 16px;
      font-weight: 600;
    }
    .data-row {
      display: flex;
      gap: 30px;
      margin-bottom: 8px;
      flex-wrap: wrap;
    }
    .data-row > div {
      display: flex;
      align-items: baseline;
      gap: 6px;
    }
    .data-label {
      font-size: 13px;
      color: #7f8c8d;
      font-weight: 500;
    }
    .data-value {
      font-size: 22px;
      font-weight: 600;
      color: #2c3e50;
    }
    .data-unit {
      font-size: 14px;
      color: #95a5a6;
    }
    .data-source {
      font-size: 12px;
      color: #7f8c8d;
      margin-top: 8px;
      display: flex;
      gap: 8px;
      align-items: center;
      flex-wrap: wrap;
    }
    .badge {
      display: inline-block;
      padding: 3px 8px;
      border-radius: 4px;
      font-size: 11px;
      font-weight: 600;
      text-transform: uppercase;
    }
    .badge-tcp { background: #3498db; color: white; }
    .badge-udp { background: #9b59b6; color: white; }
    .badge-calc { background: #e67e22; color: white; }
    .badge-nmea { background: #27ae60; color: white; }
    .badge-warning { background: #f39c12; color: white; }
    .nmea-raw {
      font-family: 'Courier New', monospace;
      font-size: 11px;
      background: #ecf0f1;
      padding: 2px 6px;
      border-radius: 3px;
      color: #2c3e50;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header-card">
      <h1>🌬️ VDO Wind Adapter</h1>
      <p class="tagline">Bring your analog sensor into the digital age</p>
    </div>
    
    <!-- Tabs -->
    <div class="tabs">
      <button class="tab active" onclick="switchTab('data')">📊 Data Config</button>
      <button class="tab" onclick="switchTab('network')">🌐 Network</button>
    </div>

    <!-- Data Config Tab -->
    <div id="dataTab" class="tab-content active">

    <!-- Status Section -->
    <div class="card">
      <h2>📊 Current Status</h2>
      
      <!-- Apparent Wind -->
      <div class="data-section">
        <h3>🌬️ Apparent Wind</h3>
        <div class="data-row">
          <div>
            <span class="data-label">Speed:</span>
            <span class="data-value" id="aws-speed">--</span>
            <span class="data-unit" id="speed-unit">kn</span>
          </div>
          <div>
            <span class="data-label">Angle:</span>
            <span class="data-value" id="aws-angle">--</span>
            <span class="data-unit">°</span>
          </div>
        </div>
        <div class="data-source">
          <span class="badge badge-nmea" id="aws-sentence">--</span>
          <span class="badge badge-tcp" id="aws-connection">--</span>
          <span id="aws-time">No data</span>
        </div>
      </div>
      
      <!-- True Wind -->
      <div class="data-section">
        <h3>🧭 True Wind</h3>
        <div class="data-row">
          <div>
            <span class="data-label">Speed:</span>
            <span class="data-value" id="tws-speed">--</span>
            <span class="data-unit">kn</span>
          </div>
          <div>
            <span class="data-label">Angle:</span>
            <span class="data-value" id="tws-angle">--</span>
            <span class="data-unit">°</span>
          </div>
        </div>
        <div class="data-source">
          <span class="badge badge-calc" id="tws-source">--</span>
          <span id="tws-formula"></span>
          <span id="tws-time">No data</span>
        </div>
      </div>
      
      <!-- GPS Data -->
      <div class="data-section">
        <h3>📍 GPS Data</h3>
        <div class="data-row">
          <div>
            <span class="data-label">SOG:</span>
            <span class="data-value" id="gps-sog">--</span>
            <span class="data-unit">kn</span>
          </div>
          <div>
            <span class="data-label">COG:</span>
            <span class="data-value" id="gps-cog">--</span>
            <span class="data-unit">°</span>
          </div>
          <div>
            <span class="data-label">HDG:</span>
            <span class="data-value" id="gps-hdg">--</span>
            <span class="data-unit">°</span>
          </div>
        </div>
        <div class="data-source">
          <span class="badge badge-nmea" id="gps-sentence">--</span>
          <span class="badge badge-udp" id="gps-connection">--</span>
          <span id="gps-time">No data</span>
        </div>
      </div>
      
      <!-- VMG -->
      <div class="data-section">
        <h3>📈 VMG (Velocity Made Good)</h3>
        <div class="data-row">
          <div>
            <span class="data-value" id="vmg-value">--</span>
            <span class="data-unit">kn</span>
          </div>
        </div>
        <div class="data-source">
          <span class="badge badge-calc">Calculated</span>
          <span>SOG × cos(TWA)</span>
          <span id="vmg-time">No data</span>
        </div>
      </div>
      
      <!-- Connections -->
      <div class="data-section">
        <h3>🔌 Connections</h3>
        <div class="data-row">
          <div>
            <span id="tcp-status">TCP: ✗</span>
          </div>
          <div>
            <span id="udp-status">UDP: ✗</span>
          </div>
        </div>
        <div class="data-source" style="margin-bottom: 8px;">
          <span class="badge badge-tcp">TCP</span>
          <span class="nmea-raw" id="last-tcp-nmea">--</span>
          <span id="tcp-nmea-time"></span>
        </div>
        <div class="data-source">
          <span class="badge badge-udp">UDP</span>
          <span class="nmea-raw" id="last-udp-nmea">--</span>
          <span id="udp-nmea-time"></span>
        </div>
      </div>
    </div>

    <!-- Direction Configuration -->
    <div class="card">
      <h2>🧭 Direction Output (shared by all Logic Wind instruments)</h2>
      <form id="directionForm">
        <div class="form-group">
          <label for="directionSource">Direction Source</label>
          <select id="directionSource">
            <option value="0">Apparent Wind Angle</option>
            <option value="1">True Wind Angle</option>
            <option value="3">Course Over Ground (COG)</option>
          </select>
          <p class="info-text">All Logic Wind instruments will show this direction via DAC output.</p>
        </div>
        
        <div class="form-group">
          <label for="directionOffset">Direction Offset (degrees)</label>
          <input type="number" id="directionOffset" min="-180" max="180" value="0">
          <p class="info-text">Calibration offset for direction output (-180 to +180)</p>
        </div>
        
        <button type="submit">💾 Save Direction Settings</button>
      </form>
    </div>

    <!-- Speed Pulse Outputs -->
    <div class="card">
      <h2>⚡ Speed Pulse Outputs</h2>
      
      <!-- Speed Pulse 1 -->
      <div class="pulse-section" id="pulse1Section">
        <h3>Speed Pulse 1</h3>
        <form id="pulse1Form">
          <div class="form-group">
            <label>
              <input type="checkbox" id="pulse1_enabled">
              Enable Output
            </label>
          </div>
          
          <div class="form-group">
            <label for="pulse1_instrumentType">Instrument Type</label>
            <select id="pulse1_instrumentType">
              <option value="sumlog">Sumlog (speed only)</option>
              <option value="logicwind">Logic Wind (speed + direction)</option>
            </select>
          </div>
          
          <div class="form-group">
            <label for="pulse1_speedSource">Speed Source</label>
            <select id="pulse1_speedSource">
              <option value="0">Apparent Wind Speed</option>
              <option value="1">True Wind Speed</option>
              <option value="2">Speed Over Ground (SOG)</option>
              <option value="4">VMG (Velocity Made Good)</option>
            </select>
          </div>
          
          <div class="form-group">
            <label for="pulse1_pulsePin">GPIO Pin</label>
            <input type="number" id="pulse1_pulsePin" min="0" max="39">
          </div>
          
          <div class="form-group">
            <label for="pulse1_pulsesPerKnot">Pulses per Knot</label>
            <input type="number" id="pulse1_pulsesPerKnot" step="0.1" min="0.1" max="100">
          </div>
          
          <div class="form-group">
            <label for="pulse1_maxFrequency">Max Frequency (Hz)</label>
            <input type="number" id="pulse1_maxFrequency" min="10" max="1000">
          </div>
          
          <div class="form-group">
            <label for="pulse1_dutyCycle">Duty Cycle (%)</label>
            <input type="number" id="pulse1_dutyCycle" min="1" max="99">
          </div>
          
          <button type="submit">💾 Save Pulse 1</button>
        </form>
      </div>

      <!-- Speed Pulse 2 -->
      <div class="pulse-section" id="pulse2Section">
        <h3>Speed Pulse 2</h3>
        <form id="pulse2Form">
          <div class="form-group">
            <label>
              <input type="checkbox" id="pulse2_enabled">
              Enable Output
            </label>
          </div>
          
          <div class="form-group">
            <label for="pulse2_instrumentType">Instrument Type</label>
            <select id="pulse2_instrumentType">
              <option value="sumlog">Sumlog (speed only)</option>
              <option value="logicwind">Logic Wind (speed + direction)</option>
            </select>
          </div>
          
          <div class="form-group">
            <label for="pulse2_speedSource">Speed Source</label>
            <select id="pulse2_speedSource">
              <option value="0">Apparent Wind Speed</option>
              <option value="1">True Wind Speed</option>
              <option value="2">Speed Over Ground (SOG)</option>
              <option value="4">VMG (Velocity Made Good)</option>
            </select>
          </div>
          
          <div class="form-group">
            <label for="pulse2_pulsePin">GPIO Pin</label>
            <input type="number" id="pulse2_pulsePin" min="0" max="39">
          </div>
          
          <div class="form-group">
            <label for="pulse2_pulsesPerKnot">Pulses per Knot</label>
            <input type="number" id="pulse2_pulsesPerKnot" step="0.1" min="0.1" max="100">
          </div>
          
          <div class="form-group">
            <label for="pulse2_maxFrequency">Max Frequency (Hz)</label>
            <input type="number" id="pulse2_maxFrequency" min="10" max="1000">
          </div>
          
          <div class="form-group">
            <label for="pulse2_dutyCycle">Duty Cycle (%)</label>
            <input type="number" id="pulse2_dutyCycle" min="1" max="99">
          </div>
          
          <button type="submit">💾 Save Pulse 2</button>
        </form>
      </div>

      <!-- Speed Pulse 3 -->
      <div class="pulse-section" id="pulse3Section">
        <h3>Speed Pulse 3</h3>
        <form id="pulse3Form">
          <div class="form-group">
            <label>
              <input type="checkbox" id="pulse3_enabled">
              Enable Output
            </label>
          </div>
          
          <div class="form-group">
            <label for="pulse3_instrumentType">Instrument Type</label>
            <select id="pulse3_instrumentType">
              <option value="sumlog">Sumlog (speed only)</option>
              <option value="logicwind">Logic Wind (speed + direction)</option>
            </select>
          </div>
          
          <div class="form-group">
            <label for="pulse3_speedSource">Speed Source</label>
            <select id="pulse3_speedSource">
              <option value="0">Apparent Wind Speed</option>
              <option value="1">True Wind Speed</option>
              <option value="2">Speed Over Ground (SOG)</option>
              <option value="4">VMG (Velocity Made Good)</option>
            </select>
          </div>
          
          <div class="form-group">
            <label for="pulse3_pulsePin">GPIO Pin</label>
            <input type="number" id="pulse3_pulsePin" min="0" max="39">
          </div>
          
          <div class="form-group">
            <label for="pulse3_pulsesPerKnot">Pulses per Knot</label>
            <input type="number" id="pulse3_pulsesPerKnot" step="0.1" min="0.1" max="100">
          </div>
          
          <div class="form-group">
            <label for="pulse3_maxFrequency">Max Frequency (Hz)</label>
            <input type="number" id="pulse3_maxFrequency" min="10" max="1000">
          </div>
          
          <div class="form-group">
            <label for="pulse3_dutyCycle">Duty Cycle (%)</label>
            <input type="number" id="pulse3_dutyCycle" min="1" max="99">
          </div>
          
          <button type="submit">💾 Save Pulse 3</button>
        </form>
      </div>
    </div>

    </div> <!-- End Data Config Tab -->

    <!-- Network Tab -->
    <div id="networkTab" class="tab-content">

    <!-- Network Configuration -->
    <div class="card">
      <h2>🌐 Network Configuration</h2>
      <form id="networkForm">
        <div class="form-group">
          <label for="ssid">WiFi SSID</label>
          <input type="text" id="ssid" value=")rawliteral";
  
  html += String(sta_ssid);
  
  html += R"rawliteral(">
        </div>
        
        <div class="form-group">
          <label for="password">WiFi Password</label>
          <input type="password" id="password" placeholder="Leave blank to keep current">
        </div>
        
        <h3>Display Preferences</h3>
        
        <div class="form-group">
          <label for="windUnit">Wind Speed Unit</label>
          <select id="windUnit">
            <option value="0">Knots (kn)</option>
            <option value="1">Meters per second (m/s)</option>
          </select>
          <p class="info-text">ℹ️ Choose how wind speeds are displayed in the UI</p>
        </div>
        
        <h3>Access Point (AP) Settings</h3>
        
        <div class="form-group">
          <label for="apPassword">AP Password</label>
          <input type="password" id="apPassword" placeholder="Leave blank to keep current (min 8 characters)">
          <p class="info-text">⚠️ Change the default AP password (wind12345) for security</p>
        </div>
        
        <h3>TCP Connection</h3>
        
        <div class="form-group">
          <label for="nmeaHost">TCP Host</label>
          <input type="text" id="nmeaHost" value=")rawliteral";
  
  html += String(nmeaHost);
  
  html += R"rawliteral(">
        </div>
        
        <div class="form-group">
          <label for="nmeaPort">TCP Port</label>
          <input type="number" id="nmeaPort" value=")rawliteral";
  
  html += String(nmeaPort);
  
  html += R"rawliteral(" min="1" max="65535">
        </div>
        
        <h3>UDP Listener</h3>
        
        <div class="form-group">
          <label for="udpPort">UDP Port</label>
          <input type="number" id="udpPort" value=")rawliteral";
  
  Preferences p;
  p.begin("cfg", true);
  uint16_t udpPort = p.getUShort("p2_port", 10110);
  p.end();
  html += String(udpPort);
  
  html += R"rawliteral(" min="1" max="65535">
        </div>
        
        <div class="form-group">
          <p class="info-text">
            ℹ️ <strong>Dual Source:</strong> Both TCP and UDP connections are active simultaneously.
            TCP connects to the specified host/port. UDP listens on the configured port for broadcasts.
          </p>
        </div>
        
        <div class="button-group">
          <button type="submit">💾 Save Network Settings</button>
          <button type="button" class="btn-secondary" onclick="location.reload()">🔄 Refresh</button>
        </div>
      </form>
    </div>

    </div> <!-- End Network Tab -->

    <!-- Footer -->
    <div class="footer">
      <p>Created by Juha-Matti Mäntylä • MIT Licensed</p>
      <p><a href="https://github.com/juhku1/VDO_Analog_Wind_NMEA_Adapter" target="_blank">View on GitHub</a></p>
    </div>

  </div> <!-- End container -->

  <script>
    // Tab switching
    function switchTab(tabName) {
      // Hide all tabs
      document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
      });
      document.querySelectorAll('.tab').forEach(tab => {
        tab.classList.remove('active');
      });
      
      // Show selected tab
      if (tabName === 'data') {
        document.getElementById('dataTab').classList.add('active');
        document.querySelectorAll('.tab')[0].classList.add('active');
      } else if (tabName === 'network') {
        document.getElementById('networkTab').classList.add('active');
        document.querySelectorAll('.tab')[1].classList.add('active');
      }
    }
    
    // Load initial configuration
    async function loadConfig() {
      try {
        // Load status to get wind unit preference
        const statusResp = await fetch('/status');
        const statusData = await statusResp.json();
        if (statusData.wind_unit !== undefined) {
          document.getElementById('windUnit').value = statusData.wind_unit;
          window.windSpeedUnit = statusData.wind_unit;  // Store globally for display
        }
        
        // Load direction source and offset
        const dirResp = await fetch('/api/direction');
        const dirData = await dirResp.json();
        if (dirData.source !== undefined) {
          document.getElementById('directionSource').value = dirData.source;
        }
        if (dirData.offset !== undefined) {
          document.getElementById('directionOffset').value = dirData.offset;
        }
        
        // Load pulse configurations
        for (let i = 1; i <= 3; i++) {
          const resp = await fetch(`/api/display?num=${i}`);
          const data = await resp.json();
          
          document.getElementById(`pulse${i}_enabled`).checked = data.enabled;
          document.getElementById(`pulse${i}_instrumentType`).value = data.type || 'sumlog';
          document.getElementById(`pulse${i}_speedSource`).value = data.speedSource || 0;
          document.getElementById(`pulse${i}_pulsePin`).value = data.pulsePin || (10 + i * 2);
          document.getElementById(`pulse${i}_pulsesPerKnot`).value = data.pulsesPerKnot || 1.0;
          document.getElementById(`pulse${i}_maxFrequency`).value = data.maxFrequency || 150;
          document.getElementById(`pulse${i}_dutyCycle`).value = data.dutyCycle || 10;
          
          // Update section styling
          const section = document.getElementById(`pulse${i}Section`);
          if (!data.enabled) {
            section.classList.add('disabled');
          }
        }
      } catch (error) {
        console.error('Failed to load config:', error);
      }
    }
    
    // Direction form submission
    document.getElementById('directionForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const dirSource = document.getElementById('directionSource').value;
      const dirOffset = document.getElementById('directionOffset').value;
      
      try {
        const response = await fetch('/api/direction', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: `source=${dirSource}&offset=${dirOffset}`
        });
        if (response.ok) {
          alert('✅ Direction settings saved!');
        } else {
          alert('❌ Failed to save direction settings');
        }
      } catch (error) {
        alert('❌ Error: ' + error.message);
      }
    });
    
    // Pulse form submissions
    for (let i = 1; i <= 3; i++) {
      document.getElementById(`pulse${i}Form`).addEventListener('submit', async (e) => {
        e.preventDefault();
        const formData = new URLSearchParams({
          action: 'save',
          num: i,
          enabled: document.getElementById(`pulse${i}_enabled`).checked ? '1' : '0',
          type: document.getElementById(`pulse${i}_instrumentType`).value,
          speedSource: document.getElementById(`pulse${i}_speedSource`).value,
          pulsePin: document.getElementById(`pulse${i}_pulsePin`).value,
          pulsesPerKnot: document.getElementById(`pulse${i}_pulsesPerKnot`).value,
          maxFrequency: document.getElementById(`pulse${i}_maxFrequency`).value,
          dutyCycle: document.getElementById(`pulse${i}_dutyCycle`).value
        });
        
        try {
          const response = await fetch('/api/display?' + formData, { method: 'POST' });
          if (response.ok) {
            alert(`✅ Speed Pulse ${i} saved!`);
            setTimeout(() => location.reload(), 1000);
          } else {
            alert(`❌ Failed to save Speed Pulse ${i}`);
          }
        } catch (error) {
          alert('❌ Error: ' + error.message);
        }
      });
    }
    
    // Network form submission
    document.getElementById('networkForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const formData = new URLSearchParams({
        sta_ssid: document.getElementById('ssid').value,
        sta_pass: document.getElementById('password').value,
        ap_pass: document.getElementById('apPassword').value,
        wind_unit: document.getElementById('windUnit').value,
        p1_host: document.getElementById('nmeaHost').value,
        p1_port: document.getElementById('nmeaPort').value,
        p1_proto: document.getElementById('nmeaProto').value,
        p2_port: document.getElementById('udpPort').value
      });
      
      try {
        const response = await fetch('/savecfg', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: formData
        });
        if (response.ok) {
          alert('✅ Network settings saved! Device will restart...');
          setTimeout(() => location.reload(), 3000);
        } else {
          alert('❌ Failed to save settings');
        }
      } catch (error) {
        alert('❌ Error: ' + error.message);
      }
    });
    
    // Auto-refresh status every 2 seconds
    setInterval(async () => {
      try {
        const resp = await fetch('/api/dataflow');
        const data = await resp.json();
        
        // Helper: format time ago (age in milliseconds)
        const formatTime = (age) => {
          if (!age || age > 900000) return 'No data';
          if (age < 1000) return 'Just now';
          if (age < 60000) return Math.floor(age/1000) + 's ago';
          if (age < 3600000) return Math.floor(age/60000) + 'm ago';
          return Math.floor(age/3600000) + 'h ago';
        };
        
        // Helper: convert speed based on unit preference (0=knots, 1=m/s)
        const convertSpeed = (knots) => {
          if (window.windSpeedUnit === 1) {
            return (knots * 0.514444).toFixed(1);  // knots to m/s
          }
          return knots.toFixed(1);
        };
        
        // Update unit labels
        const unitText = window.windSpeedUnit === 1 ? 'm/s' : 'kn';
        document.querySelectorAll('.data-unit').forEach(el => {
          if (el.textContent === 'kn' || el.textContent === 'm/s') {
            el.textContent = unitText;
          }
        });
        
        // Update Apparent Wind
        if (data.apparent && data.apparent.hasData) {
          document.getElementById('aws-speed').textContent = convertSpeed(data.apparent.speed || 0);
          document.getElementById('aws-angle').textContent = (data.apparent.angle || 0).toFixed(0);
          document.getElementById('aws-sentence').textContent = data.apparent.source || 'MWV(R)';
          document.getElementById('aws-connection').textContent = data.apparent.connection || 'TCP';
          document.getElementById('aws-connection').className = 'badge badge-' + (data.apparent.connection || 'tcp').toLowerCase();
          document.getElementById('aws-time').textContent = formatTime(data.apparent.age);
        }
        
        // Update True Wind
        if (data.true && data.true.hasData) {
          document.getElementById('tws-speed').textContent = convertSpeed(data.true.speed || 0);
          document.getElementById('tws-angle').textContent = (data.true.angle || 0).toFixed(0);
          
          if (data.true.source === 'Calculated') {
            document.getElementById('tws-source').textContent = 'Calculated';
            document.getElementById('tws-source').className = 'badge badge-calc';
            document.getElementById('tws-formula').textContent = 'AWS + SOG + COG';
          } else {
            document.getElementById('tws-source').textContent = data.true.source || 'MWV(T)';
            document.getElementById('tws-source').className = 'badge badge-nmea';
            document.getElementById('tws-formula').textContent = '';
          }
          document.getElementById('tws-time').textContent = formatTime(data.true.age);
        }
        
        // Update GPS Data
        if (data.gps) {
          if (data.gps.hasSOG) {
            document.getElementById('gps-sog').textContent = convertSpeed(data.gps.sog || 0);
          }
          if (data.gps.hasCOG) {
            document.getElementById('gps-cog').textContent = (data.gps.cog || 0).toFixed(0);
          }
          if (data.gps.hasHeading) {
            document.getElementById('gps-hdg').textContent = (data.gps.heading || 0).toFixed(0);
          }
          document.getElementById('gps-sentence').textContent = 'RMC, HDT';
          document.getElementById('gps-connection').textContent = data.gps.connection || 'UDP';
          document.getElementById('gps-connection').className = 'badge badge-' + (data.gps.connection || 'udp').toLowerCase();
          document.getElementById('gps-time').textContent = formatTime(data.gps.age);
        }
        
        // Update VMG
        if (data.vmg && data.vmg.hasData) {
          document.getElementById('vmg-value').textContent = convertSpeed(data.vmg.speed || 0);
          document.getElementById('vmg-time').textContent = formatTime(data.vmg.age);
        }
        
        // Update connection status
        const statusResp = await fetch('/status');
        const statusData = await statusResp.json();
        
        if (statusData.tcp_connected) {
          document.getElementById('tcp-status').textContent = 
            'TCP: ✓ ' + (statusData.tcp_host || '') + ':' + (statusData.tcp_port || '');
        } else {
          document.getElementById('tcp-status').textContent = 'TCP: ✗ Not connected';
        }
        
        if (statusData.udp_connected) {
          document.getElementById('udp-status').textContent = 
            'UDP: ✓ Port ' + (statusData.udp_port || '10110');
        } else {
          document.getElementById('udp-status').textContent = 'UDP: ✗ Not listening';
        }
        
        // Update last NMEA sentences (separate for TCP and UDP)
        if (statusData.last_tcp_nmea && statusData.last_tcp_nmea !== '-') {
          document.getElementById('last-tcp-nmea').textContent = statusData.last_tcp_nmea;
          document.getElementById('tcp-nmea-time').textContent = '(' + formatTime(statusData.last_tcp_age) + ')';
        }
        if (statusData.last_udp_nmea && statusData.last_udp_nmea !== '-') {
          document.getElementById('last-udp-nmea').textContent = statusData.last_udp_nmea;
          document.getElementById('udp-nmea-time').textContent = '(' + formatTime(statusData.last_udp_age) + ')';
        }
      } catch (error) {
        console.error('Status update failed:', error);
      }
    }, 2000);
    
    // Load config on page load
    loadConfig();
  </script>
</body>
</html>
)rawliteral";
  
  return html;
}
