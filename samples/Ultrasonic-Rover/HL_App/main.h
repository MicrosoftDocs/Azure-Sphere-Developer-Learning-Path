#include "hw/azure_sphere_learning_path.h"

#include "azure_iot.h"
#include "exit_codes.h"
#include "inter_core.h"
#include "peripheral_gpio.h"
#include "terminate.h"
#include "timer.h"
#include "config.h"

#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <applibs/powermanagement.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>


#define JSON_MESSAGE_BYTES 128  // Number of bytes to allocate for the JSON telemetry message for IoT Central
#define REAL_TIME_COMPONENT_ID "6583cf17-d321-4d72-8283-0b7c5b56442b"

 // Forward signatures
static void InitPeripheralGpiosAndHandlers(void);
static void ClosePeripheralGpiosAndHandlers(void);
static void IntercoreHeartBeatHandler(EventLoopTimer* eventLoopTimer);
static void InterCoreHandler(LP_INTER_CORE_BLOCK* ic_message_block);

static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };
LP_INTER_CORE_BLOCK ic_control_block;
LP_USER_CONFIG lp_config;

// Timers
static LP_TIMER intercoreHeartBeatTimer = { .period = { 5, 0 }, .name = "intercoreHeartBeatTimer", .handler = IntercoreHeartBeatHandler };

// Azure IoT Device Twins
static LP_DEVICE_TWIN_BINDING reportDistanceLeft = { .twinProperty = "ReportDistanceLeft", .twinType = LP_TYPE_INT };
static LP_DEVICE_TWIN_BINDING reportDistanceRight = { .twinProperty = "ReportDistanceRight", .twinType = LP_TYPE_INT };

// Initialize Sets
LP_TIMER* timerSet[] = { &intercoreHeartBeatTimer };
LP_DEVICE_TWIN_BINDING* deviceTwinBindingSet[] = { &reportDistanceLeft, &reportDistanceRight };

// Message property set
static LP_MESSAGE_PROPERTY messageLog = { .key = "log", .value = "false" };
static LP_MESSAGE_PROPERTY messageAppId = { .key = "appid", .value = "rover" };
static LP_MESSAGE_PROPERTY* messageProperties[] = { &messageLog, &messageAppId };
