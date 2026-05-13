#include <Arduino.h>

#ifndef IMU_HANDLER_H
#define IMU_HANDLER_H

// x, y, z 데이터를 한 번에 묶어서 전달하기 위한 구조체
struct IMU_Data {
    float x;
    float y;
    float z;
};

void initIMU();
IMU_Data readIMU(); // 기존 handleIMU()를 대체

#endif