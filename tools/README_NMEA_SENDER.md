# NMEA Wind & GPS Sender v2

Simplified GUI tool for testing VDO Wind Adapter with NMEA data.

## Features

- ✅ **Separate TCP and UDP** - Choose which protocol to use
- ✅ **Select NMEA sentences** - Pick what to send via each protocol
- ✅ **Manual controls** - Direct sliders for all values
- ✅ **No simulation modes** - Simple and predictable
- ✅ **Real-time sending** - See what's being sent

## Installation

```bash
# Python 3 with tkinter (usually pre-installed)
python3 --version

# If tkinter is missing (Ubuntu/Debian):
sudo apt-get install python3-tk
```

## Usage

```bash
cd tools
python3 nmea_sender_v2.py
```

## Configuration

### 1. Connection

- **ESP32 IP**: Your ESP32's IP address (e.g., 192.168.1.50)
- **TCP Port**: Usually 10110
- **UDP Port**: Usually 10110
- **Enable TCP**: Check to send via TCP
- **Enable UDP**: Check to send via UDP

### 2. NMEA Sentences

**Send via TCP** (typical: wind instrument):
- ☑ **MWV(R)** - Apparent Wind (angle, speed)
- ☐ **MWV(T)** - True Wind (if you want to test)
- ☐ **VWR** - Relative Wind (alternative format)

**Send via UDP** (typical: GPS/navigator):
- ☑ **RMC** - GPS position, SOG, COG
- ☑ **HDT** - True Heading
- ☐ **HDM** - Magnetic Heading
- ☐ **VTG** - Track & Speed (alternative)

### 3. Data Controls

**Apparent Wind (MWV-R)**:
- Angle: 0-359° (wind direction)
- Speed: 0-70 kn (wind speed)

**True Wind (MWV-T)**:
- Angle: 0-359°
- Speed: 0-70 kn

**GPS Data**:
- SOG: 0-30 kn (Speed Over Ground)
- COG: 0-359° (Course Over Ground)
- HDG: 0-359° (Heading)

## Test Scenarios

### Basic Test (Apparent Wind + GPS)
```
TCP: ☑ MWV(R)
UDP: ☑ RMC, ☑ HDT

Set:
- AWS: 45°, 12.5 kn
- SOG: 6.8 kn
- COG: 285°
- HDG: 290°
```

### True Wind Calculation Test
```
TCP: ☑ MWV(R)
UDP: ☑ RMC, ☑ HDT

ESP32 should calculate True Wind from:
- Apparent Wind (MWV-R)
- SOG (from RMC)
- COG (from RMC)
```

### Dual Source Test
```
Instance 1:
TCP: ☑ MWV(R)
UDP: (disabled)

Instance 2:
TCP: (disabled)
UDP: ☑ RMC, ☑ HDT

Run both simultaneously to test dual source!
```

### VWR Parsing Test
```
TCP: ☑ VWR

Test the VWR parser fix:
- VWR sends speed in 3 units (kn, m/s, km/h)
- ESP32 should parse correctly
```

### Unit Conversion Test
```
TCP: ☑ MWV(R)

1. Set wind speed to 10 kn
2. Check ESP32 Web UI shows 10 kn
3. Change unit to m/s in Web UI
4. Check ESP32 shows 5.1 m/s
```

## Troubleshooting

**"Connection Error"**:
- Check ESP32 IP address
- Verify ESP32 is on same network
- Check firewall settings

**"No data on ESP32"**:
- Check connection status (TCP ✓ / UDP ✓)
- Verify correct NMEA sentences are checked
- Check "Last Sent" shows sentences
- Look at ESP32 Serial Monitor for errors

**"TCP connects but no data"**:
- Ensure MWV(R) or other sentence is checked
- Click "Start Sending"
- Check ESP32 Status tab for "Last TCP NMEA"

**"UDP not working"**:
- UDP is connectionless - no error if ESP32 not listening
- Check ESP32 Status tab for "Last UDP NMEA"
- Verify UDP port matches ESP32 configuration

## Tips

- **Start simple**: Use default settings first
- **One protocol at a time**: Test TCP, then UDP, then both
- **Watch ESP32 Status tab**: See real-time data updates
- **Serial Monitor**: See debug logs on ESP32
- **Sending interval**: 1 second (hardcoded, modify code if needed)

## Differences from v1

- ❌ Removed simulation modes (random, sine wave, etc.)
- ✅ Added per-protocol NMEA sentence selection
- ✅ Simplified UI - only manual controls
- ✅ Separate TCP/UDP enable checkboxes
- ✅ Shows last sent sentence for each protocol
- ✅ Cleaner, more focused interface

## License

MIT License - Same as VDO Wind Adapter project
