#include <Arduino.h>
#include <ESP32Servo.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_TCS34725.h>
#include <sd.h>
#include <SPI.h>
#include <FS.h>

#define SD_CARD_PIN 17
#define SERVO360_PIN 16
#define SERVO180x1_PIN 13
#define SERVO180x2_PIN 12
#define LED_RED_PIN 4
#define LED_GREEN_PIN 2
#define LED_YELLOW_PIN 15

Servo servo360;
Servo servo180x1;
Servo servo180x2;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// กำหนดจำนวนแถว (Rows) และคอลัมน์ (Columns)
const byte ROWS = 4; 
const byte COLS = 4; 

// กำหนดตัวอักษรบนปุ่ม
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {' ',' ',' ','C'},
  {' ','0',' ','D'}
};
// ระบุขา GPIO ที่เชื่อมต่อกับ Row และ Column
byte rowPins[ROWS] = {13, 12, 14, 27}; 
byte colPins[COLS] = {26, 25, 33, 32};

// สร้าง Instance ของ Keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(115200);

  digitalWrite(SD_CARD_PIN, OUTPUT);
  digitalWrite(SERVO360_PIN, OUTPUT);
  digitalWrite(SERVO180x1_PIN, OUTPUT);
  digitalWrite(SERVO180x2_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, OUTPUT);
  digitalWrite(LED_GREEN_PIN, OUTPUT);
  digitalWrite(LED_YELLOW_PIN, OUTPUT);

  Wire.begin(21, 22);
  lcd.backlight();
  
  servo360.attach(SERVO360_PIN);
  servo180x1.attach(SERVO180x1_PIN);
  servo180x2.attach(SERVO180x2_PIN);

  
  
  
  if (!tcs.begin()) {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }
  
  if (!SD.begin(SD_CARD_PIN)) {
    Serial.println("Card Mount Failed");
    return;
  }
  
}

void loop() {
  // Main loop code here
}