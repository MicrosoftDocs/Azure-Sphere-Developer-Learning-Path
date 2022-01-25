 // Hardware definition
#include "hw/azure_sphere_learning_path.h"

// Learning Path Libraries
#include "azure_iot.h"
#include "exit_codes.h"
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

// Hardware specific
#ifdef OEM_AVNET
#include "board.h"
#include "imu_temp_pressure.h"
#include "light_sensor.h"
#endif // OEM_AVNET

// Hardware specific
#ifdef OEM_SEEED_STUDIO
#include "SEEED_STUDIO/board.h"
#endif // SEEED_STUDIO

#define LP_LOGGING_ENABLED FALSE
#define JSON_MESSAGE_BYTES 256  // Number of bytes to allocate for the JSON telemetry message for IoT Central

// Forward signatures
static LP_DECLARE_TIMER_HANDLER(AlertLedOffToggleHandler);
static LP_DECLARE_TIMER_HANDLER(ButtonPressCheckHandler);
static LP_DECLARE_TIMER_HANDLER(MeasureSensorHandler);
static LP_DECLARE_TIMER_HANDLER(NetworkConnectionStatusHandler);

static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };

static const char* msgTemplate = "{ \"Temperature\":%3.2f, \"Humidity\":%3.1f, \"Pressure\":%3.1f, \"MsgId\":%d }";

// GPIO Input Peripherals
static LP_GPIO buttonA = {
	.pin = BUTTON_A,
	.direction = LP_INPUT,
	.name = "buttonA" };

static LP_GPIO alertLed = {
	.pin = ALERT_LED,
	.direction = LP_OUTPUT,
	.initialState = GPIO_Value_Low,
	.invertPin = true,
	.name = "alertLed" };

static LP_GPIO networkConnectedLed = {
	.pin = NETWORK_CONNECTED_LED,
	.direction = LP_OUTPUT,
	.initialState = GPIO_Value_Low,
	.invertPin = true,
	.name = "networkConnectedLed" };

// Timers
static LP_TIMER buttonPressCheckTimer = {
	.period = { 0, 1000000 },
	.name = "buttonPressCheckTimer",
	.handler = ButtonPressCheckHandler };

static LP_TIMER networkConnectionStatusTimer = {
	.period = { 5, 0 },
	.name = "networkConnectionStatusTimer",
	.handler = NetworkConnectionStatusHandler };

static LP_TIMER measureSensorTimer = {
	.period = { 4, 0 },
	.name = "measureSensorTimer",
	.handler = MeasureSensorHandler };

static LP_TIMER alertLedOffOneShotTimer = {
	.period = { 0, 0 },
	.name = "alertLedOffOneShotTimer",
	.handler = AlertLedOffToggleHandler };

// Initialize Sets
LP_GPIO* gpioSet[] = { &buttonA, &networkConnectedLed, &alertLed };
LP_TIMER* timerSet[] = { &buttonPressCheckTimer, &networkConnectionStatusTimer, &measureSensorTimer, &alertLedOffOneShotTimer };
