#include "hw/azure_sphere_learning_path.h"

#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_terminate.h"
#include "dx_timer.h"

#include "buzzer_alert.h"
#include "co2_manager.h"
#include "connected_status.h"
#include "hvac_status.h"
#include "location_from_ip.h"

#include "applibs_versions.h"
#include <applibs/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define JSON_MESSAGE_BYTES 256 // Number of bytes to allocate for the JSON telemetry message for IoT Central
#define OneMS 1000000		// used to simplify timer defn.
#define NETWORK_INTERFACE "wlan0"

// Forward signatures
static void DeviceTwinGenericHandler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static void MeasureSensorHandler(EventLoopTimer* eventLoopTimer);
static void PublishTelemetryHandler(EventLoopTimer* eventLoopTimer);

DX_USER_CONFIG dx_config;
struct location_info* locInfo = NULL;
float co2_ppm = NAN, temperature = NAN, relative_humidity = NAN;
static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };

// Timers
static DX_TIMER_BINDING measureSensorTimer = { .period = {20, 0}, .name = "measureSensorTimer", .handler = MeasureSensorHandler };
static DX_TIMER_BINDING publishTelemetryTimer = { .period = {4, 0}, .name = "publishTelemetryTimer", .handler = PublishTelemetryHandler };

DX_TIMER_BINDING* timerSet[] = { &measureSensorTimer, &publishTelemetryTimer };

// Azure IoT Device Twins
DX_DEVICE_TWIN_BINDING desiredCO2AlertLevel = { .twinProperty = "DesiredCO2AlertLevel", .twinType = DX_TYPE_INT, .handler = DeviceTwinGenericHandler };
DX_DEVICE_TWIN_BINDING desiredTemperature = { .twinProperty = "DesiredTemperature", .twinType = DX_TYPE_INT, .handler = DeviceTwinGenericHandler };
DX_DEVICE_TWIN_BINDING reportedCO2Level = { .twinProperty = "ActualCO2Level", .twinType = DX_TYPE_FLOAT };
DX_DEVICE_TWIN_BINDING reportedCountryCode = { .twinProperty = "ReportedCountryCode",.twinType = DX_TYPE_STRING };
DX_DEVICE_TWIN_BINDING reportedLatitude = { .twinProperty = "ReportedLatitude",.twinType = DX_TYPE_DOUBLE };
DX_DEVICE_TWIN_BINDING reportedLongitude = { .twinProperty = "ReportedLongitude",.twinType = DX_TYPE_DOUBLE };

// Initialize Sets
DX_DEVICE_TWIN_BINDING* deviceTwinBindingSet[] = {
	&desiredCO2AlertLevel, &reportedCO2Level, &desiredTemperature,
	&reportedLatitude, &reportedLongitude, &reportedCountryCode
};

// Define the message to be sent to Azure IoT Hub
static const char* MsgTemplate = "{ \"CO2\": %3.2f, \"Temperature\": %3.2f, \"Humidity\": %3.1f, \"Longitude\": %lf, \"Latitude\":%lf, \"MsgId\":%d }";

// Attach application properties when sending telemetry to Azure IoT Hub
static DX_MESSAGE_PROPERTY* telemetryMessageProperties[] = {
	&(DX_MESSAGE_PROPERTY) { .key = "appid", .value = "co2monitor" },
	&(DX_MESSAGE_PROPERTY) {.key = "type", .value = "telemetry" },
	&(DX_MESSAGE_PROPERTY) {.key = "schema", .value = "1" }
};

// Attach content properties when sending telemetry to Azure IoT Hub
static DX_MESSAGE_CONTENT_PROPERTIES telemetryContentProperties = {
	.contentEncoding = "utf-8",
	.contentType = "application/json"
};

/*Timer event handlers******************************************/
static void PublishTelemetryHandler(EventLoopTimer* eventLoopTimer) {
	static int msgId = 0;

	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}

	if (!isnan(co2_ppm) && locInfo != NULL) {
		if (snprintf(msgBuffer, JSON_MESSAGE_BYTES, MsgTemplate, co2_ppm, temperature, relative_humidity, locInfo->lng, locInfo->lat, ++msgId) > 0) {

			Log_Debug("%s\n", msgBuffer);

			dx_azurePublish(msgBuffer, strlen(msgBuffer), telemetryMessageProperties, NELEMS(telemetryMessageProperties), &telemetryContentProperties);
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

/// <summary>
/// Generic Device Twin Handler to acknowledge device twin received
/// </summary>
static void DeviceTwinGenericHandler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
	dx_deviceTwinReportState(deviceTwinBinding, deviceTwinBinding->twinState);
	dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
	SetHvacStatusColour((int)temperature);
}

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static void InitPeripheralGpiosAndHandlers(void) {
	dx_azureConnect(&dx_config, NETWORK_INTERFACE, NULL);
	InitializeSdc30();
	scd30_read_measurement(&co2_ppm, &temperature, &relative_humidity);
	dx_deviceTwinSubscribe(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));
	dx_timerSetStart(timerSet, NELEMS(timerSet));
	CO2AlertBuzzerInitialize();
	ConnectedStatusInitialise();
}

/// <summary>
///     Close PeripheralGpios and handlers.
/// </summary>
static void ClosePeripheralGpiosAndHandlers(void) {
	dx_timerSetStop(timerSet, NELEMS(timerSet));
	dx_azureToDeviceStop();
	dx_deviceTwinUnsubscribe();
	scd30_stop_periodic_measurement();
	dx_timerEventLoopStop();
}

int main(int argc, char* argv[]) {
	dx_registerTerminationHandler();
	if (!dx_configParseCmdLineArguments(argc, argv, &dx_config)) {
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