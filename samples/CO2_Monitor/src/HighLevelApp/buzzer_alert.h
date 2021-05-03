#pragma once

#include "eventloop_timer_utilities.h"
#include "hw/azure_sphere_learning_path.h"
#include "dx_terminate.h"
#include "dx_gpio.h"
#include "dx_timer.h"
#include "dx_device_twins.h"

extern DX_DEVICE_TWIN_BINDING desiredCO2AlertLevel;
extern float co2_ppm;


void CO2AlertBuzzerInitialize(void);