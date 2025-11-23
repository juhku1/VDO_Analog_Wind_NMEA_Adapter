# VDO Wind Adapter - LITE Multi

**3 speed pulse outputs with shared direction**

## What is LITE Multi?

LITE Multi is the **honest** multi-output version that matches ESP32 hardware reality:
- **1 DAC** → **1 direction output** (shared by all Logic Wind instruments)
- **3 GPIO** → **3 speed pulse outputs** (independent)

## Key Concept

Unlike the "full version" which pretends each display can have independent direction, LITE Multi is **honest about hardware limitations**:

```
┌─────────────────────────────────────┐
│         ESP32 Hardware              │
├─────────────────────────────────────┤
│ 1 DAC (GP8403)                      │
│   └─> Direction Output (0-360°)    │ ← Shared by ALL Logic Wind
│                                     │
│ 3 GPIO Pins                         │
│   ├─> Speed Pulse 1 (0-150 Hz)     │ ← Independent
│   ├─> Speed Pulse 2 (0-150 Hz)     │ ← Independent
│   └─> Speed Pulse 3 (0-150 Hz)     │ ← Independent
└─────────────────────────────────────┘
```

## Features

### ✅ What You CAN Do

**Example 1: Three Sumlog instruments**
```
Speed Pulse 1: Apparent Wind → GPIO 12
Speed Pulse 2: True Wind     → GPIO 14
Speed Pulse 3: SOG           → GPIO 16

Direction: Not used (Sumlog doesn't need direction)
```

**Example 2: One Logic Wind + Two Sumlog**
```
Speed Pulse 1 (Logic Wind): Apparent Wind speed → GPIO 12
Speed Pulse 2 (Sumlog):     True Wind speed     → GPIO 14
Speed Pulse 3 (Sumlog):     SOG                 → GPIO 16

Direction: Apparent Wind angle → DAC (for Logic Wind only)
```

**Example 3: Three Logic Wind instruments**
```
Speed Pulse 1 (Logic Wind): Apparent Wind speed → GPIO 12
Speed Pulse 2 (Logic Wind): True Wind speed     → GPIO 14
Speed Pulse 3 (Logic Wind): SOG                 → GPIO 16

Direction: Apparent Wind angle → DAC (SHARED by all three)
```
All three Logic Wind instruments show **same direction**, but **different speeds**.

### ❌ What You CANNOT Do

```
❌ Logic Wind 1: Apparent Wind (45°, 5 kn)
❌ Logic Wind 2: True Wind (38°, 6 kn)  ← Different angle!
```

**Why?** Only 1 DAC = only 1 direction output.

## Configuration

### Direction Output (Global)

Select which data source provides direction for **all** Logic Wind instruments:
- Apparent Wind Angle (from MWV-R, VWR)
- True Wind Angle (from MWV-T, VWT)
- Course Over Ground (from RMC, VTG)

### Speed Pulse Outputs (Per-Output)

Each of the 3 outputs can be configured independently:

**Instrument Type:**
- **Sumlog**: Speed only (pulse output)
- **Logic Wind**: Speed (pulse) + Direction (DAC)

**Speed Source:**
- Apparent Wind Speed (MWV-R, VWR)
- True Wind Speed (MWV-T, VWT)
- Speed Over Ground (RMC, VTG)

**Hardware Settings:**
- GPIO Pin (0-39)
- Pulses per Knot (calibration)
- Max Frequency (Hz limit)
- Duty Cycle (pulse width %)

## Web Interface

Single-page interface at `http://[device-ip]/`:

### 📊 Status Section
- Current direction (global)
- Apparent Wind (speed + angle)
- True Wind (speed + angle)
- GPS (SOG + COG)

### 🧭 Direction Output
- Select direction source (global)
- Applies to all Logic Wind instruments

### ⚡ Speed Pulse 1, 2, 3
- Enable/disable each output
- Select instrument type
- Select speed source
- Configure GPIO pin
- Set calibration values

### 🌐 Network Configuration
- WiFi settings
- NMEA server (TCP/UDP)

## NMEA Sentences Supported

### Wind Data
- **$xxMWV** - Wind Speed and Angle (R=Apparent, T=True)
- **$xxVWR** - Relative Wind Speed and Angle
- **$xxVWT** - True Wind Speed and Angle

### GPS Data
- **$xxRMC** - Recommended Minimum (SOG, COG)
- **$xxVTG** - Track Made Good and Ground Speed
- **$xxHDT** - True Heading
- **$xxHDM** - Magnetic Heading

## Dual Source Support

**LITE Multi supports simultaneous TCP and UDP connections!**

### How It Works
```
TCP Connection:  Connects to specified host:port (e.g., OpenPlotter)
UDP Connection:  Listens on port 10110 for broadcasts (always active)

Both connections are polled simultaneously in the same loop.
Data from both sources is merged in real-time.
```

### Use Cases

**Case 1: Primary + Backup**
```
TCP: Primary NMEA source (192.168.1.100:10110)
UDP: Backup broadcast source

If TCP fails → UDP continues providing data
```

**Case 2: Multiple Instruments**
```
TCP: Wind instrument (Apparent Wind from masthead)
UDP: OpenPlotter (True Wind, GPS, calculated data)

Both active → Complete dataset
```

**Case 3: Redundancy**
```
TCP: Main chartplotter
UDP: Backup navigation system

If one fails → Other continues
```

### Configuration

**Web UI:**
- Enter TCP host and port for primary source
- UDP automatically listens on port 10110
- Status display shows both connection states:
  - TCP: ✓ (connected) or ✗ (disconnected)
  - UDP: ✓ (bound) or ✗ (not bound)

**No protocol selection needed** - both are always active!

## Hardware Setup

### Connections

**DAC (GP8403):**
- SDA → GPIO 21
- SCL → GPIO 22
- CH0 (SIN) → Logic Wind SIN input
- CH1 (COS) → Logic Wind COS input

**Speed Pulse Outputs:**
- GPIO 12 → Instrument 1 pulse input
- GPIO 14 → Instrument 2 pulse input
- GPIO 16 → Instrument 3 pulse input

(GPIO pins are configurable via Web UI)

## Use Cases

### Case 1: Racing Sailor
```
Speed Pulse 1 (Logic Wind): Apparent Wind
  - Speed: Apparent Wind Speed
  - Direction: Apparent Wind Angle
  - Pin: GPIO 12

Speed Pulse 2 (Sumlog): True Wind Speed
  - Speed: True Wind Speed (calculated by OpenPlotter)
  - Pin: GPIO 14

Speed Pulse 3 (Sumlog): SOG
  - Speed: GPS SOG
  - Pin: GPIO 16
```

### Case 2: Cruiser
```
Speed Pulse 1 (Logic Wind): Apparent Wind
  - Speed: Apparent Wind Speed
  - Direction: Apparent Wind Angle
  - Pin: GPIO 12

Speed Pulse 2: Disabled
Speed Pulse 3: Disabled
```

### Case 3: Multiple Stations
```
Speed Pulse 1 (Logic Wind): Helm station
  - Speed: Apparent Wind Speed
  - Direction: Apparent Wind Angle
  - Pin: GPIO 12

Speed Pulse 2 (Logic Wind): Nav station
  - Speed: Apparent Wind Speed
  - Direction: Apparent Wind Angle (SAME as Pulse 1)
  - Pin: GPIO 14

Speed Pulse 3 (Logic Wind): Cockpit
  - Speed: Apparent Wind Speed
  - Direction: Apparent Wind Angle (SAME as Pulse 1)
  - Pin: GPIO 16
```
All three show **same direction**, **same speed**.

## API Endpoints

### Direction
```
GET /api/direction
Returns: {"source": 0, "angle": 45}

POST /api/direction
Body: source=0
Saves direction source (0=Apparent, 1=True, 3=COG)
```

### Speed Pulses
```
GET /api/display?num=1
Returns: {
  "enabled": true,
  "type": "logicwind",
  "speedSource": 0,
  "pulsePin": 12,
  "pulsesPerKnot": 1.0,
  "maxFrequency": 150,
  "dutyCycle": 10
}

POST /api/display?num=1&action=save&enabled=1&type=logicwind&...
Saves speed pulse configuration
```

### Data Flow
```
GET /api/dataflow
Returns: {
  "apparent": {"speed": 5.2, "angle": 45, "source": "MWV(R)", "age": 123},
  "true": {"speed": 6.1, "angle": 38, "source": "MWV(T)", "age": 456},
  "sog": {"speed": 4.8, "angle": 0, "source": "GPS", "age": 789},
  "cog": {"speed": 0, "angle": 180, "source": "GPS", "age": 789},
  "directionSource": 0,
  "speedPulses": [
    {"enabled": true, "speedSource": 0},
    {"enabled": false, "speedSource": 0},
    {"enabled": false, "speedSource": 0}
  ]
}
```

## Comparison

| Feature | LITE | LITE Plus | LITE Multi | Full |
|---------|------|-----------|------------|------|
| Speed outputs | 1 | 1 | 3 | 3 |
| Direction outputs | 1 | 1 | 1 (global) | 3 (fake) |
| Speed sources | 1 | 4 | 4 per output | 4 per display |
| Honest about HW | ✅ | ✅ | ✅ | ❌ |
| Code complexity | Low | Low | Medium | High |
| Lines of code | ~1000 | ~1400 | ~1600 | ~3500 |

## Advantages over Full Version

1. **Honest**: Doesn't pretend you can have 3 independent directions
2. **Simpler**: No complex per-display data routing for angles
3. **Clearer**: Terminology matches hardware (speed pulses, not displays)
4. **Smaller**: ~1600 lines vs ~3500 lines
5. **Easier to understand**: One direction source, multiple speed sources

## When to Use

**Choose LITE Multi if:**
- You have 2-3 instruments to drive
- You understand that direction is shared
- You want flexibility in speed sources
- You prefer honest, simple code

**Choose LITE Plus if:**
- You have only 1 instrument
- You want the simplest possible solution

**Choose Full if:**
- You need the illusion of independent displays
- You have legacy code dependencies
- You don't mind extra complexity

## Building

```bash
cd VDO_Analog_Wind_NMEA_Adapter
git checkout lite-multi
pio run
pio run --target upload
```

## Troubleshooting

### Direction not updating
1. Check that at least one speed pulse has `instrumentType = "logicwind"`
2. Check that direction source has valid data
3. Check DAC connections (SDA, SCL, power)

### Speed pulse not working
1. Check that speed pulse is enabled
2. Check that speed source has valid data
3. Check GPIO pin number
4. Verify pulses per knot calibration

### All Logic Wind instruments show wrong direction
- Adjust direction source (Apparent/True/COG)
- Check NMEA data source (OpenPlotter, etc.)
- Verify angle data is being received

## Version History

- **v1.0-multi** (2025-11-23): Initial LITE Multi release
  - 3 independent speed pulse outputs
  - 1 shared direction output
  - Honest hardware representation
  - ~1600 lines of code

## License

Same as main project - see LICENSE file

## Credits

LITE Multi design based on understanding that:
- Hardware has 1 DAC → 1 direction
- Hardware has multiple GPIO → multiple speeds
- Honest design is better than fake flexibility
