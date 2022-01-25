#pragma once

#include "dx_device_twins.h"
#include "dx_gpio.h"
#include "hw/azure_sphere_learning_path.h"

extern DX_DEVICE_TWIN_BINDING desiredTemperature;

void SetHvacStatusColour(int32_t temperature);