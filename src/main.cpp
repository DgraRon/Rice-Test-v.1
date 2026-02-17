#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "Adafruit_TCS34725.h"

// --- กำหนดขาต่ออุปกรณ์ ---
// SD Card Module
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 5
#define SD_CS   17

// TCS34725 Sensor (I2C)
#define I2C_SDA 21
#define I2C_SCL 22

// ตั้งค่า Sensor (Integration Time ยิ่งนานยิ่งแม่นยำ แต่กินเวลา)
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_154MS, TCS34725_GAIN_1X);

// ตัวแปรสำหรับนับเวลา
unsigned long previousMillis = 0;
const long interval = 30000; // 30 วินาที (30,000 ms)

void setup() {
  Serial.begin(115200);

  // 1. เริ่มต้น I2C สำหรับ Sensor
  Wire.begin(I2C_SDA, I2C_SCL);

  // 2. ตรวจสอบ Sensor
  if (tcs.begin()) {
    Serial.println("Found sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }

  // 3. เริ่มต้น SPI สำหรับ SD Card ตามขาที่กำหนดเอง
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // 4. ตรวจสอบ SD Card
  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("SD Card initialized.");

  // เขียน Header ของไฟล์ CSV (ถ้าไฟล์ยังไม่มี)
  if (!SD.exists("/color_log.csv")) {
    File file = SD.open("/color_log.csv", FILE_WRITE);
    if (file) {
      file.println("Time_ms,Red,Green,Blue,Clear,Lux,ColorTemp"); // หัวตาราง
      file.close();
    }
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // ตรวจสอบว่าผ่านไป 30 วินาทีหรือยัง
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

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
    File file = SD.open("/color_log.csv", FILE_APPEND); // เปิดแบบต่อท้าย (Append)
    if (file) {
      file.println(dataString);
      file.close();
      Serial.println("Data saved: " + dataString);
    } else {
      Serial.println("Error opening file for writing");
    }
  }
}