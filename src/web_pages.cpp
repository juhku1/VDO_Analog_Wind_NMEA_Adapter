// web_pages_lite.cpp - LITE: Single-page HTML UI

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

// LITE: Single-page UI combining status, network, and display config
String buildSinglePage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>VDO Wind Adapter LITE</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { 
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #f5f5f5;
      padding: 20px;
      line-height: 1.6;
    }
    .container { max-width: 800px; margin: 0 auto; }
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
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <h1>🌬️ VDO Wind Adapter LITE</h1>
      <p class="info-text">Single display • Apparent Wind only • Simplified configuration</p>
    </div>

    <!-- Status Section -->
    <div class="card">
      <h2>📊 Current Status</h2>
      <div class="status-grid">
        <div class="status-item">
          <div class="status-label">Wind Speed</div>
          <div class="status-value">)rawliteral";
  
  // Wind speed
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  float speed = apparent_speed_kn;
  int angle = (int)apparent_angle_deg;
  bool hasData = apparent_hasData;
  uint32_t lastUpdate = apparent_lastUpdate_ms;
  String source = String(apparent_source);
  xSemaphoreGive(dataMutex);
  
  // Debug logging
  Serial.printf("Web UI: speed=%.1f, angle=%d, hasData=%d, age=%u ms, source=%s\n", 
                speed, angle, hasData, millis() - lastUpdate, source.c_str());
  
  if (hasData && (millis() - lastUpdate) < DATA_TIMEOUT_MS) {
    html += String(speed, 1);
    html += "<span class=\"status-unit\">kn</span>";
  } else {
    html += "<span class=\"status-error\">--</span>";
  }
  
  html += R"rawliteral(
          </div>
        </div>
        <div class="status-item">
          <div class="status-label">Wind Angle</div>
          <div class="status-value">)rawliteral";
  
  if (hasData && (millis() - lastUpdate) < DATA_TIMEOUT_MS) {
    html += String(angle);
    html += "<span class=\"status-unit\">°</span>";
  } else {
    html += "<span class=\"status-error\">--</span>";
  }
  
  html += R"rawliteral(
          </div>
        </div>
        <div class="status-item">
          <div class="status-label">Data Source</div>
          <div class="status-value" style="font-size: 16px;">)rawliteral";
  
  html += source;
  
  html += R"rawliteral(
          </div>
          <div class="info-text">)rawliteral";
  
  html += formatTimeAgo(lastUpdate);
  
  html += R"rawliteral(</div>
        </div>
        <div class="status-item">
          <div class="status-label">Connection</div>
          <div class="status-value" style="font-size: 16px;">)rawliteral";
  
  if (tcpConnected) {
    html += "<span class=\"status-ok\">✓ TCP</span>";
  } else if (udpConnected) {
    html += "<span class=\"status-ok\">✓ UDP</span>";
  } else {
    html += "<span class=\"status-error\">✗ None</span>";
  }
  
  html += R"rawliteral(
          </div>
        </div>
      </div>
    </div>

    <!-- Display Configuration -->
    <div class="card">
      <h2>⚙️ Display Configuration</h2>
      <form id="displayForm">
        <div class="form-group">
          <label>
            <input type="checkbox" id="enabled" )rawliteral";
  
  if (speedPulses[0].enabled) html += "checked";
  
  html += R"rawliteral(>
            Enable Display Output
          </label>
        </div>
        
        <div class="form-group">
          <label for="type">Display Type</label>
          <select id="type">
            <option value="sumlog" )rawliteral";
  
  if (strcmp(speedPulses[0].instrumentType, "sumlog") == 0) html += "selected";
  
  html += R"rawliteral(>Sumlog (Speed only)</option>
            <option value="logicwind" )rawliteral";
  
  if (strcmp(speedPulses[0].instrumentType, "logicwind") == 0) html += "selected";
  
  html += R"rawliteral(>Logic Wind (Speed + Direction)</option>
          </select>
          <p class="info-text">Sumlog: pulse output only. Logic Wind: pulse + DAC direction.</p>
        </div>
        
        <div class="form-group">
          <label for="dataType">Data Source</label>
          <select id="speedSource">
            <option value="0" )rawliteral";
  
  if (speedPulses[0].speedSource == DATA_APPARENT_WIND) html += "selected";
  
  html += R"rawliteral(>Apparent Wind (MWV-R, VWR)</option>
            <option value="1" )rawliteral";
  
  if (speedPulses[0].speedSource == DATA_TRUE_WIND) html += "selected";
  
  html += R"rawliteral(>True Wind (MWV-T, VWT)</option>
            <option value="2" )rawliteral";
  
  if (speedPulses[0].speedSource == DATA_SOG) html += "selected";
  
  html += R"rawliteral(>Speed Over Ground (RMC, VTG)</option>
            <option value="3" )rawliteral";
  
  if (speedPulses[0].speedSource == DATA_COG) html += "selected";
  
  html += R"rawliteral(>Course Over Ground (RMC, VTG)</option>
          </select>
          <p class="info-text">Select which NMEA data to display on the instrument.</p>
        </div>
        
        <div class="form-group">
          <label for="pulsePin">Pulse Output Pin (GPIO)</label>
          <input type="number" id="pulsePin" value=")rawliteral";
  
  html += String(speedPulses[0].pulsePin);
  
  html += R"rawliteral(" min="0" max="39">
          <p class="info-text">GPIO pin for pulse output (default: 12)</p>
        </div>
        
        <div class="form-group">
          <label for="pulsesPerKnot">Pulses per Knot</label>
          <input type="number" id="pulsesPerKnot" value=")rawliteral";
  
  html += String(speedPulses[0].pulsesPerKnot, 1);
  
  html += R"rawliteral(" step="0.1" min="0.1" max="100">
          <p class="info-text">Calibration factor (default: 1.0)</p>
        </div>
        
        <div class="form-group">
          <label for="maxFrequency">Maximum Frequency (Hz)</label>
          <input type="number" id="maxFrequency" value=")rawliteral";
  
  html += String(speedPulses[0].maxFrequency);
  
  html += R"rawliteral(" min="10" max="1000">
          <p class="info-text">Frequency limit (default: 150 Hz)</p>
        </div>
        
        <div class="form-group">
          <label for="dutyCycle">Pulse Duty Cycle (%)</label>
          <input type="number" id="dutyCycle" value=")rawliteral";
  
  html += String(speedPulses[0].dutyCycle);
  
  html += R"rawliteral(" min="1" max="99">
          <p class="info-text">Pulse width percentage (default: 10%)</p>
        </div>
        
        <button type="submit">💾 Save Speed Pulse Settings</button>
      </form>
    </div>

    <!-- Network Configuration -->
    <div class="card">
      <h2>🌐 Network Configuration</h2>
      <form id="networkForm">
        <div class="form-group">
          <label for="ssid">WiFi SSID</label>
          <input type="text" id="ssid" value=")rawliteral";
  
  html += String(sta_ssid);
  
  html += R"rawliteral(" placeholder="Enter WiFi network name">
        </div>
        
        <div class="form-group">
          <label for="password">WiFi Password</label>
          <input type="password" id="password" placeholder="Enter WiFi password">
          <p class="info-text">Leave blank to keep current password</p>
        </div>
        
        <div class="form-group">
          <label for="nmeaHost">NMEA Server Host</label>
          <input type="text" id="nmeaHost" value=")rawliteral";
  
  html += String(nmeaHost);
  
  html += R"rawliteral(" placeholder="192.168.1.100">
        </div>
        
        <div class="form-group">
          <label for="nmeaPort">NMEA Server Port</label>
          <input type="number" id="nmeaPort" value=")rawliteral";
  
  html += String(nmeaPort);
  
  html += R"rawliteral(" min="1" max="65535">
        </div>
        
        <div class="form-group">
          <label for="nmeaProto">Protocol</label>
          <select id="nmeaProto">
            <option value="1" )rawliteral";
  
  if (nmeaProto == PROTO_TCP) html += "selected";
  
  html += R"rawliteral(>TCP</option>
            <option value="0" )rawliteral";
  
  if (nmeaProto == PROTO_UDP) html += "selected";
  
  html += R"rawliteral(>UDP</option>
          </select>
        </div>
        
        <div class="button-group">
          <button type="submit">💾 Save Network Settings</button>
          <button type="button" class="btn-secondary" onclick="location.reload()">🔄 Refresh</button>
        </div>
      </form>
    </div>
  </div>

  <script>
    // Display form submission
    document.getElementById('displayForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const formData = new URLSearchParams({
        action: 'save',
        num: '1',
        enabled: document.getElementById('enabled').checked ? '1' : '0',
        type: document.getElementById('type').value,
        speedSource: document.getElementById('speedSource').value,
        pulsePin: document.getElementById('pulsePin').value,
        pulsesPerKnot: document.getElementById('pulsesPerKnot').value,
        maxFrequency: document.getElementById('maxFrequency').value,
        dutyCycle: document.getElementById('dutyCycle').value
      });
      
      try {
        const response = await fetch('/api/display?' + formData, { method: 'POST' });
        if (response.ok) {
          alert('✅ Display settings saved!');
          setTimeout(() => location.reload(), 1000);
        } else {
          alert('❌ Failed to save settings');
        }
      } catch (error) {
        alert('❌ Error: ' + error.message);
      }
    });
    
    // Network form submission
    document.getElementById('networkForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const formData = new URLSearchParams({
        sta_ssid: document.getElementById('ssid').value,
        sta_pass: document.getElementById('password').value,
        p1_host: document.getElementById('nmeaHost').value,
        p1_port: document.getElementById('nmeaPort').value,
        p1_proto: document.getElementById('nmeaProto').value
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
    setInterval(() => {
      fetch('/api/dataflow')
        .then(r => r.json())
        .then(data => {
          if (!data.display) return;
          
          // Select data source based on speedPulses[0].speedSource
          let sourceData;
          let sourceName = '';
          switch(data.speedPulses[0].speedSource) {
            case 0: // Apparent Wind
              sourceData = data.apparent;
              sourceName = 'Apparent Wind';
              break;
            case 1: // True Wind
              sourceData = data.true;
              sourceName = 'True Wind';
              break;
            case 2: // SOG
              sourceData = data.sog;
              sourceName = 'SOG';
              break;
            case 3: // COG
              sourceData = data.cog;
              sourceName = 'COG';
              break;
            default:
              sourceData = data.apparent;
              sourceName = 'Apparent Wind';
          }
          
          if (sourceData) {
            // Update wind speed
            const speedEl = document.querySelector('.status-grid .status-item:nth-child(1) .status-value');
            if (sourceData.speed > 0 && sourceData.age < 4000) {
              speedEl.innerHTML = sourceData.speed.toFixed(1) + '<span class="status-unit">kn</span>';
            } else {
              speedEl.innerHTML = '<span class="status-error">--</span>';
            }
            
            // Update wind angle
            const angleEl = document.querySelector('.status-grid .status-item:nth-child(2) .status-value');
            if (sourceData.angle >= 0 && sourceData.age < 4000) {
              angleEl.innerHTML = sourceData.angle + '<span class="status-unit">°</span>';
            } else {
              angleEl.innerHTML = '<span class="status-error">--</span>';
            }
            
            // Update data source
            const sourceEl = document.querySelector('.status-grid .status-item:nth-child(3) .status-value');
            sourceEl.textContent = sourceName + ' (' + (sourceData.source || '-') + ')';
            
            // Update age
            const ageEl = document.querySelector('.status-grid .status-item:nth-child(3) .info-text');
            if (sourceData.age < 1000) {
              ageEl.textContent = 'Just now';
            } else if (sourceData.age < 60000) {
              ageEl.textContent = Math.floor(sourceData.age / 1000) + 's ago';
            } else {
              ageEl.textContent = Math.floor(sourceData.age / 60000) + 'm ago';
            }
          }
        })
        .catch(() => {});
    }, 2000);
  </script>
</body>
</html>
)rawliteral";
  
  return html;
}
