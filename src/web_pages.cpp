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
      background: #f5f5f5;
      padding: 20px;
      line-height: 1.6;
    }
    .container { max-width: 1000px; margin: 0 auto; }
    .card {
      background: white;
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 20px;
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    }
    h1 { 
      color: #2c3e50;
      margin-bottom: 10px;
      font-size: 24px;
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
      border-radius: 4px;
      cursor: pointer;
      font-size: 14px;
      font-weight: 500;
      transition: background 0.3s;
    }
    button:hover { background: #2980b9; }
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
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <h1>🌬️ VDO Wind Adapter - LITE Multi</h1>
      <p class="info-text">3 speed pulse outputs • Shared direction • Simplified configuration</p>
    </div>

    <!-- Status Section -->
    <div class="card">
      <h2>📊 Current Status</h2>
      <div class="status-grid">
        <div class="status-item">
          <div class="status-label">Direction</div>
          <div class="status-value" id="directionValue">--<span class="status-unit">°</span></div>
          <div class="info-text" id="directionSource">-</div>
        </div>
        <div class="status-item">
          <div class="status-label">Apparent Wind</div>
          <div class="status-value" id="apparentSpeed">--<span class="status-unit">kn</span></div>
          <div class="info-text" id="apparentAngle">-- °</div>
        </div>
        <div class="status-item">
          <div class="status-label">True Wind</div>
          <div class="status-value" id="trueSpeed">--<span class="status-unit">kn</span></div>
          <div class="info-text" id="trueAngle">-- °</div>
        </div>
        <div class="status-item">
          <div class="status-label">GPS</div>
          <div class="status-value" id="sogValue">--<span class="status-unit">kn</span></div>
          <div class="info-text" id="cogValue">COG: -- °</div>
        </div>
        <div class="status-item">
          <div class="status-label">VMG</div>
          <div class="status-value" id="vmgValue">--<span class="status-unit">kn</span></div>
          <div class="info-text">Velocity Made Good</div>
        </div>
        <div class="status-item">
          <div class="status-label">Connections</div>
          <div class="status-value" style="font-size: 16px;">
            <span id="tcpStatus" class="status-error">TCP: ✗</span><br>
            <span id="udpStatus" class="status-error">UDP: ✗</span>
          </div>
          <div class="info-text">Dual Source Active</div>
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
        
        <div class="form-group">
          <label for="nmeaHost">NMEA Server Host</label>
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
  </div>

  <script>
    // Load initial configuration
    async function loadConfig() {
      try {
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
        
        // Update direction
        if (data.apparent && data.apparent.angle >= 0) {
          document.getElementById('directionValue').innerHTML = 
            data.apparent.angle + '<span class="status-unit">°</span>';
        }
        
        // Update apparent wind
        if (data.apparent) {
          document.getElementById('apparentSpeed').innerHTML = 
            (data.apparent.speed || 0).toFixed(1) + '<span class="status-unit">kn</span>';
          document.getElementById('apparentAngle').textContent = 
            (data.apparent.angle || 0) + '°';
        }
        
        // Update true wind
        if (data.true) {
          document.getElementById('trueSpeed').innerHTML = 
            (data.true.speed || 0).toFixed(1) + '<span class="status-unit">kn</span>';
          document.getElementById('trueAngle').textContent = 
            (data.true.angle || 0) + '°';
        }
        
        // Update GPS
        if (data.sog) {
          document.getElementById('sogValue').innerHTML = 
            (data.sog.speed || 0).toFixed(1) + '<span class="status-unit">kn</span>';
        }
        if (data.cog) {
          document.getElementById('cogValue').textContent = 
            'COG: ' + (data.cog.angle || 0) + '°';
        }
        
        // Update VMG
        if (data.vmg) {
          document.getElementById('vmgValue').innerHTML = 
            (data.vmg.speed || 0).toFixed(1) + '<span class="status-unit">kn</span>';
        }
        
        // Update connection status
        const statusResp = await fetch('/status');
        const statusData = await statusResp.json();
        
        const tcpEl = document.getElementById('tcpStatus');
        const udpEl = document.getElementById('udpStatus');
        
        if (statusData.tcp_connected) {
          tcpEl.innerHTML = 'TCP: ✓';
          tcpEl.className = 'status-ok';
        } else {
          tcpEl.innerHTML = 'TCP: ✗';
          tcpEl.className = 'status-error';
        }
        
        if (statusData.udp_connected) {
          udpEl.innerHTML = 'UDP: ✓';
          udpEl.className = 'status-ok';
        } else {
          udpEl.innerHTML = 'UDP: ✗';
          udpEl.className = 'status-error';
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
