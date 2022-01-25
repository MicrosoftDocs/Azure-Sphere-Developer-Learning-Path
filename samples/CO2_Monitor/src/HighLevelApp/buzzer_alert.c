#include "buzzer_alert.h"

static void CO2AlertBuzzerOffOneShotTimer(EventLoopTimer* eventLoopTimer);
static void CO2AlertBuzzerOnHandler(EventLoopTimer* eventLoopTimer);

static DX_GPIO_BINDING co2AlertBuzzerPin = { .pin = CO2_ALERT, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = false, .name = "co2AlertBuzzerPin" };
static DX_TIMER_BINDING co2AlertBuzzerOffOneShotTimer = { .period = { 0, 0 }, .name = "co2AlertBuzzerOffOneShotTimer", .handler = CO2AlertBuzzerOffOneShotTimer };
static DX_TIMER_BINDING co2AlertBuzzerOnTimer = { .period = {4, 0}, .name = "co2AlertBuzzerOnTimer", .handler = CO2AlertBuzzerOnHandler };

static const struct timespec co2AlertBuzzerPeriod = { 0, 5 * 100 * 1000 }; // 500 microseconds

static bool buzzer_initialized = false;

void CO2AlertBuzzerInitialize(void) {
	dx_timerStart(&co2AlertBuzzerOffOneShotTimer);
	dx_timerStart(&co2AlertBuzzerOnTimer);
	dx_gpioOpen(&co2AlertBuzzerPin);
}

/// <summary>
/// Turn off CO2 Buzzer
/// </summary>
static void CO2AlertBuzzerOffOneShotTimer(EventLoopTimer* eventLoopTimer) {
	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}
	dx_gpioOff(&co2AlertBuzzerPin);
}

/// <summary>
/// Turn on CO2 Buzzer if CO2 ppm greater than desiredCO2AlertLevel device twin
/// </summary>
static void CO2AlertBuzzerOnHandler(EventLoopTimer* eventLoopTimer) {
	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}

	if (!buzzer_initialized) {
		buzzer_initialized = true;
		dx_gpioOpen(&co2AlertBuzzerPin);
	}

	if (desiredCO2AlertLevel.twinStateUpdated && !isnan(co2_ppm) && co2_ppm > *(int*)desiredCO2AlertLevel.twinState) {
		dx_gpioOn(&co2AlertBuzzerPin);
		dx_timerOneShotSet(&co2AlertBuzzerOffOneShotTimer, &co2AlertBuzzerPeriod);
	}
}