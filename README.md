# ESP32 Irrigation Control and Soil Moisture & EC Monitoring
This project implements an ESP32-based system for monitoring soil moisture and EC in two irrigation lines.
In addition to sensor measurements, the system controls irrigation and fertigation processes according to a predefined schedule.
Sensor data is uploaded to ThingSpeak for logging and visualization.
<!-- The purpose of this project is to build an ESP32-based system for measuring **soil moisture** and **EC** (electrical conductivity) in two irrigation lines, while also supporting **greenhouse control** via MQTT (valves + fertigation pump).  
The system uploads sensor values to **ThingSpeak** for logging and visualization and can operate in **Auto** (scheduled) or **Manual** mode. -->

---

# Hardware and assembly


![Wiring](https://github.com/sarapost-alt/Humidity-and-EC-sensors/blob/b3874a4443125295f45458cb79a738ebfa62a6dc/scheme.png)
![System setup](https://github.com/sarapost-alt/Humidity-and-EC-sensors/blob/4d6bb3228513b0cd3fbd61bc5cb7c849183c5b80/project%20setup.jpeg)

## Components and Sensors
- **ESP32 board** 
- **Soil moisture sensors (analog)** – 2 channels
- **EC sensors (analog)** – 2 channels

### ESP32 Analog Pins (ADC1 recommended)
| Channel | Sensor | ESP32 Pin |
|--------|--------|----------|
| Line 1 | Moisture | GPIO36 |
| Line 1 | EC       | GPIO39 |
| Line 2 | Moisture | GPIO34 |
| Line 2 | EC       | GPIO35 |

> Note: GPIO34/35/36/39 are input-only pins and work well for analog sensors.

---

## Bill of Materials (BOM)
| ID | Part | Quantity | Notes |
|---:|------|---------:|------|
| 1 | ESP32 board | 1 | Main controller |
| 2 | Soil moisture sensor (analog) | 2 | One per line |
| 3 | EC sensor (analog) | 2 | One per line |
| 4 | Jumper wires / breadboard | 1 | For prototyping |
| 5 | Power supply | 1 | Stable 5V |

---

# Software

## Libraries
- `WiFi.h` (ESP32 core)
- `ThingSpeak.h`
- `PubSubClient.h`
- `time.h` (NTP time)

## Data Upload (ThingSpeak fields)
- Field 1: Moisture line 1 (%)
- Field 2: EC line 1 (calculated units)
- Field 3: Moisture line 2 (%)
- Field 4: EC line 2 (calculated units)

Upload interval: **15 seconds**.

---

# Installation and Setup
1. Install ESP32 support in Arduino IDE (Boards Manager).
2. Install the libraries:
   - ThingSpeak
   - PubSubClient
3. Create a ThingSpeak channel (4 fields) and get:
   - **Channel ID**
   - **Write API Key**
4. Configure MQTT broker settings (server IP, port, user/password).
5. Wire the sensors to the ESP32 (see table above).
6. Upload the code to the ESP32.
7. Open Serial Monitor at **115200 baud** and verify:
   - WiFi connects
   - MQTT connects
   - Sensor values are printed
   - ThingSpeak updates succeed (HTTP 200)

# Hardware Connections
Connect the components according to the following connections:

- Connect the soil moisture sensors and EC sensors to the analog input pins of the ESP32.

- Power the EC sensors using the 3.3V output and GND from the ESP32.

- Power the soil moisture sensors using the VCC output and GND from the ESP32.

- Power the ESP32 either with a 5V external power supply or via the USB connector.

---

<!--# Operating Modes

## Auto Mode (scheduled irrigation)
- **07:00–08:00**: Valve 1 ON (no fertigation)
- **08:00–09:00**: Valve 2 ON  
  - Fertigation ON until minute 58  
  - Flush (fertigation OFF) for the last 2 minutes

## Manual Mode (MQTT commands)
The ESP32 subscribes to manual control topics:
- `greenhouse/controller/mode` (1=Manual, 0=Auto)
- `greenhouse/controller/manual/valve1`
- `greenhouse/controller/manual/valve2`
- `greenhouse/controller/manual/pump`

Outputs are published to:
- `/greenhouse/outside/irrigation/solenoid1`
- `/greenhouse/outside/irrigation/solenoid2`
- `/greenhouse/outside/irrigation/fertigation` -->

---

# Calibration Notes
Moisture is mapped to 0–100% using dry/saturated calibration points per sensor.
![](https://github.com/sarapost-alt/Humidity-and-EC-sensors/blob/c2e142f874ed5485144cf04d59450b82a0d70142/MOIST1.png)

![](https://github.com/sarapost-alt/Humidity-and-EC-sensors/blob/67b35c09f8af41a545b65ba02d0216afd7b46008/MOIST2.png)

EC is converted using a linear factor derived from measured raw ADC values.

The linear factor represents the slope of the calibration graph (slope =0.0013).
ADC * linear factor = EC 
![](https://github.com/sarapost-alt/Humidity-and-EC-sensors/blob/59845461cdb8c609c9785ad0f77d50e00967ce56/EC.png)

---

# Experiment / Results

**ThingSpeak dashboard:** (https://thingspeak.mathworks.com/channels/3216807))

![ThingSpeak dashboard](images/thingspeak.png)

---

# Tips / Troubleshooting
1. Do not publish real credentials in a public repo (use `secrets.h` + `.gitignore`).
2. Use **ADC1 pins** on ESP32 when WiFi is active (more stable).
3. If readings are noisy, consider:
   - stable power
   - shorter wires / shielding
   - averaging multiple ADC reads


