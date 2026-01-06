#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include "SPIFFS.h"
#include <AsyncWebSocket.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

// WiFi AP Mode Config
const char* ssid = "ESP32_AP";
const char* password = "12345678";

// กำหนดขา LED และ Servo
#define LED_RED     4   // LED แดง: ESP32 เปิดใช้งาน AP Mode
#define LED_YELLOW  2   // LED เหลือง: มีอุปกรณ์เชื่อมต่อกับ AP
#define LED_GREEN   15  // LED เขียว: Servo หมุน
#define SERVO_PIN   16  // ขาเชื่อม Servo

// เซ็นเซอร์สี TCS34725
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
uint16_t r, g, b, c;
float colorTemp, lux;

// ตัวแปรควบคุมระบบ
Servo myServo;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long previousMillis = 0;
const long interval = 1000;

enum Mode {
  MANUAL,
  RANDOM
};

Mode currentMode = MANUAL;
bool isRunning = false;
unsigned long startTime = 0;
int totalCount = 0;
int gradeCounts[7] = {0}; // เก็บจำนวนแต่ละเกรด (0-6)

// เพิ่มส่วนนี้หลังจากประกาศ Mode
const byte ROWS = 4;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1','2','3',' '},
  {'4','5','6',' '},
  {'7','8','9',' '},
  {' ','0',' ',' '}
};
byte rowPins[ROWS] = {32, 33, 25, 26}; 
byte colPins[COLS] = {27, 14, 12, 13}; 
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// ฟังก์ชันตรวจสอบสีขาว
bool isWhiteColor() {
  tcs.getRawData(&r, &g, &b, &c);
  colorTemp = tcs.calculateColorTemperature_dn40(r, g, b, c);
  lux = tcs.calculateLux(r, g, b);
  
  // ปรับค่าเกณฑ์ตามการทดสอบจริง
  if (lux > 100 && r > 1000 && g > 1000 && b > 1000) {
    return true;
  }
  return false;
}

// ฟังก์ชันส่งข้อมูลผ่าน WebSocket
void notifyClients(String data) {
  ws.textAll(data);
}

// ฟังก์ชันหมุน servo 1 รอบ
void rotateServoOneTurn() {
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_YELLOW, LOW);
  myServo.write(0);
  delay(1000);
  myServo.write(90);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, HIGH);
}

// ฟังก์ชันอัปเดตข้อมูลเกรด
void updateGradeCount(int grade) {
  if (grade >= 0 && grade <= 6) {
    gradeCounts[grade]++;
    totalCount++;
    
    String updateData = "{\"type\":\"gradeUpdate\",\"grade\":" + String(grade) + 
                       ",\"count\":" + String(gradeCounts[grade]) + 
                       ",\"total\":" + String(totalCount) + "}";
    
    notifyClients(updateData);
    
    // ตรวจสอบสีและส่งการแจ้งเตือน
    if (isWhiteColor()) {
      String notification = "{\"type\":\"colorDetection\",\"message\":\"ตรวจพบสีขาว\"}";
      notifyClients(notification);
    }
  }
}

void setupEndpoints() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/setMode", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode")) {
      String mode = request->getParam("mode")->value();
      if (mode == "manual") {
        currentMode = MANUAL;
      } else if (mode == "random") {
        currentMode = RANDOM;
      }
      request->send(200, "text/plain", "Mode set to: " + mode);
    }
  });

  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    isRunning = true;
    startTime = millis();
    request->send(200, "text/plain", "Started");
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
    isRunning = false;
    request->send(200, "text/plain", "Stopped");
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    isRunning = false;
    startTime = 0;
    totalCount = 0;
    for(int i = 0; i < 7; i++) {
      gradeCounts[i] = 0;
    }
    myServo.write(90);
    request->send(200, "text/plain", "Reset");
  });
}

// แก้ไขฟังก์ชัน updateLCDDisplay()
void updateLCDDisplay() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    // คำนวณเวลาทำงานเหมือนในเว็บ
    unsigned long runTime = 0;
    if (isRunning && startTime > 0) {
      runTime = currentMillis - startTime;
      unsigned long hours = runTime / 3600000;
      unsigned long minutes = (runTime % 3600000) / 60000;
      unsigned long seconds = (runTime % 60000) / 1000;
      
      // แสดงเวลาบรรทัดบน
      lcd.setCursor(0, 0);
      lcd.print("Time: ");
      if (hours < 10) lcd.print("0");
      lcd.print(hours);
      lcd.print(":");
      if (minutes < 10) lcd.print("0");
      lcd.print(minutes);
      lcd.print(":");
      if (seconds < 10) lcd.print("0");
      lcd.print(seconds);
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Time: 00:00:00");
    }
    
    // แสดงจำนวนข้าวบรรทัดล่าง
    lcd.setCursor(0, 1);
    lcd.print("Sorted: ");
    
    // เว้นที่สำหรับตัวเลขและหน่วย
    String countStr = String(totalCount) + " PCs";
    lcd.print(countStr);
    
    // เคลียร์ส่วนที่เหลือของบรรทัดด้วยช่องว่าง
    for (int i = 8 + countStr.length(); i < 16; i++) {
        lcd.print(" ");
    }
  }
}

void setupLCD() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Rice Sorter");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);
  lcd.clear();
}

void Scanner ()
{
  Serial.println ();
  Serial.println ("I2C scanner. Scanning ...");
  byte count = 0;
  Wire.begin();
  for (byte i = 8; i < 120; i++)
  {
    Wire.beginTransmission (i); //Begin I2C transmission Address (i)
    if (Wire.endTransmission () == 0) // 0 = success(ACK response) 
    {
      Serial.print ("Found address: ");
      Serial.print (i, DEC);
      Serial.print (" (0x");
      Serial.print (i, HEX);     
      Serial.println (")");
      count++;
    }
  }
  Serial.print ("Found ");      
  Serial.print (count, DEC);        // numbers of devices
  Serial.println (" device(s).");
}



void setup() {
  Serial.begin(115200);
  Serial.println(WiFi.softAPgetStationNum() );

  
  // กำหนดขา LED เป็น OUTPUT
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, LOW);

  Wire.begin (21, 22);

  setupLCD();  

  // เริ่มต้นเซ็นเซอร์สี
 if (!tcs.begin()) {
   Serial.println("Error: TCS34725 not found");
   while (1);
 }
 


  // ตั้งค่า WiFi AP Mode
  WiFi.softAP(ssid, password);
  Serial.println("AP Mode Started!");
  Serial.print("ESP32 AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // ตั้งค่า Servo
  myServo.attach(SERVO_PIN);
  myServo.write(90);

  // ตั้งค่า SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  // ตั้งค่า WebSocket
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    // Handle WebSocket events if needed
  });
  server.addHandler(&ws);

  setupEndpoints();
  server.begin();

}

void loop() {
  // ตรวจสอบการเชื่อมต่อ WiFi
  if (WiFi.softAPgetStationNum() > 0) {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, HIGH);
  } else {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_YELLOW, LOW);
  }

  if (isRunning) {
    if (currentMode == MANUAL) {
      char customKey = customKeypad.getKey();
      if (customKey && customKey != ' ') {
        int grade = customKey - '0'; // แปลงจาก char เป็น int
        if (grade >= 0 && grade <= 6) {
          updateGradeCount(grade);
          rotateServoOneTurn();
          delay(250);
        }
      }
    } 
    else if (currentMode == RANDOM) {
      int grade = random(0, 7);
      updateGradeCount(grade);
      rotateServoOneTurn();
      delay(1000);
    }
  }

  // float red, green, blue;
  // tcs.setInterrupt(false);  // turn on LED
  // delay(500);  // takes 50ms to read
  // tcs.getRGB(&red, &green, &blue);
  // tcs.setInterrupt(true);  // turn off LED
  // Serial.print("R:\t"); Serial.print(int(red));
  // Serial.print("\tG:\t"); Serial.print(int(green)); 
  // Serial.print("\tB:\t"); Serial.print(int(blue));
  // Serial.print("\n");
  // delay(1000);
  // tcs.setInterrupt(true);
  // if(red < 137 && green > 78 && blue < 57 ){
  //   Serial.print("ยินดีด้วย คุณพบข้าวแล้ว");
  // }

 updateLCDDisplay();
}
