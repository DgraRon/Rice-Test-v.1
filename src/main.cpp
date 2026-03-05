#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

Servo servo360;
Servo servo180_A;
Servo servo180_B;

void setup() {
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(2000);

  servo360.attach(27);
  servo180_A.attach(26);
  servo180_B.attach(25);
}

void loop() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Test Servo 180");
  
  lcd.setCursor(0, 1);
  lcd.print("Angle: 0");
  servo180_A.write(0);
  servo180_B.write(0);
  delay(1000);

  lcd.setCursor(0, 1);
  lcd.print("Angle: 90");
  servo180_A.write(90);
  servo180_B.write(90);
  delay(1000);

  lcd.setCursor(0, 1);
  lcd.print("Angle: 180");
  servo180_A.write(180);
  servo180_B.write(180);
  delay(1000);

  lcd.setCursor(0, 1);
  lcd.print("Angle: 0");
  servo180_A.write(0);
  servo180_B.write(0);
  delay(1000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Test Servo 360");
  
  lcd.setCursor(0, 1);
  lcd.print("Dir: Forward");
  servo360.write(100);
  delay(2000);
  
  lcd.setCursor(0, 1);
  lcd.print("Dir: Stop   ");
  servo360.write(90);
  delay(1000);
  
  lcd.setCursor(0, 1);
  lcd.print("Dir: Reverse");
  servo360.write(70);
  delay(2000);
  
  lcd.setCursor(0, 1);
  lcd.print("Dir: Stop   ");
  servo360.write(90);
  delay(2000);
}