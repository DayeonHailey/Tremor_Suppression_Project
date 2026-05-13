#include "config.h" // 핀 번호나 I2C 주소가 필요할 경우
#include "imu_handler.h"
#include <Wire.h>
#include <LSM6DS3.h>

LSM6DS3 myIMU(I2C_MODE, 0x6A);

void initIMU() {
    if (myIMU.begin() != 0) {
        Serial.println("IMU error");
        while (1) delay(10); 
    }
}

// 타이밍 제어나 출력 없이, 호출될 때마다 센서값만 딱 읽어옵니다.
IMU_Data readIMU() {
    IMU_Data data;
    data.x = myIMU.readFloatAccelX();
    data.y = myIMU.readFloatAccelY();
    data.z = myIMU.readFloatAccelZ();
    return data;
}