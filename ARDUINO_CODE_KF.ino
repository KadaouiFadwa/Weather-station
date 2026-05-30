/*
 * ESP32 Weather Monitoring Station
 * Features: Temperature, Humidity, Rain, CO2, Noise, Wind monitoring
 * Outputs: Web Interface, Telegram Bot Alerts, CSV Logging
 * Sensors: DHT22, LDR, Rain Sensor, MQ135, I2S Microphone
 * Actuators: Fan with speed control, LED indicators, Buzzer
 */

// ==================== LIBRARIES ====================
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <SD.h>

// ==================== HARDWARE CONFIGURATION ====================
// Sensor Pins
#define DHT_PIN        17      // Temperature & Humidity sensor
#define DHT_TYPE       DHT22
#define LDR_PIN        9       // Light Dependent Resistor
#define RAIN_PIN       8       // Rain sensor
#define MQ135_PIN      16      // CO2 sensor
#define BUZZER_PIN     18      // Alert buzzer

// I2S Microphone Pins (for noise measurement)
#define I2S_WS         4       // Word Select
#define I2S_SD         6       // Serial Data
#define I2S_SCK        5       // Serial Clock

// SD Card Pins
#define SD_CS          10      // Chip Select
#define SD_MOSI        11      // Master Out Slave In
#define SD_MISO        13      // Master In Slave Out
#define SD_SCK         12      // Serial Clock

// Fan Control Pins
#define FAN_PIN        7       // PWM fan control
#define FAN_FREQ       20000   // 20kHz PWM frequency
#define FAN_RES        8       // 8-bit resolution (0-255)

// Status LED Pins
#define CO2_LED        38      // Yellow LED - CO2 alert
#define HUM_LED        41      // Green LED - Humidity alert
#define HOT_LED        39      // Red LED - Temperature alert
#define RAIN_LED       42      // Blue LED - Rain alert
#define NORMAL_LED     40      // White LED - Normal conditions

// Potentiometer for manual fan control
#define POT_PIN        15      // Analog input for fan speed

// ==================== NETWORK CONFIGURATION ====================
const char* ssid = "FADWA KADAOUI";
const char* password = "12341234";

// Telegram Bot Configuration
#define BOT_TOKEN      "7799732665:AAEUcw6kkGj91TPo32Fz41eM_PYIZlNYcvg"
#define CHAT_ID        "7651154382"

// ==================== GLOBAL OBJECTS ====================
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ==================== CALIBRATION VALUES ====================
const int DRY_VALUE = 4095;    // Rain sensor value when dry
const int WET_VALUE = 1500;    // Rain sensor value when wet

// ==================== GLOBAL VARIABLES ====================
// Sensor Readings
float noiseDB = 0;             // Noise level in dB
float temperature = 0;         // Temperature in °C
float humidity = 0;            // Humidity in %
int lightLevel = 0;            // Light intensity in %
float rainIntensity = 0;       // Rain intensity in %
float co2Level = 0;            // CO2 level in ppm
float windSpeed = 0;           // Wind speed in m/s
float windPressure = 0;        // Wind pressure in Pa
int rpm = 0;                   // Fan RPM

// Timing Variables
unsigned long lastLogTime = 0;         // Last CSV log time
unsigned long lastAlert = 0;           // Last temperature alert
unsigned long lastHumAlert = 0;        // Last humidity alert
unsigned long lastRainAlert = 0;       // Last rain alert
unsigned long lastCO2Alert = 0;        // Last CO2 alert
unsigned long lastWindAlert = 0;       // Last wind alert
unsigned long lastNoiseAlert = 0;      // Last noise alert

// Alert Intervals (milliseconds)
const unsigned long ALERT_INTERVAL = 300000;       // 5 minutes
const unsigned long FAST_ALERT_INTERVAL = 5000;    // 5 seconds

// Wind Calculation Constants
const float FAN_DIAMETER = 0.07;       // 7 cm fan diameter
const float AIR_DENSITY = 1.225;       // kg/m³ at sea level
const float WIND_ALERT_THRESHOLD = 9.2; // m/s threshold for wind alert

// File Management
const char* CSV_FILENAME = "/data.csv";

// ==================== FUNCTION DECLARATIONS ====================
void setupI2S();
float readNoiseDB();
void sendTelegramAlert(const String& msg);
void sendFanButtons();
void handleFanControl(const String& command);
void saveCSV(const String& row);
void handleRoot();
void handleWeather();
void updateSensorReadings();
void checkAlerts();
void controlFan();
void updateTelegramBot();
void logSensorData();

// ==================== I2S MICROPHONE SETUP ====================
void setupI2S() {
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64
    };

    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

// ==================== NOISE MEASUREMENT ====================
float readNoiseDB() {
    int32_t samples[256];
    size_t bytes_read;

    i2s_read(I2S_NUM_0, (char*)samples, sizeof(samples), &bytes_read, portMAX_DELAY);
    int samples_read = bytes_read / sizeof(int32_t);

    double sumSquares = 0.0;
    for (int i = 0; i < samples_read; i++) {
        float sample = (float)samples[i] / 2147483648.0f;
        sumSquares += sample * sample;
    }

    float rms = sqrt(sumSquares / samples_read);
    float dB = 20.0 * log10(rms) + 100.0;
    
    return dB;
}

// ==================== TELEGRAM BOT FUNCTIONS ====================
void sendTelegramAlert(const String& msg) {
    Serial.println("Sending Telegram alert: " + msg);
    bool sent = bot.sendMessage(String(CHAT_ID), msg, "");
    
    if (sent) {
        Serial.println("✅ Alert sent");
    } else {
        Serial.println("❌ Failed to send alert");
    }
}

void sendFanButtons() {
    String keyboardJson = R"({
        "inline_keyboard": [
            [
                {"text": "🌀 Turn Fan ON", "callback_data": "fan_on"},
                {"text": "🛑 Turn Fan OFF", "callback_data": "fan_off"}
            ]
        ]
    })";
    
    bot.sendMessageWithInlineKeyboard(
        String(CHAT_ID),
        "💨 Fan Control:\nChoose an action:",
        "",
        keyboardJson
    );
}

void handleFanControl(const String& command) {
    if (command == "fan_on") {
        ledcWrite(FAN_PIN, 200);
        bot.sendMessage(String(CHAT_ID), "✅ WIND ON 🌀", "");
    } else if (command == "fan_off") {
        ledcWrite(FAN_PIN, 0);
        bot.sendMessage(String(CHAT_ID), "🛑 WIND OFF", "");
    }
}

// ==================== DATA LOGGING ====================
void saveCSV(const String& row) {
    File file = SD.open(CSV_FILENAME, FILE_APPEND);
    
    if (file) {
        file.println(row);
        file.close();
        Serial.println("✅ Data logged to CSV");
    } else {
        Serial.println("❌ Failed to write to CSV");
    }
}

// ==================== SETUP FUNCTION ====================
void setup() {
    Serial.begin(115200);
    Serial.println("\n🌤️ ESP32 Weather Station Starting...");
    
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    pinMode(POT_PIN, INPUT);
    pinMode(LDR_PIN, INPUT);
    pinMode(RAIN_PIN, INPUT);
    pinMode(MQ135_PIN, INPUT);
    
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(HOT_LED, OUTPUT);
    pinMode(RAIN_LED, OUTPUT);
    pinMode(NORMAL_LED, OUTPUT);
    pinMode(CO2_LED, OUTPUT);
    pinMode(HUM_LED, OUTPUT);
    
    digitalWrite(HOT_LED, LOW);
    digitalWrite(RAIN_LED, LOW);
    digitalWrite(NORMAL_LED, LOW);
    digitalWrite(CO2_LED, LOW);
    digitalWrite(HUM_LED, LOW);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("❌ SD Card initialization failed!");
    } else {
        Serial.println("✅ SD Card initialized");
        
        if (!SD.exists(CSV_FILENAME)) {
            File file = SD.open(CSV_FILENAME, FILE_WRITE);
            if (file) {
                file.println("date;time;temp;hum;light;rain;co2;noise;wind_kmh;wind_pressure_pa");
                file.close();
                Serial.println("✅ CSV file created with headers");
            }
        }
    }
    
    dht.begin();
    setupI2S();
    
    ledcAttach(FAN_PIN, FAN_FREQ, FAN_RES);
    ledcWrite(FAN_PIN, 0);
    
    Serial.print("📶 Connecting to WiFi: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\n✅ WiFi connected!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    
    client.setInsecure();
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    delay(2000);
    
    bot.sendMessage(String(CHAT_ID), "🤖 Weather Station Started! System is online.", "");
    sendFanButtons();
    
    server.on("/", handleRoot);
    server.on("/weather", handleWeather);
    server.on("/download", []() {
        File file = SD.open(CSV_FILENAME);
        if (!file) {
            server.send(500, "text/plain", "File not found");
            return;
        }
        server.streamFile(file, "text/csv");
        file.close();
    });
    
    server.begin();
    Serial.println("🌐 Web server started on port 80");
    Serial.println("\n✅ Setup complete! System is ready.");
}

// ==================== SENSOR READING FUNCTIONS ====================
void updateSensorReadings() {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    
    lightLevel = map(analogRead(LDR_PIN), 0, 4095, 0, 100);
    
    int rainValue = analogRead(RAIN_PIN);
    rainIntensity = map(rainValue, WET_VALUE, DRY_VALUE, 0, 100);
    rainIntensity = constrain(rainIntensity, 0, 100);
    
    co2Level = analogRead(MQ135_PIN);
    co2Level = map(co2Level, 0, 4095, 400, 2000);
    
    noiseDB = readNoiseDB();
}

// ==================== ALERT CHECKING FUNCTIONS ====================
void checkAlerts() {
    digitalWrite(HOT_LED, LOW);
    digitalWrite(RAIN_LED, LOW);
    digitalWrite(NORMAL_LED, LOW);
    digitalWrite(CO2_LED, LOW);
    digitalWrite(HUM_LED, LOW);
    noTone(BUZZER_PIN);
    
    unsigned long currentTime = millis();
    
    if (humidity > 60 && currentTime - lastHumAlert >= ALERT_INTERVAL) {
        lastHumAlert = currentTime;
        digitalWrite(HUM_LED, HIGH);
        tone(BUZZER_PIN, 1200, 500);
        sendTelegramAlert("💧 High Humidity Alert: " + String(humidity) + "%");
    }
    
    if (temperature > 25 && currentTime - lastAlert >= ALERT_INTERVAL) {
        lastAlert = currentTime;
        digitalWrite(HOT_LED, HIGH);
        tone(BUZZER_PIN, 1700, 800);
        sendTelegramAlert("🔥 High Temperature Alert: " + String(temperature) + "°C");
    }
    
    if (co2Level > 1000 && currentTime - lastCO2Alert >= ALERT_INTERVAL) {
        lastCO2Alert = currentTime;
        digitalWrite(CO2_LED, HIGH);
        tone(BUZZER_PIN, 1000, 800);
        sendTelegramAlert("⚠️ High CO₂ Level: " + String(co2Level) + " ppm!");
    }
    
    if (windSpeed > WIND_ALERT_THRESHOLD && currentTime - lastWindAlert >= FAST_ALERT_INTERVAL) {
        lastWindAlert = currentTime;
        sendTelegramAlert("💨 High Wind Alert: " + String(windSpeed * 3.6, 1) + " km/h");
    }
    
    if (noiseDB > 60 && currentTime - lastNoiseAlert >= FAST_ALERT_INTERVAL) {
        lastNoiseAlert = currentTime;
        tone(BUZZER_PIN, 200, 800);
        sendTelegramAlert("🔊 High Noise Alert: " + String(noiseDB, 1) + " dB");
    }
    
    if (rainIntensity > 70 && currentTime - lastRainAlert >= ALERT_INTERVAL) {
        lastRainAlert = currentTime;
        digitalWrite(RAIN_LED, HIGH);
        tone(BUZZER_PIN, 700, 800);
        sendTelegramAlert("🌧️ Rain Alert: " + String(rainIntensity) + "% intensity");
    }
    
    if (temperature <= 25 && humidity <= 60 && co2Level <= 1000 && 
        rainIntensity <= 70 && noiseDB <= 60) {
        digitalWrite(NORMAL_LED, HIGH);
    }
}

// ==================== FAN CONTROL ====================
void controlFan() {
    int potValue = analogRead(POT_PIN);
    int pwmValue = map(potValue, 0, 4095, 0, 100);
    int pwmOutput = map(pwmValue, 0, 100, 0, 255);
    
    ledcWrite(FAN_PIN, pwmOutput);
    
    rpm = map(pwmValue, 0, 100, 0, 3000);
    windSpeed = 3.1416 * FAN_DIAMETER * rpm / 60.0;
    windPressure = 0.5 * AIR_DENSITY * windSpeed * windSpeed;
}

// ==================== TELEGRAM BOT UPDATE ====================
void updateTelegramBot() {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    
    while (numNewMessages > 0) {
        for (int i = 0; i < numNewMessages; i++) {
            String chatId = bot.messages[i].chat_id;
            String text = bot.messages[i].text;
            
            if (text == "/start") {
                bot.sendMessage(chatId, "🌦️ Weather Station Ready!\nMonitor your environment in real-time.", "");
                sendFanButtons();
            }
            
            if (text == "fan_on" || text == "fan_off") {
                handleFanControl(text);
            }
        }
        
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
}

// ==================== DATA LOGGING ====================
void logSensorData() {
    if (millis() - lastLogTime >= 5000) {
        lastLogTime = millis();
        
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        char timestamp[30];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d,%H:%M:%S", &timeinfo);
        
        String row = String(timestamp) + ";" +
                     String(temperature) + ";" +
                     String(humidity) + ";" +
                     String(lightLevel) + ";" +
                     String(rainIntensity) + ";" +
                     String(co2Level) + ";" +
                     String(noiseDB) + ";" +
                     String(windSpeed * 3.6, 1) + ";" +
                     String(windPressure, 2);
        
        saveCSV(row);
    }
}

// ==================== MAIN LOOP ====================
void loop() {
    server.handleClient();
    updateSensorReadings();
    controlFan();
    updateTelegramBot();
    checkAlerts();
    logSensorData();
    delay(10);
}

// ==================== WEB SERVER HANDLERS ====================
void handleWeather() {
    DynamicJsonDocument doc(1024);
    
    doc["temp"] = temperature;
    doc["hum"] = humidity;
    doc["rain"] = rainIntensity;
    doc["light"] = lightLevel;
    doc["co2"] = co2Level;
    doc["noise"] = noiseDB;
    doc["wind"] = windSpeed * 3.6;
    doc["windP"] = windPressure;
    doc["fan"] = ledcRead(FAN_PIN);
    
    Serial.println("📊 Sending JSON data to web...");
    
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handleRoot() {
String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Alger Weather Station 🌙</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
* {margin:0; padding:0; box-sizing:border-box; font-family:'Poppins',sans-serif;}
body {
  background: radial-gradient(circle at top,#0d1b2a,#000);
  color:#fff;
  text-align:center;
  transition: background 1s, color 1s;
}
header {margin-top:30px;}
header img {
  width: 350px;
  max-width: 90%;
  height: auto;
  border-radius: 20px;
  box-shadow: 0 0 30px rgba(0,153,255,0.5);
  animation: pulse 3s ease-in-out infinite;
}
@keyframes pulse {0%,100%{transform:scale(1);}50%{transform:scale(1.02);}}
.datetime {
  margin-top:15px;
  font-size:1.1rem;
  letter-spacing:1px;
  color:#9bc9ff;
  text-shadow:0 0 8px #0ff;
}
.card-container {display:flex; flex-wrap:wrap; justify-content:center; margin:20px;}
.card {
  flex:1 1 220px;
  margin:15px;
  padding:20px;
  border-radius:20px;
  background: rgba(255,255,255,0.05);
  backdrop-filter: blur(10px);
  box-shadow: 0 0 30px rgba(0,70,120,0.5);
  transition: transform 0.3s, box-shadow 0.3s;
}
.card:hover {transform: translateY(-5px); box-shadow: 0 0 50px rgba(0,153,255,0.7);}
.card h3 {font-size:1.2rem; margin-bottom:10px; color:#fff;}
.sensor-value {font-size:1.5rem; font-weight:600; margin-bottom:8px;}
.progress-bar {width:100%; height:10px; background:#222; border-radius:8px; overflow:hidden; margin:5px 0;}
.progress {height:100%; border-radius:8px 0 0 8px; transition: width 0.5s; position: relative;}
canvas {margin-top:15px; width:95%!important; height:200px!important;}
.condition {font-size:1.2rem; margin-top:15px; color:#ffb703; text-shadow:0 0 8px rgba(255,183,3,0.6);}
.light-mode {background: radial-gradient(circle at top,#d9f2ff,#fefefe); color:#000;}
.dark-mode {background: radial-gradient(circle at top,#0d1b2a,#000); color:#fff;}
.raindrops::after {content: "💧💧"; position: absolute; top: -10px; left: 50%; transform: translateX(-50%); font-size: 1rem; animation: fall 1.2s linear infinite; opacity: 0.6;}
.co2-clouds::after {content: "☁️☁️"; position: absolute; top: -10px; left: 50%; transform: translateX(-50%); font-size: 1rem; animation: puff 11.5s ease-in-out infinite; opacity: 0.5;}
@keyframes fall {0%{transform: translateY(0);}100%{transform: translateY(20px);}}
@keyframes puff {0%,100%{transform: scale(1);}50%{transform: scale(1.2);}}
</style>
</head>
<body id="body" class="dark-mode">
<header><img src="https://i.imgur.com/Psv9U2T.png" alt="Weather Logo"></header>
<div class="datetime" id="datetime">Loading time...</div>
<div class="card-container">

<!-- Temperature -->
<div class="card">
<h3>🌡️ Temperature</h3>
<div class="sensor-value" id="temp">-- °C</div>
<div class="progress-bar"><div class="progress" id="temp-progress" style="background:#ff7300;width:0%;"></div></div>
</div>

<!-- Noise -->
<div class="card">
<h3>🔊 Noise</h3>
<div class="sensor-value" id="noise">-- dB</div>
<div class="progress-bar"><div class="progress" id="noise-progress" style="background:#ffaa00;width:0%;"></div></div>
</div>

<!-- CO2 -->
<div class="card">
<h3>🫁 CO₂</h3>
<div class="sensor-value" id="co2">-- ppm</div>
<div class="progress-bar"><div class="progress co2-clouds" id="co2-progress" style="background:#ff4d4d;width:0%;"></div></div>
</div>

<!-- Humidity -->
<div class="card">
<h3>💧 Humidity</h3>
<div class="sensor-value" id="hum">-- %</div>
<div class="progress-bar"><div class="progress" id="hum-progress" style="background:#00bfff;width:0%;"></div></div>
</div>

<!-- Rain -->
<div class="card">
<h3>🌧️ Rain</h3>
<div class="sensor-value" id="rain">-- %</div>
<div class="progress-bar"><div class="progress raindrops" id="rain-progress" style="background:#1e90ff;width:0%;"></div></div>
</div>

<!-- Light -->
<div class="card">
<h3>💡 Light</h3>
<div class="sensor-value" id="light">-- %</div>
<div class="progress-bar"><div class="progress" id="light-progress" style="background:#f3ff00;width:0%;"></div></div>
</div>

<!-- Wind Speed -->
<div class="card">
<h3>🌪 Wind Speed</h3>
<div class="sensor-value" id="wind">-- km/h</div>
<div class="progress-bar"><div class="progress" id="wind-progress" style="background:#00ffcc;width:0%;"></div></div>
</div>

<!-- Wind Pressure -->
<div class="card">
<h3>🧭 Wind Pressure</h3>
<div class="sensor-value" id="windP">-- Pa</div>
</div>
</div>

<!-- Chart -->
<div class="card" style="width:90%;max-width:800px;margin:0 auto;">
<h3>📈 Sensor Trends</h3>
<canvas id="trendChart"></canvas>
</div>

<div class="condition" id="condition">🌫️ Loading...</div>
<a href="/download" target="_blank"><button style="padding:10px 20px; background:#3498db; color:white; border:none; border-radius:10px; font-size:16px; margin:20px; cursor:pointer;">📥 Download CSV Data</button></a>

<script>
const ctx=document.getElementById('trendChart').getContext('2d');
const chart=new Chart(ctx,{
  type:'line',
  data:{
    labels:[],
    datasets:[
      {label:'Temp °C', borderColor:'#ff7300', data:[], fill:false},
      {label:'Hum %', borderColor:'#00bfff', data:[], fill:false},
      {label:'CO₂ ppm', borderColor:'#ff4d4d', data:[], fill:false},
      {label:'Rain %', borderColor:'#1e90ff', data:[], fill:false},
      {label:'Noise dB', borderColor:'#ffaa00', data:[], fill:false},
      {label:'Wind km/h', borderColor:'#00ffcc', data:[], fill:false},
      {label:'Wind Pressure Pa', borderColor:'#00ffff', data:[], fill:false}
    ]
  },
  options:{
    responsive:true, 
    scales:{
      x:{
        ticks:{color:'#9bc9ff'},
        grid:{color:'rgba(155,201,255,0.2)'}
      },
      y:{
        ticks:{color:'#9bc9ff'},
        grid:{color:'rgba(155,201,255,0.2)'}
      }
    }
  }
});

async function updateWeather(){
  try{
    const res=await fetch('/weather');
    const data=await res.json();
    
    console.log("Received data:", data);

    // Update ALL values
    document.getElementById('temp').innerText = data.temp.toFixed(1) + ' °C';
    document.getElementById('hum').innerText = data.hum.toFixed(1) + ' %';
    document.getElementById('rain').innerText = data.rain.toFixed(1) + ' %';
    document.getElementById('co2').innerText = data.co2.toFixed(0) + ' ppm';
    document.getElementById('light').innerText = data.light.toFixed(0) + ' %';
    document.getElementById('noise').innerText = data.noise.toFixed(1) + ' dB';
    document.getElementById('wind').innerText = data.wind.toFixed(1) + ' km/h';
    document.getElementById('windP').innerText = data.windP.toFixed(2) + ' Pa';

    // Update ALL progress bars
    document.getElementById('temp-progress').style.width = Math.min((data.temp/40)*100, 100) + '%';
    document.getElementById('hum-progress').style.width = Math.min(data.hum, 100) + '%';
    document.getElementById('rain-progress').style.width = Math.min(data.rain, 100) + '%';
    document.getElementById('co2-progress').style.width = Math.min((data.co2/2000)*100, 100) + '%';
    document.getElementById('light-progress').style.width = Math.min(data.light, 100) + '%';
    document.getElementById('noise-progress').style.width = Math.min((data.noise/100)*100, 100) + '%';
    document.getElementById('wind-progress').style.width = Math.min((data.wind/50)*100, 100) + '%';

    // Update chart
    const now=new Date().toLocaleTimeString();
    chart.data.labels.push(now);
    chart.data.datasets[0].data.push(data.temp);
    chart.data.datasets[1].data.push(data.hum);
    chart.data.datasets[2].data.push(data.co2);
    chart.data.datasets[3].data.push(data.rain);
    chart.data.datasets[4].data.push(data.noise);
    chart.data.datasets[5].data.push(data.wind);
    chart.data.datasets[6].data.push(data.windP);

    // Keep only last 10 data points
    if(chart.data.labels.length>10){
      chart.data.labels.shift(); 
      chart.data.datasets.forEach(d=>d.data.shift());
    }
    chart.update();

    // Weather condition
    let cond="";
    if(data.rain>70) cond="⛈️ Heavy Rain";
    else if(data.rain>40) cond="🌧 Rainy";
    else if(data.temp>25 && data.hum<60) cond="🔥 Hot and dry";
    else if(data.temp>30 && data.hum>=60) cond="☀️ Hot and humid";
    else if(data.temp<25 && data.hum>70) cond="🌫️ Cold and damp";
    else if(data.temp<15 && data.hum<40) cond="❄️ Cold and dry";
    else if(data.light>60 && data.hum>40) cond="🌤️ Warm and bright";
    else cond="🌈 Pleasant and calm";
    document.getElementById('condition').innerText=cond;

    // Light/Dark theme
    const body=document.getElementById('body');
    if(data.light>60) {
      body.className='light-mode';
    } else {
      body.className='dark-mode';
    }

  } catch(error) {
    console.error("Error fetching data:", error);
    document.getElementById('condition').innerText="⚠️ Offline — No Data";
  }
}

// Time update
function updateTime(){
  const now=new Date();
  document.getElementById('datetime').innerHTML=
  `${now.toLocaleDateString('en-GB',{weekday:'long',day:'numeric',month:'long',year:'numeric'})} — ${now.toLocaleTimeString()}`;
}

// Start everything
setInterval(updateTime,1000);
setInterval(updateWeather,2000);
updateTime();
updateWeather();
</script>
</body>
</html>
)=====";  
    server.send(200, "text/html", html);  
}