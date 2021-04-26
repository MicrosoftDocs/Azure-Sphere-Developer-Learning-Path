/*
 *   Please read the disclaimer
 *
 *
 *   DISCLAIMER
 *
 *   The learning_path_libs functions provided in the learning_path_libs folder:
 *
 *	   1. are NOT supported Azure Sphere APIs.
 *	   2. are prefixed with dx_, typedefs are prefixed with DX_
 *	   3. are built from the Azure Sphere SDK Samples at https://github.com/Azure/azure-sphere-samples
 *	   4. are not intended as a substitute for understanding the Azure Sphere SDK Samples.
 *	   5. aim to follow best practices as demonstrated by the Azure Sphere SDK Samples.
 *	   6. are provided as is and as a convenience to aid the Azure Sphere Developer Learning experience.
 *
 */

#include "hw/azure_sphere_learning_path.h"

#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_gpio.h"
#include "dx_terminate.h"
#include "dx_timer.h"

#include "location_from_ip.h"

#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "./embedded-scd/scd30/scd30.h"

#define JSON_MESSAGE_BYTES 256 // Number of bytes to allocate for the JSON telemetry message for IoT Central
#define OneMS 1000000		// used to simplify timer defn.

 // Forward signatures
static void CO2AlertBuzzerOffOneShotTimer(EventLoopTimer* eventLoopTimer);
static void CO2AlertBuzzerOnHandler(EventLoopTimer* eventLoopTimer);
static void ConnectedLedOffHandler(EventLoopTimer* eventLoopTimer);
static void ConnectedLedOnHandler(EventLoopTimer* eventLoopTimer);
static void DeviceTwinGenericHandler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static void MeasureSensorHandler(EventLoopTimer* eventLoopTimer);
static void PublishTelemetryHandler(EventLoopTimer* eventLoopTimer);
static void SetHvacStatusColour(int32_t temperature);

DX_USER_CONFIG dx_config;
enum LEDS { RED, GREEN, BLUE, UNKNOWN };
static enum LEDS current_led = UNKNOWN;

struct location_info* locInfo = NULL;

static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };

static float co2_ppm = NAN, temperature = NAN, relative_humidity = NAN;
static const struct timespec co2AlertBuzzerPeriod = { 0, 5 * 100 * 1000 }; // 500 microseconds

//GPIO
static DX_GPIO co2AlertBuzzerPin = { .pin = CO2_ALERT, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = false, .name = "co2AlertBuzzerPin" };
static DX_GPIO azureIotConnectedLed = { .pin = CONNECTED_LED, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "azureConnectedLed" };
static DX_GPIO* ledRgb[] = {
	&(DX_GPIO) { .pin = LED_RED, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true,.name = "red led" },
	&(DX_GPIO) {.pin = LED_GREEN, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "green led" },
	&(DX_GPIO) {.pin = LED_BLUE, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "blue led" }
};

// Timers
static DX_TIMER co2AlertBuzzerOffOneShotTimer = { .period = { 0, 0 }, .name = "co2AlertBuzzerOffOneShotTimer", .handler = CO2AlertBuzzerOffOneShotTimer };
static DX_TIMER co2AlertBuzzerOnTimer = { .period = {4, 0}, .name = "co2AlertBuzzerOnTimer", .handler = CO2AlertBuzzerOnHandler };
static DX_TIMER connectedLedOffTimer = { .period = {0,0},.name = "connectedLedOffTimer",.handler = ConnectedLedOffHandler };
static DX_TIMER connectedLedOnTimer = { .period = {0, 0},.name = "connectedLedOnTimer",.handler = ConnectedLedOnHandler };
static DX_TIMER measureSensorTimer = { .period = {20, 0}, .name = "measureSensorTimer", .handler = MeasureSensorHandler };
static DX_TIMER publishTelemetryTimer = { .period = {4, 0}, .name = "publishTelemetryTimer", .handler = PublishTelemetryHandler };

// Azure IoT Device Twins
static DX_DEVICE_TWIN_BINDING desiredCO2AlertLevel = { .twinProperty = "DesiredCO2AlertLevel", .twinType = DX_TYPE_INT, .handler = DeviceTwinGenericHandler };
static DX_DEVICE_TWIN_BINDING desiredTemperature = { .twinProperty = "DesiredTemperature", .twinType = DX_TYPE_INT, .handler = DeviceTwinGenericHandler };
static DX_DEVICE_TWIN_BINDING reportedCO2Level = { .twinProperty = "ReportedCO2Level", .twinType = DX_TYPE_FLOAT };
static DX_DEVICE_TWIN_BINDING reportedLatitude = { .twinProperty = "ReportedLatitude",.twinType = DX_TYPE_FLOAT };
static DX_DEVICE_TWIN_BINDING reportedLongitude = { .twinProperty = "ReportedLongitude",.twinType = DX_TYPE_FLOAT };
static DX_DEVICE_TWIN_BINDING reportedCountryCode = { .twinProperty = "ReportedCountryCode",.twinType = DX_TYPE_STRING };

// Initialize Sets
DX_GPIO* gpioSet[] = { &co2AlertBuzzerPin, &azureIotConnectedLed };
DX_DEVICE_TWIN_BINDING* deviceTwinBindingSet[] = {
	&desiredCO2AlertLevel, &reportedCO2Level, &desiredTemperature, &reportedLatitude, &reportedLongitude, &reportedCountryCode };
DX_TIMER* timerSet[] = {
	&connectedLedOnTimer, &connectedLedOffTimer, &measureSensorTimer, &publishTelemetryTimer,
	&co2AlertBuzzerOnTimer, &co2AlertBuzzerOffOneShotTimer
};

// Define the message to be sent to Azure IoT Hub
static const char* MsgTemplate = "{ \"CO2\": %3.2f, \"Temperature\": %3.2f, \"Humidity\": \"%3.1f\", \"Longitude\", %f, \"Latitude\":%f, \"MsgId\":%d }";
// Attach these properties when sending telemetry to Azure IoT Hub
static DX_MESSAGE_PROPERTY* telemetryMessageProperties[] = {
	&(DX_MESSAGE_PROPERTY) { .key = "appid", .value = "co2monitor" },
	&(DX_MESSAGE_PROPERTY) {.key = "format", .value = "json" },
	&(DX_MESSAGE_PROPERTY) {.key = "type", .value = "telemetry" },
	&(DX_MESSAGE_PROPERTY) {.key = "version", .value = "1" }
};

/*Timer event handlers******************************************/

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
	if (desiredCO2AlertLevel.twinStateUpdated && !isnan(co2_ppm) && co2_ppm > *(int*)desiredCO2AlertLevel.twinState) {
		dx_gpioOn(&co2AlertBuzzerPin);
		dx_timerOneShotSet(&co2AlertBuzzerOffOneShotTimer, &co2AlertBuzzerPeriod);
	}
}

static void PublishTelemetryHandler(EventLoopTimer* eventLoopTimer) {
	static int msgId = 0;
	float lng, lat;

	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}
	if (!isnan(co2_ppm)) {

		lng = locInfo == NULL ? NAN : (float)locInfo->lng;
		lat = locInfo == NULL ? NAN : (float)locInfo->lat;

		if (snprintf(msgBuffer, JSON_MESSAGE_BYTES, MsgTemplate, co2_ppm, temperature, relative_humidity, lng, lat, ++msgId) > 0) {
			Log_Debug("%s\n", msgBuffer);
			dx_azureMsgSendWithProperties(msgBuffer, telemetryMessageProperties, NELEMS(telemetryMessageProperties));
		}
		dx_deviceTwinReportState(&reportedCO2Level, &co2_ppm);
	}
}

/// <summary>
/// Read sensor and send to Azure IoT
/// </summary>
static void MeasureSensorHandler(EventLoopTimer* eventLoopTimer) {
	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}
	if (scd30_read_measurement(&co2_ppm, &temperature, &relative_humidity) != STATUS_OK) {
		co2_ppm = NAN;
	}

	SetHvacStatusColour((int)temperature);
}

static void ConnectedLedOffHandler(EventLoopTimer* eventLoopTimer) {
	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}
	dx_gpioOff(&azureIotConnectedLed);
}

/// <summary>
/// Check status of connection to Azure IoT
/// </summary>
static void ConnectedLedOnHandler(EventLoopTimer* eventLoopTimer) {
	static bool first_connect = true;

	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}
	if (dx_azureIsConnected()) {
		dx_gpioOn(&azureIotConnectedLed);
		// on for 1300ms off for 100ms = 1400 ms in total
		dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 400 * OneMS});
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){1, 300 * OneMS});
	} else if (dx_isNetworkReady()) {
		dx_gpioOn(&azureIotConnectedLed);
		// on for 100ms off for 1300ms = 1400 ms in total
		dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 400 * OneMS});
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){0, 100 * OneMS});

		if (first_connect) {
			// set device long/lat using geo location lookup
			first_connect = false;
			locInfo = GetLocationData();
			// update long, lat, and country code device twins
			dx_deviceTwinReportState(&reportedLatitude, &locInfo->lat);
			dx_deviceTwinReportState(&reportedLongitude, &locInfo->lng);
			dx_deviceTwinReportState(&reportedCountryCode, &locInfo->countryCode);
		}
	} else {
		dx_gpioOn(&azureIotConnectedLed);
		// on for 700ms off for 700ms = 1400 ms in total
		dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 400 * OneMS});
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){0, 700 * OneMS});
	}
}


/*Device Twin Handlers******************************************/

/// <summary>
/// Generic Device Twin Handler. It just sets reported state for the twin
/// </summary>
static void DeviceTwinGenericHandler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
	dx_deviceTwinReportState(deviceTwinBinding, deviceTwinBinding->twinState);
	dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
	SetHvacStatusColour((int)temperature);
}

static inline bool InRange(int low, int high, int x) {
	return  low <= x && x <= high;
}

/// <summary>
/// Set the temperature status led. 
/// Red if HVAC needs to be turned on to get to desired temperature. 
/// Blue to turn on cooler. 
/// Green equals just right, no action required.
/// </summary>
static void SetHvacStatusColour(int32_t temperature) {
	static enum LEDS previous_led = UNKNOWN;

	// desired temperature device twin not yet updated
	if (!desiredTemperature.twinStateUpdated) { return; }

	int desired_temperature = *(int*)desiredTemperature.twinState;

	current_led = InRange(desired_temperature - 1, desired_temperature + 1, temperature)
		? GREEN : temperature > desired_temperature + 1 ? BLUE : RED;

	if (previous_led != current_led) {
		if (previous_led != UNKNOWN) {
			dx_gpioOff(ledRgb[(int)previous_led]); // turn off old current colour
		}
		previous_led = current_led;
	}
	dx_gpioOn(ledRgb[(int)current_led]);
}


static bool InitializeSdc30(void) {
	uint16_t interval_in_seconds = 2;
	int retry = 0;
	uint8_t asc_enabled, enable_asc;

	sensirion_i2c_init();

	while (scd30_probe() != STATUS_OK && ++retry < 5) {
		printf("SCD30 sensor probing failed\n");
		sensirion_sleep_usec(1000000u);
	}

	if (retry >= 5) { return false; }

	/*
	When scd30 automatic self calibration activated for the first time a period of minimum 7 days is needed so
	that the algorithm can find its initial parameter set for ASC. The sensor has to be exposed to fresh air for at least 1 hour every day.
	Refer to the datasheet for further conditions and scd30.h for more info.
	*/

	if (scd30_get_automatic_self_calibration(&asc_enabled) == 0) {
		if (asc_enabled == 0) {
			enable_asc = 1;
			if (scd30_enable_automatic_self_calibration(enable_asc) == 0) {
				Log_Debug("scd30 automatic self calibration enabled. Takes 7 days, at least 1 hour/day outside, powered continuously");
			}
		}
	}

	scd30_set_measurement_interval(interval_in_seconds);
	sensirion_sleep_usec(20000u);
	scd30_start_periodic_measurement(0);
	sensirion_sleep_usec(interval_in_seconds * 1000000u);

	return true;
}

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static void InitPeripheralGpiosAndHandlers(void) {
	dx_azureInitialize(dx_config.scopeId, NULL);

	InitializeSdc30();
	scd30_read_measurement(&co2_ppm, &temperature, &relative_humidity);

	dx_gpioSetOpen(gpioSet, NELEMS(gpioSet));
	dx_gpioSetOpen(ledRgb, NELEMS(ledRgb));
	dx_deviceTwinSetOpen(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));
	dx_timerSetStart(timerSet, NELEMS(timerSet));
	dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 0});
}

/// <summary>
///     Close PeripheralGpios and handlers.
/// </summary>
static void ClosePeripheralGpiosAndHandlers(void) {
	dx_timerSetStop(timerSet, NELEMS(timerSet));
	dx_azureToDeviceStop();
	dx_gpioSetClose(gpioSet, NELEMS(gpioSet));
	dx_gpioSetClose(ledRgb, NELEMS(ledRgb));
	dx_deviceTwinSetClose();
	scd30_stop_periodic_measurement();
	dx_timerEventLoopStop();
}

int main(int argc, char* argv[]) {
	dx_registerTerminationHandler();
	dx_configParseCmdLineArguments(argc, argv, &dx_config);
	if (!dx_configValidate(&dx_config)) {
		return dx_getTerminationExitCode();
	}
	InitPeripheralGpiosAndHandlers();

	// Main loop
	while (!dx_isTerminationRequired()) {
		int result = EventLoop_Run(dx_timerGetEventLoop(), -1, true);
		// Continue if interrupted by signal, e.g. due to breakpoint being set.
		if (result == -1 && errno != EINTR) {
			dx_terminate(DX_ExitCode_Main_EventLoopFail);
		}
	}

	ClosePeripheralGpiosAndHandlers();
	return dx_getTerminationExitCode();
}