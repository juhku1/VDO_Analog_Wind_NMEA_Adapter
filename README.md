# VDO_Analog_Wind_NMEA_Adapter
This ESP32-based adapter revives legacy VDO wind instruments by allowing them to display wind data from modern NMEA 0183 sources. Originally designed for analog sensors, these gauges can now be integrated into digital marine systems — without replacing the original hardware.

Only VDO Logic Wind (wind angle & wind speed) and VDO Sumlog (wind speed) have been tested so far.

Use at your own risk. If you're using another brand or model, try to measure the original gauge to understand if your device is similar — and what voltages it expects. And before connecting any expensive electronics, measure the voltages and signals the ESP32 is sending.

Again: this works with my setup — but use at your own risk. I am not professional - not even very advanced hobbyist.
But as I managed to make this work I want to share my findings.

What I did: I reverse engineered the VDO Logic Wind sensor behavior using information from various sources, including the German open-boat-projects.org forum.
https://www.segeln-forum.de/thread/75527-reparaturhilfe-f%C3%BCr-vdo-windmessgeber/

They listed some tech specs that helped me out. I also had some old notes on how my electronics were originally connected. My Sumlog is from the 1980s, and the Logic Wind from the late 1990s.

# Wind Direction
Back in the day, wind direction was measured at the masthead using a vane that rotated freely with the wind. Inside the sensor, two outputs — SIN and COS voltages — varied according to the angle. By comparing these signals, the instrument determined the wind angle relative to the boat.

Legacy VDO displays expect these analog signals (typically 2–6 V for SIN/COS), which this adapter reproduces from modern NMEA 0183 wind data using an ESP32 microcontroller and a GP8403 DAC.

We need this DAC because the ESP32’s built-in DAC only outputs 0–3.3 V — not enough to drive the VDO gauge directly. So I used the GP8403 DAC, part of the DFRobot Gravity Series — a 2-channel I²C DAC module with 0–10 V output.

If someone wants to replicate this using the ESP32’s built-in DAC, they’ll need to add an op-amp stage to scale the signal up to 2–6 V. You can use that or any other method to boost the SIN and COS signals — but this code currently supports only the DFRobot module.

Even though the DAC can output up to 10 V, we only use 2–6 V.

The DAC is configured to output two channels:
- Channel 0 (SIN): 4000 + 2000 × sin(angle) in millivolts
- Channel 1 (COS): 4000 + 2000 × cos(angle) in millivolts

These values simulate the original masthead sensor’s behavior:
- 4000 mV is the center voltage (needle at 0°)
- ±2000 mV is the amplitude range for directional movement
- Sounds simple? Check out this diagram from the OBP forum.

The DAC outputs are connected directly to the SIN and COS inputs of the VDO gauge. Pins are explained here:
https://github.com/juhku1/VDO_Analog_Wind_NMEA_Adapter/tree/main/src/images

# Devices used for wind direction:
- ESP32 microcontroller – receives NMEA data and calculates wind angle
- GP8403 DAC (DFRobot Gravity Series) – outputs analog voltages
- VDO Logic Wind analog display – responds to SIN/COS signals to show wind direction

Wi-Fi settings and calibration are accessible via a built-in web interface: SSID: VDO-Cal Password: wind12345

Reminder: This adapter works with SIN/COS-based VDO analog meters only. If you modify it to support other brands or modules, I’d love to hear about it and include your work in the code.


# Wind Speed
This software supports both Logic Wind and older Sumlog-style analog speedometers. These expect a pulse signal (e.g. X Hz = X knots). The ESP32 generates a calibrated pulse output using pin D12. The signal is isolated using a PC817 optocoupler, allowing safe interfacing with 12 V systems.

Devices used for wind speed:
- ESP32 microcontroller – generates pulse signal
- PC817 optocoupler module – provides galvanic isolation and level shifting. It allready has a resistor needed.
- Analog Sumlog-style speedometer Sumlog or Logic Wind – interprets pulse frequency as speed

Signal wire is the key one. The gauge expects it to be pulled to ground briefly for each speed pulse. So how do we simulate that pulse safely using an ESP32?
We use a PC817 optocoupler to isolate the ESP32 from the 12 V system. Here's how it works:
- The ESP32 drives the LED side of the optocoupler.
- When the optocoupler LED lights up, the transistor side of the optocoupler pulls the signal wire to ground — just like the original sensor would.

Pulse frequency is proportional to wind speed and can be calibrated via the web interface.

Supported NMEA sentences are the same as for the wind vane: MWV, VWR, VWT.

You can use this code just for Sumlog, just for Logic Wind — or both.

# Wiring 
Logic Wind pins can be found from images folder.
Sumlog wiring is there too, but maybe a bit harder to understand.
- Green&yellow = signal
- blue = Ground (boat battery)
- brown = 10,1V (These displays do not need 12V and might be a good idea to "underpower them").

Wind Direction

             +---------------------+
             |     ESP32 MCU       |
             |                     |
             |  I2C: SDA / SCL     |
             +----------+----------+
                        |
                        |
                        v
             +---------------------+
             |   GP8403 DAC (I2C)  |
             |  DFRobot Gravity    |
             |  2-ch, 0–10 V out   |
             |  ONY 2–6 V USED!    |
             +----------+----------+
                        |         
         +--------------+--------------+
         |                             |
         v                             v
   SIN Output (VOUT0)           COS Output (VOUT1)
     2–6 V analog                  2–6 V analog
         |                             |
         |                             |
         v                             v
+----------------+           +----------------+
| VDO Wind Gauge |           | VDO Wind Gauge |
|   SIN Input    |           |   COS Input    |
+----------------+           +----------------+

         ↑
         |
   Mechanical needle moves based on SIN/COS voltage




Wind Speed
(can be done simultaniously)

                          ESP32 (3.3 V) side
                        +-------------------+
                        |                   |
                        |   GPIO D12        |
                        |                   |
                        |   GND             |
                        +----+--------+-----+
                             |        |
                             |        |
                             |        v
                             |   +-----------+
                             |   |  PC817    |   Optocoupler
                             |   |  LED side |
                             +--->  Anode    |
                                 |  Cathode  +<--- ESP32 GND
                                 +-----------+
                                       ||
                                       ||  (Galvanic isolation
                                       VV    based on led)
                                 +-----------+
                                 |  PC817    |
                                 |Transistor |     Idea is thet signal goes 
                                 |   side    |     to ground during pulse
                                 | Collector +-----+
                                 | Emitter   +-----+----------------+
                                 +-----------+     |                |
                                                   |                |
                                                   v                v
                                              +---------+     +-----------+
                                              | SIGNAL  |     |   GND     |
                                              |   pin   |     |   pin     |
                                              +----+----+     +-----+-----+
                                                   |                |
                                                   |                |
                                                   |                |
                                                   |                |
                                              +----+----------------+----+
                                              |                         |
                                              |   VDO Speedometer       |
                                              |   (Sumlog / Logic Wind) |
                                              |                         |
                                              |   +12 V pin o-----------+
                                              +-------------------------+


# MORE?
Remember to use fuses ect not mentioned here.

## Tools
### NMEA Wind Sender GUI
This Python tool provides a graphical interface for sending NMEA wind sentences (VWR, MWV, VWT) to the ESP32 adapter over UDP. It allows you to simulate wind angle and speed, test the device, and verify gauge operation without real NMEA data sources.

Location: `tools/nmea_wind_sender_gui.py`


