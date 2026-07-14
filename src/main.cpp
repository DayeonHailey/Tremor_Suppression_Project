#include <Arduino.h>
#include "config.h"
#include "imu_handler.h"
#include "dsp_filter.h"
#include "haptic_driver.h"

unsigned long previous_micros = 0;
const unsigned long sample_period_us = 10000; // 100Hz = 10,000us, 400Hz = 2,500us

void setup() {
    // 전원 인가 표시: LED 3회 blink (XIAO nRF52840은 LED가 active-LOW)
    pinMode(LED_BUILTIN, OUTPUT);
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BUILTIN, LOW);  delay(150);  // 켜짐
        digitalWrite(LED_BUILTIN, HIGH); delay(150);  // 꺼짐
    }

    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000);
    delay(2000); 
    
    Serial.println("=== Tremor Suppression System Initialized ===");
    Serial.println("Timestamp(ms), Raw_X, Raw_Y, Raw_Z, Tremor_RMS");

    initIMU();    // IMU 모듈 초기화
    Filter_Init(); // DSP 필터 초기화
    Haptic_Init(); // 햅틱 초기화
    
    previous_micros = micros();
}

void loop() {
    unsigned long current_micros = micros();

    // 정확히 100Hz 주기로 동작 (Non-blocking 방식)
    if (current_micros - previous_micros >= sample_period_us) {
        // 타이머 오차 누적 방지를 위해 previous 값을 주기만큼 더함
        previous_micros += sample_period_us; 

        // 1. 센서 데이터 획득 (imu_handler)
        IMU_Data raw_data = readIMU();
        
        // 2. 신호 처리 (dsp_filter) -> Detrend + BPF + RMS
        float tremorMagnitude = Process_Tremor_Signal(raw_data);

        // 3. PC(Python GUI 등) 전송을 위한 데이터 출력
        Serial.print(millis());
        Serial.print(",");
        Serial.print(raw_data.x, 4);
        Serial.print(",");
        Serial.print(raw_data.y, 4);
        Serial.print(",");
        Serial.print(raw_data.z, 4);
        Serial.print(",");
        Serial.println(tremorMagnitude, 4);
        

        // 4. 햅틱 피드백 제어 (추후 구현)
        Haptic_Set_Vibration(tremorMagnitude);
    }
}   

//-----------------------
/*#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(1000);


  pinMode(D1, OUTPUT);
  digitalWrite(D1, HIGH);   // DRV EN — 스캔 전에 반드시 깨워둠
  delay(10);

  Wire.begin();
  Wire.setClock(100000);    // 100kHz 명시
  delay(50);

  Serial.println("Scanning...");

  Serial.print("D1 read-back: ");
  Serial.println(digitalRead(D1));  // HIGH면 1, 아니면 0
}

void loop() {
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found 0x"); Serial.println(addr, HEX);
      found++;
    }
  }
  if (!found) Serial.println("No I2C devices.");
  delay(2000);
}
*/