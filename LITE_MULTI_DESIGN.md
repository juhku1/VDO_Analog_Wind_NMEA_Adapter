# LITE Multi - Simplified Multi-Display Design

## Problem with Full Version

The full version pretends each display can have independent angle/speed, but:
- **Only 1 DAC** → Can only show **1 direction**
- Storing 3 separate `windAngle_deg` values is misleading
- Complex per-display data routing that doesn't match hardware reality

## Hardware Reality

**ESP32 has:**
- 1 DAC (GP8403) → **1 global direction** (shared by all Logic Wind displays)
- Multiple GPIO pins → **Multiple independent speeds** (one per display)

## Simplified Model

### Global Direction
```cpp
// ONE direction for ALL Logic Wind displays
uint8_t global_direction_source = DATA_APPARENT_WIND;  // User selects
int global_wind_angle = 0;  // Updated from selected source
```

### Per-Display Configuration
```cpp
struct Display {
  bool enabled;
  char type[16];        // "sumlog" or "logicwind"
  uint8_t speedSource;  // Which data source for speed
  int pulsePin;         // GPIO pin
  float sumlogK;        // Calibration
  int sumlogFmax;       // Max frequency
  int pulseDuty;        // Pulse duty %
  
  // Runtime (updated automatically)
  float currentSpeed_kn;
  uint32_t lastUpdate_ms;
};

Display displays[3];
```

## Data Flow

```
NMEA Input → Parse → Update data sources
                      ↓
              ┌───────┴────────┐
              ↓                ↓
    Global Direction    Per-Display Speeds
    (from one source)   (each from own source)
              ↓                ↓
         DAC Output      GPIO Pulses (×3)
              ↓                ↓
      Logic Wind Dir    Display 1, 2, 3
```

## Example Configurations

### Config 1: Three Sumlog displays
```
Display 1: Apparent Wind speed → GPIO 12
Display 2: True Wind speed     → GPIO 14
Display 3: SOG                 → GPIO 27

Global Direction: N/A (no Logic Wind displays)
```

### Config 2: One Logic Wind + Two Sumlog
```
Display 1 (Logic Wind): Apparent Wind speed → GPIO 12
Display 2 (Sumlog):     True Wind speed     → GPIO 14
Display 3 (Sumlog):     SOG                 → GPIO 27

Global Direction: Apparent Wind angle → DAC
```

### Config 3: Three Logic Wind displays
```
Display 1 (Logic Wind): Apparent Wind speed → GPIO 12
Display 2 (Logic Wind): True Wind speed     → GPIO 14
Display 3 (Logic Wind): SOG                 → GPIO 27

Global Direction: Apparent Wind angle → DAC
All three displays show SAME direction, different speeds
```

## Web UI

### Direction Section (Global)
```html
<h3>🧭 Direction (for Logic Wind displays)</h3>
<select id="globalDirection">
  <option value="0">Apparent Wind Angle</option>
  <option value="1">True Wind Angle</option>
  <option value="3">COG</option>
</select>
<p>All Logic Wind displays will show this direction</p>
```

### Display Sections (Per-Display)
```html
<h3>📊 Display 1</h3>
<input type="checkbox" id="d1_enabled"> Enabled
<select id="d1_type">
  <option value="sumlog">Sumlog (speed only)</option>
  <option value="logicwind">Logic Wind (speed + direction)</option>
</select>
<select id="d1_speedSource">
  <option value="0">Apparent Wind Speed</option>
  <option value="1">True Wind Speed</option>
  <option value="2">SOG</option>
</select>
<input type="number" id="d1_pin"> GPIO Pin
<input type="number" id="d1_sumlogK"> Pulses per knot

<!-- Repeat for Display 2 and 3 -->
```

## Code Structure

### Main Loop
```cpp
void loop() {
  // 1. Update global direction (once)
  updateGlobalDirection();
  
  // 2. Update each display's speed (3 times)
  for (int i = 0; i < 3; i++) {
    updateDisplaySpeed(i);
    updateDisplayPulse(i);
  }
}
```

### Update Functions
```cpp
void updateGlobalDirection() {
  // Select angle based on global_direction_source
  switch(global_direction_source) {
    case DATA_APPARENT_WIND:
      global_wind_angle = (int)apparent_angle_deg;
      break;
    case DATA_TRUE_WIND:
      global_wind_angle = (int)true_angle_deg;
      break;
    case DATA_COG:
      global_wind_angle = (int)gps_cog_deg;
      break;
  }
  
  // Update DAC (for all Logic Wind displays)
  setOutputsDeg(global_wind_angle);
}

void updateDisplaySpeed(int i) {
  // Select speed based on display's speedSource
  switch(displays[i].speedSource) {
    case DATA_APPARENT_WIND:
      displays[i].currentSpeed_kn = apparent_speed_kn;
      break;
    case DATA_TRUE_WIND:
      displays[i].currentSpeed_kn = true_speed_kn;
      break;
    case DATA_SOG:
      displays[i].currentSpeed_kn = gps_sog_kn;
      break;
  }
}
```

## Benefits

✅ **Simpler than full version**
- No per-display angle storage (only one global angle)
- No complex per-display data routing for angles
- Honest about hardware limitations

✅ **More flexible than LITE Plus**
- Supports 3 displays instead of 1
- Each display can show different speed source
- Still simple to understand

✅ **Matches hardware reality**
- 1 DAC → 1 direction (global)
- 3 GPIO → 3 speeds (independent)

## Limitations

❌ **Cannot do:**
- Display 1: Apparent Wind direction (45°)
- Display 2: True Wind direction (38°) ← Different angle!

✅ **Can do:**
- Display 1: Apparent Wind speed (5 kn)
- Display 2: True Wind speed (6 kn)
- Display 3: SOG (4 kn)
- All Logic Wind displays: Apparent Wind direction (45°)

## Comparison

| Feature | LITE | LITE Plus | LITE Multi | Full |
|---------|------|-----------|------------|------|
| Displays | 1 | 1 | 3 | 3 |
| Direction sources | 1 | 1 | 1 (global) | 3 (fake) |
| Speed sources | 1 | 1 | 3 (per-display) | 3 |
| Code complexity | Low | Low | Medium | High |
| Honest about HW | ✅ | ✅ | ✅ | ❌ |

## Implementation Plan

1. Add `global_direction_source` variable
2. Remove per-display `windAngle_deg` (use global)
3. Rename `dataType` → `speedSource` (clearer)
4. Update display controller
5. Simplify Web UI
6. Update documentation
