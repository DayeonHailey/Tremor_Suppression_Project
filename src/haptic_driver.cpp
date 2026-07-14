#include "haptic_driver.h"
#include <Wire.h>
#include "config.h" // I2C 핀 번호나 기타 설정이 필요할 경우 
#include <Arduino.h>

// --- DRV2605L I2C 주소 및 주요 레지스터 맵 ---
#define DRV2605_ADDR        0x5A 
#define REG_MODE            0x01
#define REG_RTPIN           0x02
#define REG_LIBSEL          0x03
#define REG_RATEDV          0x16
//#define REG_LRARESON        0x22
#define REG_CONTROL3        0x1D
#define REG_ODCLAMP 0x17
#define REG_FEEDBACK 0x1A
#define REG_OLP   0x20

// --- 튜닝 파라미터 ---
const float TREMOR_THRESHOLD = 0.04;   
const float MAX_RMS_EXPECTED = 0.4;   
const unsigned long RTP_UPDATE_PERIOD = 100; // 진동 세기 유지/업데이트 주기 (100ms)

// 매크로 함수
#define CALC_RATED_VOLTAGE(vrms)  (uint8_t)((vrms) * 255.0 / 5.3)
#define CALC_LRA_FREQ(hz)         (uint8_t)(1.0 / ((hz) * 0.00009846))

// --- 내부 상태 관리 변수 ---
static bool is_standby = false; 
static int brake_counter = 0; // 
static unsigned long last_rtp_update_time = 0; // 마지막 진동 업데이트 시간

// --- I2C Register Write 헬퍼 함수 --- +리턴값 축사 버전
static uint8_t writeRegister8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(DRV2605_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission();
}

// 레지스터 읽기 함수 추가
static uint8_t readRegister8(uint8_t reg) {
    Wire.beginTransmission(DRV2605_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);  // repeated start
    Wire.requestFrom(DRV2605_ADDR, (uint8_t)1);
    return Wire.read();
}

void Haptic_Init() {

    pinMode(D1, OUTPUT);
    digitalWrite(D1, HIGH);  // DRV EN 핀 HIGH
    delay(10);
    
    Wire.begin(); // Custom PCB의 I2C 핀이 다를 경우 Wire.begin(SDA_PIN, SCL_PIN) 으로 수정 필요
    delay(10);    // IC 부팅 대기
    Serial.print("SDA pin: "); Serial.println(PIN_WIRE_SDA);
    Serial.print("SCL pin: "); Serial.println(PIN_WIRE_SCL);

        // === I2C 통신 진단 ===
    Wire.beginTransmission(DRV2605_ADDR);
    uint8_t ack = Wire.endTransmission();
    Serial.print("DRV ACK (0=OK): ");
    Serial.println(ack);

    uint8_t status = readRegister8(0x00);  // STATUS 레지스터
    Serial.print("STATUS reg (0x00): 0x");
    Serial.println(status, HEX);
    // 상위 3비트(DEVICE_ID)가 nonzero면 칩 살아있음. 0xFF나 0x00이면 통신 실패.

    // 1. Standby 모드 해제 및 초기화
    writeRegister8(REG_MODE, 0x00); 
    // 2. ★ 먼저 LRA 모드로 전환
    writeRegister8(REG_FEEDBACK, 0x80 | 0x06); 
    
    // 3. LRA 라이브러리(6) 선택
    writeRegister8(REG_LIBSEL, 0x06); 

    // 4. Open-loop LRA 설정
    writeRegister8(REG_CONTROL3, 0x01);  // LRA_OPEN_LOOP 활성화
    
    // 5. ★ 진폭 제한 (진동 세기) — 살려야 함
    writeRegister8(REG_ODCLAMP, 0x60);   // 14 = 0.43V, 20 = 0.68V, 30 = 1.0V, 60 = 1.5V, 73 = 1.8V

    // 5. ★ open-loop 구동 주파수 = 0x20 (170Hz)
    writeRegister8(REG_OLP, CALC_LRA_FREQ(170));  // ≈ 60
    
    // 6. RATED_VOLTAGE (open-loop에선 영향 적지만 무해)
    writeRegister8(REG_RATEDV, CALC_RATED_VOLTAGE(1.8));


    // 초기 상태: 제동(0) 및 Standby 진입
    writeRegister8(REG_MODE, 0x05); // RTP 모드 (0x05)
    writeRegister8(REG_RTPIN, 0x00); // 진동 0
    writeRegister8(REG_MODE, 0x45); // Standby 비트(bit 6) 설정으로 초저전력 모드 진입

    is_standby = true;

    // init 끝나고 핵심 레지스터 read-back 확인
    Serial.print("MODE(0x01): 0x");     Serial.println(readRegister8(0x01), HEX);
    Serial.print("FEEDBACK(0x1A): 0x"); Serial.println(readRegister8(0x1A), HEX);
    Serial.print("CONTROL3(0x1D): 0x"); Serial.println(readRegister8(0x1D), HEX);
    Serial.print("ODCLAMP(0x17): 0x");  Serial.println(readRegister8(0x17), HEX);
    Serial.print("OLP(0x20): 0x");      Serial.println(readRegister8(0x20), HEX);
}

//RTP Matching Logic (임시로 주석 처리, 추후 튜닝 후 복구 예정)
void Haptic_Set_Vibration(float rms_value) {
    unsigned long current_time = millis();

    if (rms_value >= TREMOR_THRESHOLD) {
        // [1] Standby 해제 (모터 깨우기)
        writeRegister8(REG_MODE, 0x05); // RTP 모드로 복귀
        is_standby = false;
    
        
        brake_counter = 0; // 브레이크 초기화

        // [2] 진동 세기 유지 시간 확인 (100ms마다만 업데이트)
        if (current_time - last_rtp_update_time >= RTP_UPDATE_PERIOD) {
            last_rtp_update_time = current_time;

            long rtp_value = map((long)(rms_value * 100), 
                                 (long)(TREMOR_THRESHOLD * 100), 
                                 (long)(MAX_RMS_EXPECTED * 100), 
                                 5, 100);
                                 
            if (rtp_value > 100) rtp_value = 100;
            if (rtp_value < 5) rtp_value = 5;

            uint8_t ack = writeRegister8(REG_RTPIN, (uint8_t)rtp_value);

            writeRegister8(REG_RTPIN, (uint8_t)rtp_value);
        }
    } 
    else {
        // [3] Active Brake 및 Standby 진입 로직
        if (!is_standby) {
            writeRegister8(REG_RTPIN, 0x00); // Active Brake 시작
            
            // main 루프가 10ms(100Hz)마다 호출된다고 가정할 때, 10번(100ms) 유지
            brake_counter++;
            
            if (brake_counter >= 10) {
                writeRegister8(REG_MODE, 0x40); // Standby 진입
                is_standby = true;
            }
        }
    }
}


/*void Haptic_Set_Vibration(float rms_value) {
    (void)rms_value;  // 인자 미사용 (스캔 모드)

    // --- 내부 상태 ---
    static uint8_t rtp_value = 0;               // 현재 RTP (0~127)
    static unsigned long last_step_time = 0;    // 마지막 증가 시각
    static bool scan_started = false;
    const unsigned long STEP_PERIOD = 2000;     // 2초마다 1씩 증가

    unsigned long now = millis();

    // 최초 진입 시 Standby 해제 + RTP 모드 진입 + 타이머 초기화
    if (!scan_started) {
        writeRegister8(REG_MODE, 0x00); delay(1);  // Standby 해제
        writeRegister8(REG_MODE, 0x05);            // RTP 모드
        is_standby = false;

        rtp_value = 0;
        writeRegister8(REG_RTPIN, rtp_value);
        last_step_time = now;
        scan_started = true;

        Serial.println("=== RTP scan start (0 -> 127, +1 / 2s) ===");
        Serial.print("RTP="); Serial.print(rtp_value);
        Serial.print("  ODCLAMP(0x17)=0x"); Serial.print(readRegister8(REG_ODCLAMP), HEX);
        Serial.print("  MODE(0x01)=0x");     Serial.println(readRegister8(REG_MODE), HEX);
        return;
    }

    // 2초 경과 시 RTP 1 증가
    if (now - last_step_time >= STEP_PERIOD) {
        last_step_time = now;

        if (rtp_value < 127) {
            rtp_value++;
        } else {
            // 127 도달 시: 여기서 멈추려면 아래 유지, 0부터 재시작하려면 rtp_value = 0;
            Serial.println("=== RTP reached 127 (hold) ===");
        }

        uint8_t ack = writeRegister8(REG_RTPIN, rtp_value);

        // 실시간 상태 출력
        Serial.print(" RTP=");     Serial.print(rtp_value);           
        Serial.print("  t(ms)="); Serial.print(now);
    }
}
    */

/*void Haptic_Set_Vibration(float rms_value) {
    // === 임시 테스트: 무조건 고정 진동 ===
    if (is_standby) {
        writeRegister8(REG_MODE, 0x00); delay(1);
        writeRegister8(REG_MODE, 0x05);
        is_standby = false;
    }

    writeRegister8(REG_RTPIN, 10);  // 고정값 100으로 강제 구동
    return;  // 아래 로직 전부 스킵
}
*/

