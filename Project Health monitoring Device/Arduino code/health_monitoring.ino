#include <Wire.h>
#include "MAX30105.h"
#include <Adafruit_MLX90614.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <FirebaseESP32.h> 
#include <HTTPClient.h>         
#include <WiFiClientSecure.h>   
#include "health_model.h" 

// ==========================================
// 1. CREDENTIALS & MULTI-PATIENT SETUP
// ==========================================
#define WIFI_SSID "realme"
#define WIFI_PASSWORD "abdy1234"
#define FIREBASE_HOST "fever-detector-53654-default-rtdb.asia-southeast1.firebasedatabase.app" 
#define FIREBASE_AUTH "AIzaSyB2MjD3dMKJUGOy5r470Y1Pxu6p2wcDLiU" 

String PATIENT_UID = "PATIENT_001"; 

// ==========================================
// 2. TELEGRAM BOT SETTINGS
// ==========================================
#define BOT_TOKEN "8592467261:AAESxo5hPSr39li2fgJtL_nlpMKde-93m34" 
#define CHAT_ID "1309209895"    

unsigned long lastAlertTime = 0;
// ---> CHANGED: Cooldown set to 300,000 milliseconds (5 Minutes) <---
const unsigned long ALERT_COOLDOWN = 300000; 
bool hasAlertedBefore = false;
int abnormalCount = 0; 

// ==========================================
// 3. AI CONFIGURATION & SCALING
// ==========================================
Eloquent::ML::Port::RandomForest ml;

const float TEMP_MEAN = 98.3877;  const float TEMP_SCALE = 3.1125;
const float HR_MEAN = 84.0470;    const float HR_SCALE = 26.5208;
const float SPO2_MEAN = 95.3812;  const float SPO2_SCALE = 1.5264;

// ==========================================
// 4. OBJECTS & VITALS VARIABLES
// ==========================================
MAX30105 particleSensor;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

long rollingAverage = 0; bool isAboveAverage = false; long lastBeatTime = 0;
const byte RATE_SIZE = 4; byte rates[RATE_SIZE]; byte rateSpot = 0; int beatAvg = 0;

long ir_min = 999999, ir_max = 0, red_min = 999999, red_max = 0;
long lastScreenUpdate = 0, lastFirebaseUpdate = 0; 
int current_spo2 = 98; float filtered_spo2 = 98.0;

bool fingerOn = false; 

// Removed anim_hr completely
float anim_temp = 0.0;
int anim_spo2 = 0;

// ==========================================
// SETUP FUNCTION
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("\n\n=== ESP32 IS AWAKE ===");

  Serial.println("[TEST] Turning on OLED Display...");
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay(); 
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10); 
  display.println("Booting..."); 
  display.display();

  Serial.println("[TEST] Turning on MLX90614 Temp Sensor...");
  mlx.begin();

  Serial.println("[TEST] Random Forest AI Model Loaded into Memory.");
  
  Serial.println("[TEST] Turning on MAX30102 Pulse Sensor...");
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("❌ ERROR: MAX30102 not found. Check wiring!");
    while (1); 
  }
  particleSensor.setup(127, 4, 2, 100, 411, 4096); 

  Serial.print("[TEST] Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA); 
  WiFi.disconnect();   
  delay(100);  
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); 
  }
  Serial.println("\n✅ Wi-Fi Connected!");
  
  Serial.println("[TEST] Connecting to Firebase...");
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println("=== SETUP COMPLETE! ENTERING MAIN LOOP ===");
}

// ==========================================
// MAIN LOOP FUNCTION
// ==========================================
void loop() {
  particleSensor.check();
  
  while (particleSensor.available()) {
    long irValue = particleSensor.getFIFORed(); 
    long redValue = particleSensor.getFIFOIR(); 
    particleSensor.nextSample(); 

    if (irValue > 50000) {
      
      if (!fingerOn) {
        fingerOn = true;
        rollingAverage = irValue; 
        lastBeatTime = millis();  
        
        beatAvg = 0; 
        for(byte i=0; i<RATE_SIZE; i++) rates[i] = 0;
      }

      rollingAverage = (rollingAverage * 99 + irValue) / 100;
      
      if (irValue > rollingAverage + 5 && !isAboveAverage) {
        isAboveAverage = true; 
        long delta = millis() - lastBeatTime;
        lastBeatTime = millis();
        float bpm = 60000.0 / delta; 
        
        if (bpm >= 55 && bpm <= 120) {
          rates[rateSpot++] = (byte)bpm; rateSpot %= RATE_SIZE; 
          
          beatAvg = 0;
          byte validCount = 0;
          for (byte x = 0; x < RATE_SIZE; x++) {
              if (rates[x] > 0) {
                  beatAvg += rates[x];
                  validCount++;
              }
          }
          if (validCount > 0) beatAvg /= validCount; 
        }
      } else if (irValue < rollingAverage - 5) {
          isAboveAverage = false;
      }

      if (millis() - lastBeatTime > 3000) {
          lastBeatTime = millis();
      }

      if (irValue < ir_min) ir_min = irValue; if (irValue > ir_max) ir_max = irValue;
      if (redValue < red_min) red_min = redValue; if (redValue > red_max) red_max = redValue;

      if (millis() - lastScreenUpdate > 1000) { 
        float ir_ac = ir_max - ir_min; float ir_dc = (ir_max + ir_min) / 2.0;
        float red_ac = red_max - red_min; float red_dc = (red_max + red_min) / 2.0;

        if (ir_dc > 0 && red_dc > 0 && ir_ac > 0 && red_ac > 0) {
           float r = (red_ac / red_dc) / (ir_ac / ir_dc);
           float computed_spo2 = 104.0 - (17.0 * r) + 10.0;
           
           if (computed_spo2 > 100.0) computed_spo2 = 100.0;

           if (computed_spo2 >= 80 && computed_spo2 <= 100) {
               filtered_spo2 = (filtered_spo2 * 0.8) + (computed_spo2 * 0.2);
               current_spo2 = (int)filtered_spo2;
           }
        }

        float tempF = mlx.readObjectTempF();

        Serial.print("Live Vitals -> HR: "); Serial.print(beatAvg);
        Serial.print(" | SpO2: "); Serial.print(current_spo2);
        Serial.print("% | Temp: "); Serial.print(tempF); 
        Serial.println("F");

        while (anim_temp < tempF || anim_spo2 < current_spo2) {
            if (anim_temp < tempF) anim_temp += 3.5; 
            if (anim_temp > tempF) anim_temp = tempF; 

            if (anim_spo2 < current_spo2) anim_spo2 += 4;
            if (anim_spo2 > current_spo2) anim_spo2 = current_spo2;

            display.clearDisplay(); display.setTextSize(2);
            
            display.setCursor(0, 0); display.print("HR: "); 
            if (beatAvg < 55 || beatAvg > 120) display.print("--"); 
            else display.print(beatAvg);
            
            display.setCursor(0, 24); display.print("O2: "); display.print(anim_spo2); display.print("%");
            display.setCursor(0, 48); display.print("T : "); display.print(anim_temp, 1); display.print("F");
            display.display();
            
            delay(15); 
        }
        
        anim_temp = tempF; anim_spo2 = current_spo2;

        display.clearDisplay(); display.setTextSize(2);
        
        display.setCursor(0, 0); display.print("HR: "); 
        if (beatAvg < 55 || beatAvg > 120) display.print("--"); 
        else display.print(beatAvg);
        
        display.setCursor(0, 24); display.print("O2: "); display.print(current_spo2); display.print("%");
        display.setCursor(0, 48); display.print("T : "); display.print(tempF, 1); display.print("F");
        display.display();

        ir_min = 999999; ir_max = 0; red_min = 999999; red_max = 0;
        lastScreenUpdate = millis();
      }

      if (millis() - lastFirebaseUpdate > 2000) {
        float tempF = mlx.readObjectTempF();
        String healthStatus = "Normal";

        if (beatAvg >= 55 && beatAvg <= 120 && tempF > 90.0) { 
            
            float input_array[3] = { (tempF - TEMP_MEAN)/TEMP_SCALE, (beatAvg - HR_MEAN)/HR_SCALE, (current_spo2 - SPO2_MEAN)/SPO2_SCALE };
            int prediction = ml.predict(input_array); 
            
            if (prediction == 0) healthStatus = "Healthy";
            else if (prediction == 1) healthStatus = "Fever";
            else if (prediction == 2) healthStatus = "Vital Warning";
            else if (prediction == 3) healthStatus = "CRITICAL";

            if (tempF > 100.5) healthStatus = "FEVER (High Temp)";
            if (current_spo2 < 90) healthStatus = "CRITICAL (Low O2)";

            String dbPath = "/Patients/" + PATIENT_UID + "/LiveVitals";
            Firebase.setFloat(firebaseData, dbPath + "/Temperature", tempF);
            Firebase.setInt(firebaseData, dbPath + "/HeartRate", beatAvg);
            Firebase.setInt(firebaseData, dbPath + "/SpO2", current_spo2);
            Firebase.setString(firebaseData, dbPath + "/Status", healthStatus);

            if (healthStatus != "Healthy") {
                abnormalCount++; 
                Serial.print("⚠️ Vital Alert Triggered! Status: "); Serial.print(healthStatus);
                Serial.print(" | Count: "); Serial.println(abnormalCount);
                
                if (abnormalCount >= 5) { 
                    if (!hasAlertedBefore || (millis() - lastAlertTime >= ALERT_COOLDOWN)) {
                        Serial.println("🚨 VERIFIED EMERGENCY! Sending Telegram...");
                        sendTelegramAlert(healthStatus, tempF, beatAvg, current_spo2);
                        lastAlertTime = millis();
                        hasAlertedBefore = true;
                    } else {
                        Serial.println("⏳ Emergency verified, but Telegram is on Cooldown timer (5 minutes).");
                    }
                }
            } else {
                if (abnormalCount > 0) {
                    Serial.println("✅ Patient stabilized. Resetting abnormal count to 0.");
                }
                abnormalCount = 0; 
            }
        }
        lastFirebaseUpdate = millis();
      }
      
    } else {
      fingerOn = false;
      beatAvg = 0; rollingAverage = 0; abnormalCount = 0; 
      ir_min = 999999; ir_max = 0; red_min = 999999; red_max = 0;
      
      anim_temp = 0.0; anim_spo2 = 0; 

      if (millis() - lastScreenUpdate > 1000) {
          display.clearDisplay(); display.setTextSize(2); display.setCursor(15, 10); display.print("Waiting");
          display.setTextSize(1); display.setCursor(10, 40); display.print("Place Finger");
          display.display();
          lastScreenUpdate = millis();
      }
    }
  }
}
  
// ==========================================
// TELEGRAM ALERT HTTP FUNCTION 
// ==========================================
void sendTelegramAlert(String status, float t, int hr, int o2) {
  WiFiClientSecure client;
  client.setInsecure(); 
  HTTPClient http;

  // ---> CHANGED: Title updated to remove the word "ABNORMAL" <---
  String msg = "⚠️ VITAL ALERT ⚠️\n\n";
  msg += "👤 Patient ID: " + PATIENT_UID + "\n";
  msg += "🩺 Status: " + status + "\n\n";
  msg += "🌡️ Temp: " + String(t) + " F\n";
  msg += "❤️ HR: " + String(hr) + " BPM\n";
  msg += "🩸 SpO2: " + String(o2) + " %";

  msg.replace(" ", "%20");
  msg.replace("\n", "%0A");

  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage?chat_id=" + String(CHAT_ID) + "&text=" + msg;

  Serial.println("--- TELEGRAM ATTEMPT START ---");
  
  http.begin(client, url);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    Serial.println("✅ SUCCESS! Telegram accepted the message!");
  } else {
    Serial.print("❌ FAILED! Telegram rejected it. Error Code: ");
    Serial.println(httpResponseCode);
    Serial.println("Telegram Reason: " + http.getString()); 
  }
  
  http.end();
  Serial.println("--- TELEGRAM ATTEMPT END ---");
}