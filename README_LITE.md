# VDO Wind Adapter LITE

**Simplified firmware for VDO analog wind instruments**

## What is LITE?

VDO Wind Adapter LITE is a streamlined version of the full firmware, designed for users who need:
- **Single display** support (instead of 3)
- **Apparent Wind only** (no True Wind, VMG, or GPS calculations)
- **Simple single-page Web UI**
- **Easier to understand** codebase (~75% smaller)

## Features

### ✅ Included
- ✅ Single VDO display (Sumlog or Logic Wind)
- ✅ Apparent Wind data (MWV-R, VWR)
- ✅ TCP or UDP NMEA input
- ✅ Single-page web configuration
- ✅ DAC output for Logic Wind direction
- ✅ PWM pulse output for speed
- ✅ WiFi configuration (STA mode)

### ❌ Removed (vs Full Version)
- ❌ Multiple displays (3 → 1)
- ❌ True Wind calculations
- ❌ VMG calculations
- ❌ GPS parsing (RMC, VTG, HDT, HDM)
- ❌ Multi-page Web UI
- ❌ Advanced data type selection

## Hardware Requirements

Same as full version:
- ESP32 development board
- DFRobot GP8403 DAC module (for Logic Wind direction)
- VDO analog wind instrument (Sumlog or Logic Wind)

## Quick Start

### 1. Flash Firmware
```bash
pio run --target upload
```

### 2. Connect to WiFi
- Device creates AP: `VDO-Cal` (password: `wind12345`)
- Connect and open: http://192.168.4.1
- Configure your WiFi network

### 3. Configure NMEA Source
- Enter NMEA server IP and port
- Select TCP or UDP protocol
- Save settings

### 4. Configure Display
- Select display type (Sumlog or Logic Wind)
- Set pulse output pin (default: GPIO 12)
- Adjust calibration (pulses per knot)
- Save settings

## Web Interface

Single-page interface at `http://[device-ip]/` shows:
- **Current Status**: Wind speed, angle, data source, connection
- **Display Configuration**: Type, pins, calibration
- **Network Configuration**: WiFi, NMEA server

## NMEA Sentences Supported

### Apparent Wind
- **$xxMWV** - Wind Speed and Angle (R=Relative only)
- **$xxVWR** - Relative Wind Speed and Angle

Where `xx` is the talker ID (e.g., `II`, `WI`, `GP`)

## Configuration

### Display Types

**Sumlog (Speed only)**
- Pulse output proportional to wind speed
- Frequency = Speed (kn) × K (pulses/knot)
- No direction output

**Logic Wind (Speed + Direction)**
- Pulse output for speed (same as Sumlog)
- DAC output for direction (sin/cos signals)
- Requires GP8403 DAC module

### Calibration

**Pulses per Knot (K)**
- Default: 1.0
- Adjust to match your instrument's calibration
- Higher value = more pulses per knot

**Direction Offset**
- Adjust if wind direction is incorrect
- Range: -180° to +180°
- Only applies to Logic Wind

**Maximum Frequency**
- Default: 150 Hz
- Limits pulse frequency at high wind speeds
- Prevents instrument overload

## Code Structure

```
src/
├── wind_project.ino          # Main program
├── web_ui.h/cpp              # Web server & API
├── web_pages.cpp             # Single-page HTML UI
├── nmea_parser.h/cpp         # NMEA sentence parsing (MWV, VWR)
├── display_controller.h/cpp  # Pulse & DAC output
└── config_manager.h          # NVS configuration
```

**Total: ~1020 lines** (vs 3555 in full version)

## Differences from Full Version

| Feature | Full | LITE |
|---------|------|------|
| Displays | 3 | 1 |
| Wind Types | Apparent, True, VMG | Apparent only |
| GPS Support | ✅ | ❌ |
| True Wind Calc | ✅ | ❌ |
| VMG Calc | ✅ | ❌ |
| Web UI | Multi-page | Single page |
| Code Size | 3555 lines | 1020 lines |
| Complexity | Advanced | Simple |

## When to Use LITE

**Choose LITE if you:**
- Have only one VDO display
- Only need Apparent Wind
- Don't have GPS data
- Want simpler code to understand/modify
- Prefer minimal configuration

**Choose Full if you:**
- Have multiple displays
- Need True Wind calculations
- Have GPS data (SOG, COG, Heading)
- Need VMG calculations
- Want advanced features

## Troubleshooting

### No Wind Data
1. Check NMEA connection (TCP/UDP)
2. Verify NMEA server is sending MWV or VWR sentences
3. Check Web UI status page for connection status

### Wrong Wind Direction
1. Adjust "Direction Offset" in display settings
2. Verify DAC connections (Logic Wind only)
3. Check that MWV/VWR sentences have correct format

### Pulse Output Not Working
1. Verify "Enable Display Output" is checked
2. Check pulse output pin (default: GPIO 12)
3. Verify wind speed > 0
4. Check "Pulses per Knot" calibration

## API Endpoints

### Status
```
GET /status
Returns: JSON with current wind data and configuration
```

### Display Configuration
```
GET /api/display
Returns: Display configuration

POST /api/display?action=save&enabled=1&type=sumlog&...
Saves display configuration
```

### Network Configuration
```
POST /savecfg
Body: sta_ssid=...&sta_pass=...&p1_host=...&p1_port=...
Saves network configuration and restarts
```

## Building from Source

### Requirements
- PlatformIO
- ESP32 platform support

### Build
```bash
cd VDO_Analog_Wind_NMEA_Adapter
git checkout lite-version
pio run
```

### Upload
```bash
pio run --target upload
```

## License

Same as full version - see LICENSE file

## Support

For issues or questions:
- Full version: See main README.md
- LITE version: Create issue with `[LITE]` prefix

## Migration

### From Full to LITE
1. Backup your configuration
2. Flash LITE firmware
3. Reconfigure (only single display settings needed)

### From LITE to Full
1. Flash full firmware
2. Configure additional displays if needed
3. Enable GPS/True Wind features as desired

## Version History

- **v1.0-lite** (2025-11-23): Initial LITE release
  - Single display support
  - Apparent Wind only
  - Simplified Web UI
  - ~75% code reduction
