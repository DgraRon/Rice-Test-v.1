#include <Arduino.h>
#include <Wire.h> 
#include <SPI.h>
#include <LiquidCrystal_I2C.h>

// ตั้งค่า Address ของ LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); 

void setup() {
  // เริ่มต้นการสื่อสาร I2C
  Wire.begin(21, 22); 
  
  lcd.init();
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("ESP32 LCD Test");
  lcd.setCursor(0, 1);
  lcd.print("SDA:21 SCL:22");
}

void loop() {
  // แสดงผลค้างไว้
}