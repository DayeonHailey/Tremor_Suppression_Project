#include <Arduino.h>
#include "config.h"
#include "imu_handler.h"
#include "dsp_filter.h"
#include "haptic_driver.h"

void setup() {
    Serial.begin(115200);
    delay(2000); 
    Serial.println("=== Tremor Suppression System Initialized ===");

    // 각 모듈의 초기화 함수만 호출 (구현은 각 .cpp 파일에 위치)
    // IMU_Init();
    while (!Serial); // PC와 시리얼 연결 대기

    initIMU(); // IMU 초기화
    // Filter_Init();
    // Haptic_Init();
}

void loop() {
    // 1. 센서 데이터 획득 (imu_handler)
    handleIMU();
    
    // 2. 신호 처리 (dsp_filter)
    // float filteredZ = Process_Filter(rawZ);
    // float tremorMagnitude = Process_FFT_or_LMS(filteredZ);

    // 3. 햅틱 피드백 제어 (haptic_driver)
    // Update_Haptic_Feedback(tremorMagnitude);
}