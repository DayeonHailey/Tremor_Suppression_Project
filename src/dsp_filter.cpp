#include "dsp_filter.h"
#include "config.h" // Sampling Rate 등의 상수가 필요할 경우
#include <math.h>

// --- Biquad 필터 클래스 ---
class BiquadFilter {
  private:
    float b0, b1, b2, a1, a2;
    float z1 = 0, z2 = 0;

  public:
    void setLPF(float fc, float fs) {
        float w0 = 2.0 * PI * fc / fs;
        float alpha = sin(w0) / (2.0 * 0.7071); 
        float a0 = 1.0 + alpha;
        b0 = ((1.0 - cos(w0)) / 2.0) / a0;
        b1 = (1.0 - cos(w0)) / a0;
        b2 = ((1.0 - cos(w0)) / 2.0) / a0;
        a1 = (-2.0 * cos(w0)) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setHPF(float fc, float fs) {
        float w0 = 2.0 * PI * fc / fs;
        float alpha = sin(w0) / (2.0 * 0.7071);
        float a0 = 1.0 + alpha;
        b0 = ((1.0 + cos(w0)) / 2.0) / a0;
        b1 = -(1.0 + cos(w0)) / a0;
        b2 = ((1.0 + cos(w0)) / 2.0) / a0;
        a1 = (-2.0 * cos(w0)) / a0;
        a2 = (1.0 - alpha) / a0;
    }
//Biquad 필터의 Direct Form II 구조로 구현, z1,z2(지연변수) 는 연산 중간 결과값 저장 --> 다음 사이클에 사용
    float process(float in) {
        float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return out;
    }
};

// 4차 필터 구현을 위한 2차 필터(Biquad) 직렬 구성
static BiquadFilter hpf1, hpf2, lpf1, lpf2;

// --- RMS 연산용 버퍼 (원형 큐 방식) ---
//RMS_WINDOW_SIZE 100Hz > 50, 400Hz > 200 으로 조정
const int RMS_WINDOW_SIZE = 50; // 100Hz 기준 50샘플 = 0.5초 동안의 떨림 크기 분석, 값이 커지면 떨림 측정은 매끄러워지고 피드백 느려짐, Test후 파라미터 조정.
float rms_buffer[RMS_WINDOW_SIZE] = {0};
int rms_idx = 0;

void Filter_Init() {
    float fs = 100.0; // Sampling Rate (Hz)
    
    // 4Hz HPF 2번 직렬 통과 (이 과정에서 중력 DC 성분이 완벽히 Detrend 됨)
    hpf1.setHPF(4.0, fs);
    hpf2.setHPF(4.0, fs);
    
    // 12Hz LPF 2번 직렬 통과 (고주파 노이즈 제거)
    lpf1.setLPF(12.0, fs);
    lpf2.setLPF(12.0, fs);
}

float Process_Tremor_Signal(IMU_Data imu_data) {
    // 1. 3축 벡터의 크기(Magnitude) 연산
    float raw_mag = sqrt((imu_data.x * imu_data.x) + 
                         (imu_data.y * imu_data.y) + 
                         (imu_data.z * imu_data.z));

    // 2. 4차 BPF 필터링 (Detrend + 대역 통과)
    float filtered = hpf1.process(raw_mag);
    filtered = hpf2.process(filtered);
    filtered = lpf1.process(filtered);
    filtered = lpf2.process(filtered);

    // 3. RMS 연산 (Root Mean Square)
    rms_buffer[rms_idx] = filtered * filtered; // 제곱값 저장
    rms_idx = (rms_idx + 1) % RMS_WINDOW_SIZE; // 버퍼 인덱스 순환

    float sum_squares = 0;
    for (int i = 0; i < RMS_WINDOW_SIZE; i++) {
        sum_squares += rms_buffer[i];
    }
    
    float rms_value = sqrt(sum_squares / RMS_WINDOW_SIZE); // 평균 후 루트

    return rms_value; // 최종 떨림 강도 반환!
}