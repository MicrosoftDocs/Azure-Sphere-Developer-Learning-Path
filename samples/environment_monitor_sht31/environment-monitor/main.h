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
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:com:example:azuresphere:labmonitor;1"	// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play

 // Forward signatures
static void MeasureSensorHandler(EventLoopTimer* eventLoopTimer);
static void NetworkConnectionStatusHandler(EventLoopTimer* eventLoopTimer);
static void TemperatureAlertHandler(EventLoopTimer* eventLoopTimer);
static void TemperatureAlertBuzzerOffOneShotTimer(EventLoopTimer* eventLoopTimer);
static void DeviceTwinGenericHandler(LP_DEVICE_TWIN_BINDING* deviceTwinBinding);

LP_USER_CONFIG lp_config;

static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };

enum HVAC { HEATING, COOLING, OFF };
static char* hvacState[] = { "Heating", "Cooling", "Off" };

static enum HVAC current_hvac_state = OFF;
static float temperature, humidity;
static int previous_temperature = 0;
static const struct timespec co2AlertBuzzerPeriod = { 0, 5 * 100 * 1000 };

// GPIO Output PeripheralGpios
#ifdef OEM_SEEED_STUDIO
static LP_GPIO hvacHeatingLed = { .pin = LED_RED, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low,  .name = "hvacHeatingLed" };
static LP_GPIO hvacCoolingLed = { .pin = LED_BLUE, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low,  .name = "hvacCoolingLed" };
static LP_GPIO co2AlertPin = { .pin = CO2_ALERT, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low,  .name = "co2AlertPin" };
#endif // OEM_AVNET

#ifdef OEM_AVNET
static LP_GPIO hvacHeatingLed = { .pin = LED_RED, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true,  .name = "hvacHeatingLed" };
static LP_GPIO hvacCoolingLed = { .pin = LED_BLUE, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true,  .name = "hvacCoolingLed" };
static LP_GPIO co2AlertPin = { .pin = CO2_ALERT, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true,  .name = "co2AlertPin" };
#endif // OEM_AVNET

static LP_GPIO azureIotConnectedLed = { .pin = NETWORK_CONNECTED_LED, .direction = LP_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true,  .name = "azureConnectedLed" };

// Timers
static LP_TIMER azureConnectedStatusTimer = { .period = {10, 0}, .name = "azureConnectedStatusTimer", .handler = NetworkConnectionStatusHandler };
static LP_TIMER measureSensorTimer = { .period = {4, 0}, .name = "measureSensorTimer", .handler = MeasureSensorHandler };
static LP_TIMER temperatureAlertTimer = { .period = {4, 0}, .name = "temperatureAlertTimer", .handler = TemperatureAlertHandler };
static LP_TIMER temperatureAlertBuzzerOffOneShotTimer = { .period = { 0, 0 }, .name = "temperatureAlertBuzzerOffOneShotTimer", .handler = TemperatureAlertBuzzerOffOneShotTimer };

// Azure IoT Device Twins
static LP_DEVICE_TWIN_BINDING desiredTemperature = { .twinProperty = "DesiredTemperature", .twinType = LP_TYPE_FLOAT, .handler = DeviceTwinGenericHandler };
static LP_DEVICE_TWIN_BINDING desiredTemperatureAlertLevel = { .twinProperty = "DesiredTemperatureAlertLevel", .twinType = LP_TYPE_FLOAT, .handler = DeviceTwinGenericHandler };
static LP_DEVICE_TWIN_BINDING actualTemperature = { .twinProperty = "ReportedTemperature", .twinType = LP_TYPE_FLOAT };
static LP_DEVICE_TWIN_BINDING actualHvacState = { .twinProperty = "ReportedHvacState", .twinType = LP_TYPE_STRING };

// Initialize Sets
LP_GPIO* PeripheralGpioSet[] = { &hvacHeatingLed, &hvacCoolingLed, &co2AlertPin, &azureIotConnectedLed };
LP_DEVICE_TWIN_BINDING* deviceTwinBindingSet[] = { &desiredTemperature, &actualTemperature, &desiredTemperatureAlertLevel, &actualHvacState };
LP_TIMER* timerSet[] = { &azureConnectedStatusTimer, &measureSensorTimer, &temperatureAlertTimer, &temperatureAlertBuzzerOffOneShotTimer };

static const char* MsgTemplate = "{ \"Temperature\": %3.2f, \"Humidity\": \"%3.1f\", \"MsgId\":%d }";
static LP_MESSAGE_PROPERTY* telemetryMessageProperties[] = {
	&(LP_MESSAGE_PROPERTY) { .key = "appid", .value = "hvac" },
	&(LP_MESSAGE_PROPERTY) {.key = "format", .value = "json" },
	&(LP_MESSAGE_PROPERTY) {.key = "type", .value = "telemetry" },
	&(LP_MESSAGE_PROPERTY) {.key = "version", .value = "1" }
};
