// Hardware definition
#include "hw/azure_sphere_learning_path.h"

// Learning Path Libraries
#include "azure_iot.h"
#include "config.h"
#include "exit_codes.h"
#include "inter_core.h"
#include "peripheral_gpio.h"
#include "terminate.h"
#include "timer.h"

// System Libraries
#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <applibs/powermanagement.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define LP_LOGGING_ENABLED FALSE
#define JSON_MESSAGE_BYTES 256  // Number of bytes to allocate for the JSON telemetry message for IoT Central
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:com:example:azuresphere:labmonitor;1"	// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
#define REAL_TIME_COMPONENT_ID "6583cf17-d321-4d72-8283-0b7c5b56442b"


// Forward signatures
static LP_DECLARE_DEVICE_TWIN_HANDLER(DeviceTwinSetSampleRateHandler);
static LP_DECLARE_DEVICE_TWIN_HANDLER(DeviceTwinSetTemperatureHandler);
static LP_DECLARE_DIRECT_METHOD_HANDLER(RestartDeviceHandler);
static LP_DECLARE_TIMER_HANDLER(AzureIoTConnectionStatusHandler);
static LP_DECLARE_TIMER_HANDLER(DelayRestartDeviceTimerHandler);
static LP_DECLARE_TIMER_HANDLER(MeasureSensorHandler);
static void InterCoreHandler(LP_INTER_CORE_BLOCK* ic_message_block);


LP_USER_CONFIG lp_config;
LP_INTER_CORE_BLOCK ic_control_block;

static int previous_temperature = 999999;

enum LEDS { RED, GREEN, BLUE, UNKNOWN };
static enum LEDS current_led = RED;
static const char* hvacState[] = { "heating", "off", "cooling" };

static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };

// Declare GPIO
static LP_GPIO azureIotConnectedLed = {
	.pin = NETWORK_CONNECTED_LED,
	.direction = LP_OUTPUT,
	.initialState = GPIO_Value_Low,
	.invertPin = true,
	.name = "azureIotConnectedLed" };

static LP_GPIO* ledRgb[] = {
	&(LP_GPIO) { .pin = LED_RED, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true,.name = "red led" },
	&(LP_GPIO) {.pin = LED_GREEN, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "green led" },
	&(LP_GPIO) {.pin = LED_BLUE, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "blue led" }
};

// Timers
static LP_TIMER azureIotConnectionStatusTimer = {
	.period = { 5, 0 },
	.name = "azureIotConnectionStatusTimer",
	.handler = AzureIoTConnectionStatusHandler };

static LP_TIMER measureSensorTimer = {
	.period = { 6, 0 },
	.name = "measureSensorTimer",
	.handler = MeasureSensorHandler };

static LP_TIMER restartDeviceOneShotTimer = {
	.period = {0, 0},
	.name = "restartDeviceOneShotTimer",
	.handler = DelayRestartDeviceTimerHandler };

// Azure IoT Device Twins
static LP_DEVICE_TWIN_BINDING dt_desiredTemperature = {
	.twinProperty = "DesiredTemperature",
	.twinType = LP_TYPE_FLOAT,
	.handler = DeviceTwinSetTemperatureHandler };

static LP_DEVICE_TWIN_BINDING dt_desiredSampleRateInSeconds = {
	.twinProperty = "DesiredSampleRateInSeconds",
	.twinType = LP_TYPE_INT,
	.handler = DeviceTwinSetSampleRateHandler };

static LP_DEVICE_TWIN_BINDING dt_reportedTemperature = {
	.twinProperty = "ReportedTemperature",
	.twinType = LP_TYPE_FLOAT };

static LP_DEVICE_TWIN_BINDING dt_reportedHvacState = {
	.twinProperty = "ReportedHvacState",
	.twinType = LP_TYPE_STRING };

static LP_DEVICE_TWIN_BINDING dt_reportedDeviceStartTime = {
	.twinProperty = "ReportedDeviceStartTime",
	.twinType = LP_TYPE_STRING };

static LP_DEVICE_TWIN_BINDING dt_reportedRestartUtc = {
	.twinProperty = "ReportedRestartUTC",
	.twinType = LP_TYPE_STRING };

// Azure IoT Direct Methods
static LP_DIRECT_METHOD_BINDING dm_restartDevice = {
	.methodName = "RestartDevice",
	.handler = RestartDeviceHandler };

// Initialize Sets
LP_GPIO* gpioSet[] = { &azureIotConnectedLed };
LP_TIMER* timerSet[] = { &azureIotConnectionStatusTimer, &measureSensorTimer, &restartDeviceOneShotTimer };
LP_DEVICE_TWIN_BINDING* deviceTwinBindingSet[] = {
	&dt_desiredTemperature, &dt_reportedTemperature, &dt_reportedHvacState,
	&dt_reportedDeviceStartTime, &dt_desiredSampleRateInSeconds, &dt_reportedRestartUtc
};
LP_DIRECT_METHOD_BINDING* directMethodBindingSet[] = { &dm_restartDevice };

// Telemetry message template and properties
static const char* msgTemplate = "{ \"Temperature\":%3.2f, \"Humidity\":%3.1f, \"Pressure\":%3.1f, \"MsgId\":%d }";

static LP_MESSAGE_PROPERTY* telemetryMessageProperties[] = {
	&(LP_MESSAGE_PROPERTY) { .key = "appid", .value = "hvac" },
	&(LP_MESSAGE_PROPERTY) {.key = "format", .value = "json" },
	&(LP_MESSAGE_PROPERTY) {.key = "type", .value = "telemetry" },
	&(LP_MESSAGE_PROPERTY) {.key = "version", .value = "1" }
};