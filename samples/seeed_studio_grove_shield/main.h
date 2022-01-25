// Hardware definition
#include "hw/azure_sphere_learning_path.h"

// Learning Path Libraries
#include "azure_iot.h"
#include "exit_codes.h"
#include "peripheral_gpio.h"
#include "terminate.h"
#include "timer.h"
#include "config.h"

// System Libraries
#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <applibs/powermanagement.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

// Hardware specific
#ifdef OEM_AVNET
#include "AVNET/board.h"
#include "AVNET/imu_temp_pressure.h"
#include "AVNET/light_sensor.h"
#endif // OEM_AVNET

// Hardware specific
#ifdef OEM_SEEED_STUDIO
#include "SEEED_STUDIO/board.h"
#endif // SEEED_STUDIO


// Number of bytes to allocate for the JSON telemetry message for IoT Central
#define JSON_MESSAGE_BYTES 256  

// Forward signatures
static LP_DECLARE_TIMER_HANDLER(ReadSensorHandler);
static LP_DECLARE_DEVICE_TWIN_HANDLER(SampleRateHandler);

// Variables
static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };

LP_USER_CONFIG lp_config;

// Telemetry message template and properties
static const char* MsgTemplate = "{ \"Temperature\": \"%3.2f\", \"Humidity\": \"%3.1f\", \"Pressure\":\"%3.1f\", \"Light\":%d, \"MsgId\":%d }";

static LP_MESSAGE_PROPERTY messageAppId = { .key = "appid", .value = "hvac" };
static LP_MESSAGE_PROPERTY messageFormat = { .key = "format", .value = "json" };
static LP_MESSAGE_PROPERTY telemetryMessageType = { .key = "type", .value = "telemetry" };
static LP_MESSAGE_PROPERTY messageVersion = { .key = "version", .value = "1" };

static LP_MESSAGE_PROPERTY* telemetryMessageProperties[] = { &messageAppId, &telemetryMessageType, &messageFormat, &messageVersion };

// Timer
static LP_TIMER readSensorTimer = {
	.period = { 5, 0 },
	.name = "readSensorTimer",
	.handler = ReadSensorHandler
};

// Cloud to Device
static LP_DEVICE_TWIN_BINDING sampleRate_DeviceTwin = {
	.twinProperty = "SampleRateSeconds",
	.twinType = LP_TYPE_INT,
	.handler = SampleRateHandler
};

// Sets
static LP_TIMER* timerSet[] = { &readSensorTimer };
static LP_GPIO* gpioSet[] = { };
static LP_DEVICE_TWIN_BINDING* deviceTwinBindingSet[] = { &sampleRate_DeviceTwin };