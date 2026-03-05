#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>

// ================= PINOUT CONFIGURATION =================
// --- ตั้งค่า Address ของ PCF8574 ---
#define KEYPAD_I2C_ADDR 0x38
// I2C (LCD)
#define SDA_PIN 21
#define SCL_PIN 22
// SPI (SD Card)
#define SD_CS 5
// Servos
#define SERVO1_PIN 27 // M1: 360 Feed
#define SERVO2_PIN 26 // M2: 180 Sort (0-6)
#define SERVO3_PIN 25 // M2: 180 Gate
// LEDs
#define LED_RED 14
#define LED_GREEN 12
#define LED_YELLOW 13
// Sensor
#define SENSOR_PIN 34 
// Keypad 4x4
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'}, // A = Manual
  {'4','5','6','B'}, // B = Auto
  {'7','8','9','C'}, // C = Start
  {'*','0','#','D'}  // D = Stop
};
byte rowPins[ROWS] = {0, 1, 2, 3}; 
byte colPins[COLS] = {4, 5, 6, 7}; // ระวัง: Pin 0 และ 2 เป็น Strapping ห้ามกดปุ่มค้างตอนเปิดเครื่อง
// สร้างออบเจกต์ Keypad แบบ I2C
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYPAD_I2C_ADDR);

// ================= OBJECTS & VARS =================
AsyncWebServer server(80);
Preferences pref;
LiquidCrystal_I2C lcd(0x27, 16, 2); 
Servo servo1, servo2, servo3;

// State Machine
enum State { STANDBY, RUNNING, WAIT_MANUAL, SORTING, RELEASING };
State currentState = STANDBY;
bool isAuto = false; // true=Auto, false=Manual
bool isSystemRunning = false;
int currentGrade = -1;
unsigned long actionTimer = 0;
unsigned long lastLcdUpdate = 0;

// Stats
int gradeCounts[7] = {0};
int totalCount = 0;

// Sensor Calibration
int blankValue = 0;
int riceValue = 0;
int threshold = 0;

// Servo Angles
int s2Angles[7] = {0, 30, 60, 90, 120, 150, 180};
int s3Open = 90, s3Close = 0;

// Time & Logging
bool isTimeSynced = false;
bool needLogging = false; 

// ================= FUNCTIONS =================
void calculateThreshold() {
    threshold = (blankValue + riceValue) / 2;
}

void loadMemory() {
    pref.begin("rice_m4", false);
    for (int i=0; i<7; i++) {
        s2Angles[i] = pref.getInt(("s2_"+String(i)).c_str(), s2Angles[i]);
        gradeCounts[i] = pref.getInt(("g_"+String(i)).c_str(), 0);
        totalCount += gradeCounts[i];
    }
    s3Open = pref.getInt("s3_open", 90);
    s3Close = pref.getInt("s3_close", 0);
    blankValue = pref.getInt("blank", 0);
    riceValue = pref.getInt("rice", 100); // กันค่า 0
    calculateThreshold();
    pref.end();
}

void resetStats() {
    pref.begin("rice_m4", false);
    for (int i=0; i<7; i++) {
        gradeCounts[i] = 0;
        pref.putInt(("g_"+String(i)).c_str(), 0);
    }
    totalCount = 0;
    pref.end();
}

bool detectRice() {
    int val = analogRead(SENSOR_PIN);
    // ถ้าตั้งค่า riceValue ให้มากกว่า blank (เช่น สะท้อนแสงได้ดีกว่าเมื่อมีข้าว)
    if(riceValue >= blankValue) return val > threshold;
    else return val < threshold; // กรณีข้าวบังแสง
}

String getTimeString() {
    if (isTimeSynced) {
        struct tm timeinfo;
        if(getLocalTime(&timeinfo)){
            char buf[30];
            strftime(buf, sizeof(buf), "%d/%m/%Y, %H:%M:%S", &timeinfo);
            return String(buf);
        }
    }
    // ถ้ายังไม่ซิงค์ ใช้ Uptime
    unsigned long sec = millis() / 1000;
    return "Uptime, " + String(sec / 3600) + ":" + String((sec % 3600)/60) + ":" + String(sec % 60);
}

void logToSD(int grade) {
    if(!SD.begin(SD_CS)) return; // ข้ามถ้าไม่มี SD
    File file = SD.open("/rice_log.csv", FILE_APPEND);
    if(file) {
        String modeStr = isAuto ? "Auto" : "Manual";
        // Format: Time, Mode, Grade, Total
        file.println(getTimeString() + ", " + modeStr + ", " + String(grade) + ", " + String(totalCount));
        file.close();
    }
}

void updateLCD() {
    if(millis() - lastLcdUpdate < 500) return; // อัปเดตทุกครึ่งวิเพื่อกันจอกระพริบ
    lastLcdUpdate = millis();
    lcd.clear();
    
    // บรรทัด 1: สถานะ
    lcd.setCursor(0, 0);
    switch(currentState) {
        case STANDBY: lcd.print("Status: STANDBY "); break;
        case RUNNING: lcd.print("Status: RUNNING "); break;
        case WAIT_MANUAL: lcd.print("Input Grade 0-6:"); break;
        case SORTING: lcd.print("Sorting... G: "); lcd.print(currentGrade); break;
        case RELEASING: lcd.print("Releasing...    "); break;
    }

    // บรรทัด 2: โหมด และ ยอดรวม (ยกเว้นตอนรอรับค่า)
    if(currentState != WAIT_MANUAL && currentState != SORTING) {
        lcd.setCursor(0, 1);
        lcd.print(isAuto ? "M:Auto " : "M:Man  ");
        lcd.print("Tot:"); lcd.print(totalCount);
    }
}

void changeState(State newState) {
    currentState = newState;
    actionTimer = millis();
    updateLCD(); // บังคับอัปเดตจอทันทีที่เปลี่ยน State
}

void setSystemRunning(bool state) {
    isSystemRunning = state;
    if(state) {
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_GREEN, HIGH);
        servo1.write(180); // M1 หมุนตักข้าว
        changeState(RUNNING);
    } else {
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_YELLOW, LOW);
        servo1.write(90); // M1 หยุด
        servo3.write(s3Close);
        servo2.write(0);
        changeState(STANDBY);
    }
}

// ================= SETUP =================
void setup() {
    Serial.begin(115200);
    
    // เริ่มต้นระบบ I2C ที่พิน 21(SDA) และ 22(SCL)
    Wire.begin(SDA_PIN, SCL_PIN);
    
    // เริ่มการทำงานของ Keypad
    keypad.begin();

    // Init Hardware
    pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT); pinMode(LED_YELLOW, OUTPUT);
    servo1.attach(SERVO1_PIN); servo2.attach(SERVO2_PIN); servo3.attach(SERVO3_PIN);
    lcd.init(); lcd.backlight();
    
    loadMemory();
    setSystemRunning(false); // เริ่มที่ Standby
    
    // Init File System
    SPIFFS.begin(true);
    SD.begin(SD_CS);

    // WiFi Access Point (Fallback)
    WiFi.softAP("Rice_Sorter", "12345678"); // สร้างเน็ตตัวเองเสมอ เผื่อมือถือมาต่อ
    lcd.setCursor(0,0); lcd.print("WiFi Ready!");
    delay(1000);

    // Web API Endpoints
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){ req->send(SPIFFS, "/index.html", "text/html"); });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *req){ req->send(SPIFFS, "/style.css", "text/css"); });
    
    // API: รับเวลาจากมือถือ (Magic Time Sync)
    server.on("/api/sync_time", HTTP_GET, [](AsyncWebServerRequest *req){
        if (req->hasParam("t")) {
            time_t t = req->getParam("t")->value().toInt();
            struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
            settimeofday(&tv, NULL);
            isTimeSynced = true;
        }
        req->send(200, "text/plain", "OK");
    });

    // API: อัปเดตหน้าเว็บ Real-time
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
        DynamicJsonDocument doc(512);
        switch(currentState){
            case STANDBY: doc["state"] = "STANDBY"; break;
            case RUNNING: doc["state"] = "กำลังลำเลียง (RUNNING)"; break;
            case WAIT_MANUAL: doc["state"] = "รอจำลองเกรด (WAIT INPUT)"; break;
            case SORTING: doc["state"] = "กำลังคัดแยก (SORTING)"; break;
            case RELEASING: doc["state"] = "กำลังปล่อยข้าว (RELEASING)"; break;
        }
        doc["isAuto"] = isAuto;
        doc["timeStr"] = isTimeSynced ? "Synced (ตรงกับเวลาจริง)" : "Uptime (ยังไม่ซิงค์)";
        doc["total"] = totalCount;
        doc["threshold"] = threshold;
        JsonArray g = doc.createNestedArray("grades");
        for(int i=0; i<7; i++) g.add(gradeCounts[i]);
        
        String res; serializeJson(doc, res);
        req->send(200, "application/json", res);
    });

    // API: รับคำสั่ง
    server.on("/api/command", HTTP_GET, [](AsyncWebServerRequest *req){
        if (req->hasParam("action")) {
            String act = req->getParam("action")->value();
            if (act == "start") setSystemRunning(true);
            else if (act == "stop") setSystemRunning(false);
            else if (act == "reset_stats") resetStats();
            else if (act == "set_blank") {
                blankValue = analogRead(SENSOR_PIN);
                pref.begin("rice_m4", false); pref.putInt("blank", blankValue); pref.end();
                calculateThreshold();
            }
            else if (act == "set_rice") {
                riceValue = analogRead(SENSOR_PIN);
                pref.begin("rice_m4", false); pref.putInt("rice", riceValue); pref.end();
                calculateThreshold();
            }
            else if (act == "set_grade" && currentState == WAIT_MANUAL && req->hasParam("val")) {
                currentGrade = req->getParam("val")->value().toInt();
                changeState(SORTING);
            }
        }
        req->send(200, "text/plain", "OK");
    });

    server.on("/api/mode", HTTP_GET, [](AsyncWebServerRequest *req){
        if (req->hasParam("set")) isAuto = (req->getParam("set")->value() == "auto");
        req->send(200, "text/plain", "OK");
    });

    server.begin();
}

// ================= MAIN LOOP (Non-Blocking) =================
void loop() {
    // 1. ตรวจสอบการเขียน SD Card (ป้องกันเว็บค้าง)
    if(needLogging) {
        logToSD(currentGrade);
        // บันทึกสถิติลงหน่วยความจำ
        gradeCounts[currentGrade]++;
        totalCount++;
        pref.begin("rice_m4", false);
        pref.putInt(("g_"+String(currentGrade)).c_str(), gradeCounts[currentGrade]);
        pref.end();
        
        currentGrade = -1;
        needLogging = false;
        if(isSystemRunning) {
            servo1.write(180); // กลับไปลำเลียงต่อ
            changeState(RUNNING);
        }
    }

    // 2. จัดการ Keypad (Dual-Control)
    char key = keypad.getKey();
    if (key) {
        if (key == 'A') isAuto = false;
        else if (key == 'B') isAuto = true;
        else if (key == 'C') setSystemRunning(true);
        else if (key == 'D') setSystemRunning(false);
        // ถ้ากำลังรอค่าอยู่ และกดเลข 0-6
        else if (currentState == WAIT_MANUAL && key >= '0' && key <= '6') {
            currentGrade = key - '0';
            changeState(SORTING);
        }
    }

    // 3. State Machine ของ M1-M3
    if (!isSystemRunning) {
        updateLCD(); 
        return; 
    }

    switch (currentState) {
        case RUNNING:
            if (detectRice()) {
                servo1.write(90); // M1 หยุด
                if (isAuto) {
                    currentGrade = random(0, 7);
                    changeState(SORTING);
                } else {
                    digitalWrite(LED_GREEN, LOW);
                    digitalWrite(LED_YELLOW, HIGH);
                    changeState(WAIT_MANUAL);
                }
            }
            break;

        case WAIT_MANUAL:
            // รอรับค่าจาก Keypad หรือ Web (ถูกจัดการผ่าน Event ด้านบนแล้ว)
            break;

        case SORTING:
            digitalWrite(LED_YELLOW, LOW); digitalWrite(LED_GREEN, HIGH);
            servo2.write(s2Angles[currentGrade]); // หมุนท่อไปตามองศา
            if (millis() - actionTimer >= 500) {  // รอ 0.5 วิ
                changeState(RELEASING);
            }
            break;

        case RELEASING:
            // เปิดประตู M3 -> รอ 1 วิ -> ปิด
            if (millis() - actionTimer < 1000) {
                servo3.write(s3Open);
            } 
            else if (millis() - actionTimer < 1500) {
                servo3.write(s3Close);
            } 
            else {
                servo2.write(0); // Reset
                needLogging = true; // ฝาก Loop เขียน SD Card รอบถัดไป
            }
            break;
    }
    
    updateLCD();
}