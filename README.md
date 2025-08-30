VDO_Analog_Wind_NMEA_Adapter
This ESP32-based adapter revives legacy VDO wind instruments by allowing them to display wind data from modern NMEA 0183 sources. Originally designed for analog sensors, these gauges can now be integrated into digital marine systems — without replacing the original hardware.

I reverse engineered the VDO Logic Wind sensor behavior using information from various sources, including the German open-boat-projects.org forum.

Wind Direction Support
VDO analog wind gauges operate using SIN/COS voltage signals (typically 2–6 V) to move a mechanical needle. This adapter reads NMEA wind sentences (MWV, VWR, VWT), calculates the wind angle, and generates corresponding analog voltages using a GP8403 DAC module.

Devices used for wind direction:

ESP32 microcontroller – receives NMEA data and calculates wind angle

GP8403 DAC (Gravity: 2-Channel I2C DAC Module 0–10V) – outputs analog voltages

Analog VDO wind gauge – responds to SIN/COS signals to show wind direction

The ESP32 receives NMEA data over Wi-Fi (UDP or TCP) and outputs:

Koodi
SIN = 4000 + 2000 × sin(angle)
COS = 4000 + 2000 × cos(angle)
These voltages are sent to the gauge, which responds just like it would with its original masthead sensor.

Wi-Fi settings and calibration are accessible via a built-in web interface: SSID: VDO-Cal Password: wind12345

Wind Speed Support — Compatible with Wind Logic and Sumlog Gauges
Wind speed is now supported in two ways:

1. Wind Logic-style gauges
These gauges interpret wind speed internally from NMEA sentences. The adapter parses wind speed from MWV, VWR, and VWT and makes it available for display or further processing.

2. Sumlog-style analog speedometers
These expect a pulse signal (e.g. 1 Hz = 1 knot). The ESP32 generates a calibrated pulse output using its LEDC hardware peripheral. The signal is isolated using a PC817 optocoupler, allowing safe interfacing with 12 V systems.

Devices used for wind speed:

ESP32 microcontroller – generates pulse signal

PC817 optocoupler module – provides galvanic isolation and level shifting

Analog Sumlog-style speedometer – interprets pulse frequency as speed

12 V boat power supply – powers the gauge and pull-up circuit

Pulse simulation setup:

ESP32 drives the optocoupler’s LED side

The transistor side pulls the signal line to ground

A pull-up resistor to +12 V completes the circuit (often built into the gauge)

Pulse frequency is proportional to wind speed and can be calibrated via the web interface.

Supported NMEA Sentences
$WIMWV — Wind speed and angle (relative and true)

$IIVWR — Apparent wind angle and speed

$IIVWT — True wind angle and speed

Compatibility Notes
This adapter works with SIN/COS-based analog meters only.

Wind direction and wind speed are both supported.

Use at your own risk.