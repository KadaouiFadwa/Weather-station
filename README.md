
# 🌤️ ESP32 Weather Monitoring Station

A comprehensive IoT weather station built on the ESP32 platform that monitors environmental conditions in real-time, serves a live web dashboard, sends Telegram alerts, and logs data to an SD card.

---

## 📋 Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring / Pin Configuration](#wiring--pin-configuration)
- [Software Dependencies](#software-dependencies)
- [Configuration](#configuration)
- [How It Works](#how-it-works)
- [Web Interface](#web-interface)
- [Telegram Bot](#telegram-bot)
- [Alert Thresholds](#alert-thresholds)
- [Data Logging](#data-logging)
- [Project Structure](#project-structure)

---

## ✨ Features

- 🌡️ **Temperature & Humidity** monitoring via DHT22
- 🌧️ **Rain intensity** detection with analog rain sensor
- 🫁 **CO₂ / Air quality** measurement via MQ135 gas sensor
- 🔊 **Noise level** measurement via I2S digital microphone (in dB)
- 💡 **Light intensity** via LDR (Light Dependent Resistor)
- 💨 **Wind speed & pressure** calculated from fan RPM (controlled by potentiometer)
- 🌐 **Live web dashboard** with real-time charts and weather condition inference
- 🤖 **Telegram bot** for remote monitoring and fan control
- 💾 **CSV data logging** to SD card every 5 seconds
- 📥 **CSV download** directly from the web interface
- 🚨 **Multi-sensor alert system** with LED indicators and buzzer

---

## 🔧 Hardware Requirements

| Component | Description |
|---|---|
| ESP32 | Main microcontroller |
| DHT22 | Temperature & Humidity sensor |
| LDR | Light Dependent Resistor |
| Rain Sensor | Analog rain detection module |
| MQ135 | CO₂ / Air quality gas sensor |
| I2S Microphone | Digital microphone (e.g. INMP441) |
| SD Card Module | SPI-based SD card for data logging |
| PWM Fan (5V) | Used to simulate wind; speed read as wind proxy |
| Potentiometer | Manual fan speed control |
| Buzzer | Audible alerts |
| LEDs × 5 | Status indicators (Yellow, Green, Red, Blue, White) |

---

## 📌 Wiring / Pin Configuration

### Sensors & Outputs

| Component | ESP32 Pin |
|---|---|
| DHT22 (Temp/Humidity) | GPIO 17 |
| LDR (Light) | GPIO 9 |
| Rain Sensor | GPIO 8 |
| MQ135 (CO₂) | GPIO 16 |
| Buzzer | GPIO 18 |
| Potentiometer | GPIO 15 |
| Fan (PWM) | GPIO 7 |

### I2S Microphone

| Signal | ESP32 Pin |
|---|---|
| WS (Word Select) | GPIO 4 |
| SD (Serial Data) | GPIO 6 |
| SCK (Clock) | GPIO 5 |

### SD Card (SPI)

| Signal | ESP32 Pin |
|---|---|
| CS | GPIO 10 |
| MOSI | GPIO 11 |
| MISO | GPIO 13 |
| SCK | GPIO 12 |

### LED Indicators

| LED | Color | ESP32 Pin | Condition |
|---|---|---|---|
| CO2_LED | Yellow | GPIO 38 | CO₂ > 1000 ppm |
| HUM_LED | Green | GPIO 41 | Humidity > 60% |
| HOT_LED | Red | GPIO 39 | Temperature > 25°C |
| RAIN_LED | Blue | GPIO 42 | Rain intensity > 70% |
| NORMAL_LED | White | GPIO 40 | All conditions normal |

---

## 📦 Software Dependencies

Install the following libraries via the Arduino Library Manager or PlatformIO:

| Library | Purpose |
|---|---|
| `WiFi.h` | ESP32 WiFi connectivity |
| `WebServer.h` | HTTP server for web dashboard |
| `ArduinoJson` | JSON serialization for API endpoint |
| `DHT sensor library` | DHT22 temperature/humidity reading |
| `WiFiClientSecure` | HTTPS for Telegram API |
| `UniversalTelegramBot` | Telegram bot messaging |
| `driver/i2s.h` | I2S microphone interface (ESP-IDF built-in) |
| `SPI.h` / `SD.h` | SD card access |

---

## ⚙️ Configuration

Before uploading, update the following constants in the sketch:

```cpp
// WiFi credentials
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Telegram Bot
#define BOT_TOKEN  "YOUR_BOT_TOKEN"
#define CHAT_ID    "YOUR_CHAT_ID"
```

> **Getting a Telegram Bot Token:** Open Telegram, message `@BotFather`, run `/newbot`, and copy the token it provides.
> **Getting your Chat ID:** Message `@userinfobot` in Telegram to retrieve your numeric chat ID.

---

## ⚙️ How It Works

### Sensor Loop (every ~10ms)
1. `updateSensorReadings()` — reads all sensors
2. `controlFan()` — reads potentiometer, sets PWM, calculates wind speed and pressure
3. `updateTelegramBot()` — checks for new Telegram messages/commands
4. `checkAlerts()` — evaluates thresholds, triggers LEDs, buzzer, and Telegram alerts
5. `logSensorData()` — writes a row to the SD card CSV every 5 seconds

### Wind Speed Calculation
Wind speed is derived from the fan RPM, which itself is mapped from the potentiometer value:

```
RPM         = map(potValue%, 0, 100, 0, 3000)
Wind Speed  = π × FAN_DIAMETER × RPM / 60   (m/s)
Wind Pressure = 0.5 × AIR_DENSITY × WindSpeed²   (Pa)
```

Fan diameter is set to `0.07 m` (7 cm). Adjust `FAN_DIAMETER` if using a different fan.

### Noise Measurement
256 samples are read from the I2S microphone, RMS amplitude is computed, and converted to dB:
```
dB = 20 × log10(RMS) + 100
```

---

## 🌐 Web Interface

Access the dashboard at `http://<ESP32_IP>/` after connecting to your network.

The IP address is printed to the Serial Monitor on boot.

| Endpoint | Description |
|---|---|
| `GET /` | Live HTML dashboard with charts |
| `GET /weather` | JSON API — current sensor readings |
| `GET /download` | Download the full CSV log file |

The dashboard auto-refreshes every **2 seconds** and displays:
- Real-time values with animated progress bars
- A live trend chart (last 10 readings) for all sensors
- An inferred weather condition string (e.g. "🔥 Hot and dry")
- Light/Dark mode toggle based on light intensity

---

## 🤖 Telegram Bot

The bot sends automatic alerts and accepts commands:

### Commands

| Command | Description |
|---|---|
| `/start` | Sends a welcome message and fan control buttons |
| Inline: `🌀 Turn Fan ON` | Sets fan PWM to speed 200/255 |
| Inline: `🛑 Turn Fan OFF` | Turns fan off (PWM = 0) |

---

## 🚨 Alert Thresholds

| Sensor | Threshold | Alert Interval |
|---|---|---|
| Temperature | > 25°C | 5 minutes |
| Humidity | > 60% | 5 minutes |
| CO₂ | > 1000 ppm | 5 minutes |
| Rain Intensity | > 70% | 5 minutes |
| Wind Speed | > 9.2 m/s (~33 km/h) | 5 seconds |
| Noise | > 60 dB | 5 seconds |

Each alert also triggers the corresponding LED and a buzzer tone.

---

## 💾 Data Logging

Data is written to `/data.csv` on the SD card every **5 seconds**.

### CSV Format

```
date;time;temp;hum;light;rain;co2;noise;wind_kmh;wind_pressure_pa
2025-01-01;12:00:00;24.3;55.1;72;0.0;420;45.2;12.5;0.09
```

The file is created automatically with headers on first boot if it does not exist.

---

## 🗂️ Project Structure

```
weather_station/
├── weather_station.ino   # Main Arduino sketch (all-in-one)
└── README.md             # This file
```

---

## 📝 Notes

- The MQ135 sensor output is linearly mapped from ADC range (0–4095) to CO₂ range (400–2000 ppm). For accurate ppm readings, a proper calibration curve should be applied.
- The NTP time server (`pool.ntp.org`) is used for timestamping CSV logs. Ensure the ESP32 has internet access, or timestamps will show epoch time.
- `client.setInsecure()` is used for Telegram HTTPS — acceptable for hobby projects; for production, use a proper certificate.
