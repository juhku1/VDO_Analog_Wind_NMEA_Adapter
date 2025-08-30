# VDO_Analog_Wind_NMEA_Adapter

This ESP32 adapter enables legacy VDO wind gauges, which do not natively support NMEA 0183, to display wind direction from modern NMEA wind data – making it easy to integrate these legacy analog instruments with new electronics.
This ESP32 adapter enables older VDO wind gauges, which do not natively support NMEA 0183, to display wind direction from modern NMEA wind data. It receives various NMEA sentences, calculates the corresponding sine and cosine signals, and drives the analog gauge as if it were connected to its original sensor. I sort of reverse engineered it.

The web interface allows configuration, calibration, and manual control, making it easy to integrate legacy instruments with new electronics. Käytetään wifiä, asetuksia pääsee säätämään access pointista, salasana wind12345.
# Reviving Legacy Analog Wind Meters
This project brings old VDO analog wind meters back to life by bridging them with modern NMEA 0183 wind data. These meters use SIN/COS voltage signals (2–6 V) to move a physical needle, but were never designed to interface with digital systems.

Using an ESP32 microcontroller and a GP8403 DAC (Gravity: 2-Channel I2C DAC Module 0-10V), this adapter reads NMEA wind sentences, calculates wind angle, and generates the correct analog voltages to drive the meter — effectively reverse engineering the original signal behavior. Valuable info from: https://www.segeln-forum.de/thread/75527-reparaturhilfe-f%C3%BCr-vdo-windmessgeber/
# Why This Matters
Legacy analog meters still work: Many are still installed on boats or stored in garages, but lack compatible sensors.
Original masthead sensors are rare and expensive: Especially for older VDO systems.
Analog meters are sunlight-readable: Mechanical needles outperform cheap LCDs in bright daylight.
Sustainability: Reusing existing hardware reduces waste and preserves classic marine aesthetics.

This adapter lets you use any NMEA-compatible wind sensor — including DIY or commercial units — to drive your old analog VDO wind meter.
Supported NMEA 0183 Sentences: MWV (Wind speed and angle, relative and true), VWR, VWT.

This is alpha release. Only wind angle is provided. Wind speed is not yet implemented — but will be added in the next version as a pulse output (e.g. 1 Hz = 1 knot).
# How It Works
This adapter was reverse engineered to replicate the analog SIN/COS signals used by legacy VDO wind meters.
NMEA Input: ESP32 receives wind data over Wi-Fi via UDP or TCP. It supports both:
- STA mode: connects to an external Wi-Fi network (e.g. boat router)
- AP mode: creates its own Wi-Fi network (VDO-Cal) → If the NMEA source connects to this AP, it can send data directly to ESP32.
Sentence Parsing: The firmware parses wind angle from NMEA 0183 sentences:

$WIMWV, $IIVWR, $IIVWT - Wind speed is ignored in this first version.
Signal Generation: The wind angle is converted to analog voltages:

SIN = 4000 + 2000 × sin(angle)
COS = 4000 + 2000 × cos(angle)
These are sent to a GP8403 DAC, which outputs 2–6 V signals to drive the meter.
Meter Output: The analog wind meter responds to SIN/COS voltages by moving its needle.

This is Version 1 — only wind direction is supported. Wind speed (as pulse output) will be added in the next version. This adapter only works with SIN/COS-based meters — not all legacy units are compatible. Use at your own risk.