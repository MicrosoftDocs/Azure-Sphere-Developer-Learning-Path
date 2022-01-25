#include "hw/azure_sphere_learning_path.h"

#include "azure_iot.h"
#include "exit_codes.h"
#include "config.h"
#include "inter_core.h"
#include "peripheral_gpio.h"
#include "terminate.h"
#include "timer.h"

#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <applibs/powermanagement.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "./embedded/sht31/sht3x.h"

#define JSON_MESSAGE_BYTES 256 // Number of bytes to allocate for the JSON telemetry message for IoT Central

 // Forward signatures
static void MeasureSensorHandler(EventLoopTimer* eventLoopTimer);

static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };
float temperature, humidity;

// Timers
static LP_TIMER measureSensorTimer = { .period = {4, 0}, .name = "measureSensorTimer", .handler = MeasureSensorHandler };

// Initialize Sets
LP_TIMER* timerSet[] = { &measureSensorTimer };

static const char* MsgTemplate = "{ \"Temperature\": %3.2f, \"Humidity\": \"%3.1f\", \"MsgId\":%d }";