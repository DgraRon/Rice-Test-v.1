#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "Adafruit_TCS34725.h"
#include <LiquidCrystal_I2C.h> // เพิ่ม Library สำหรับจอ LCD I2C

// --- กำหนดขาต่ออุปกรณ์ ---
// SD Card Module
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 5
#define SD_CS   17

// TCS34725 Sensor & LCD (I2C ใช้ขาร่วมกันได้เลย)
#define I2C_SDA 21
#define I2C_SCL 22

// ตั้งค่า Sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_154MS, TCS34725_GAIN_1X);

// ตั้งค่าจอ LCD (ที่อยู่ I2C ปกติคือ 0x27, ขนาด 16 ตัวอักษร 2 บรรทัด)
// หมายเหตุ: ถ้าจอเปิดแล้วตัวหนังสือไม่ขึ้น ให้ลองเปลี่ยน 0x27 เป็น 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ตัวแปรสำหรับนับเวลา
unsigned long previousMillis = 0;
const long interval = 30000; // 30 วินาที (30,000 ms)

void setup() {
  Serial.begin(115200);

  // 1. เริ่มต้น I2C สำหรับ Sensor และ LCD
  Wire.begin(I2C_SDA, I2C_SCL);

  // เริ่มต้นจอ LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Starting");
  delay(1000);
  lcd.clear();

  // 2. ตรวจสอบ Sensor
  lcd.setCursor(0, 0);
  if (tcs.begin()) {
    Serial.println("Found sensor");
    lcd.print("TCS: OK ");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    lcd.print("TCS: ERROR");
    while (1); // หยุดการทำงานถ้าไม่เจอเซนเซอร์
  }

  // 3. เริ่มต้น SPI สำหรับ SD Card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // 4. ตรวจสอบ SD Card
  lcd.setCursor(0, 1);
  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    lcd.print("SD : ERROR");
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    lcd.print("SD : NO CARD");
    return;
  }
  Serial.println("SD Card initialized.");
  lcd.print("SD : OK   ");

  delay(2000); // แสดงสถานะ OK ค้างไว้ 2 วินาที
  
  // เขียน Header ของไฟล์ CSV (ถ้าไฟล์ยังไม่มี)
  if (!SD.exists("/color_log.csv")) {
    File file = SD.open("/color_log.csv", FILE_WRITE);
    if (file) {
      file.println("Time_ms,Red,Green,Blue,Clear,Lux,ColorTemp");
      file.close();
    }
  }

  // เปลี่ยนจอเป็นสถานะรอ
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Status: Waiting");
}

void loop() {
  unsigned long currentMillis = millis();

  // ตรวจสอบว่าผ่านไป 30 วินาทีหรือยัง
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // เปลี่ยนจอเพื่อบอกว่ากำลังอ่านและบันทึก
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Reading Data...");

    // อ่านค่าสี
    uint16_t r, g, b, c, colorTemp, lux;
    tcs.getRawData(&r, &g, &b, &c);
    colorTemp = tcs.calculateColorTemperature(r, g, b);
    lux = tcs.calculateLux(r, g, b);

    // สร้าง String ข้อมูลที่จะบันทึก
    String dataString = String(currentMillis) + "," +
                        String(r) + "," +
                        String(g) + "," +
                        String(b) + "," +
                        String(c) + "," +
                        String(lux) + "," +
                        String(colorTemp);

    // บันทึกลง SD Card
    File file = SD.open("/color_log.csv", FILE_APPEND);
    
    lcd.setCursor(0, 1);
    if (file) {
      file.println(dataString);
      file.close();
      Serial.println("Data saved: " + dataString);
      lcd.print("Save: SUCCESS");
    } else {
      Serial.println("Error opening file for writing");
      lcd.print("Save: FAILED!");
    }

    delay(2000); // โชว์สถานะการบันทึกค้างไว้ 2 วินาที
    
    // กลับไปหน้าจอรอเวลาตามปกติ
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Status: Waiting");
    lcd.setCursor(0, 1);
    // พิมพ์ค่าสีที่เพิ่งอ่านได้คร่าวๆ ให้ดูบนจอด้วย
    lcd.print("R:"); lcd.print(r);
    lcd.print(" G:"); lcd.print(g);
    lcd.print(" B:"); lcd.print(b);
  }
}