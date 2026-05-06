#include "imu_handler.h"
#include "config.h" // 핀 번호나 I2C 주소가 필요할 경우
#include <Wire.h>
#include <LSM6DS3.h> // Seeed Studio IMU 라이브러리

// I2C 모드와 주소(0x6A)로 IMU 객체 생성
LSM6DS3 myIMU(I2C_MODE, 0x6A);

unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 2500; // 400Hz 샘플링 (2500us)

// IMU 초기화 함수 (main.cpp의 setup에서 호출됨)
void initIMU() {
    if (myIMU.begin() != 0) {
        Serial.println("IMU error");
        while (1); // 센서 연결 실패 시 시스템 정지
    }
}

// IMU 데이터 수집 및 전송 함수 (main.cpp의 loop에서 호출됨)
void handleIMU() {
    unsigned long currentMicros = micros();

    // 400Hz(2.5ms) 주기로 샘플링
    if (currentMicros - lastSampleTime >= sampleInterval) {
        lastSampleTime = currentMicros;

        float x = myIMU.readFloatAccelX();
        float y = myIMU.readFloatAccelY();
        float z = myIMU.readFloatAccelZ();

        // PC(Python GUI)로 전송할 Raw Data 포맷: timestamp,x,y,z
        Serial.print(millis());
        Serial.print(",");
        Serial.print(x, 4);
        Serial.print(",");
        Serial.print(y, 4);
        Serial.print(",");
        Serial.println(z, 4);
    }
}