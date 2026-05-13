#ifndef DSP_FILTER_H
#define DSP_FILTER_H

#include "imu_handler.h"

void Filter_Init();
float Process_Tremor_Signal(IMU_Data imu_data);

#endif