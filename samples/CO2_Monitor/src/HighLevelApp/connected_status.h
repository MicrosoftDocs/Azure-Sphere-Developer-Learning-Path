#pragma once

#include "dx_device_twins.h"
#include "dx_gpio.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "eventloop_timer_utilities.h"
#include "hw/azure_sphere_learning_path.h"
#include "location_from_ip.h"
#include <stdbool.h>

#define OneMS 1000000		// used to simplify timer defn.

extern DX_DEVICE_TWIN_BINDING reportedCountryCode;
extern DX_DEVICE_TWIN_BINDING reportedLatitude;
extern DX_DEVICE_TWIN_BINDING reportedLongitude;
extern struct location_info* locInfo;

void ConnectedStatusInitialise(void);
