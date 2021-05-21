#include "connected_status.h"

static void ConnectedLedOnHandler(EventLoopTimer* eventLoopTimer);
static void ConnectedLedOffHandler(EventLoopTimer* eventLoopTimer);

static DX_GPIO_BINDING azureIotConnectedLed = { .pin = CONNECTED_LED, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "azureConnectedLed" };

static DX_TIMER_BINDING connectedLedOffTimer = { .period = {0, 0},.name = "connectedLedOffTimer",.handler = ConnectedLedOffHandler };
static DX_TIMER_BINDING connectedLedOnTimer = { .period = {0, 0},.name = "connectedLedOnTimer",.handler = ConnectedLedOnHandler };

void ConnectedStatusInitialise(void) {
	dx_gpioOpen(&azureIotConnectedLed);
	dx_timerStart(&connectedLedOnTimer);
	dx_timerStart(&connectedLedOffTimer);

	dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 0});
}

void ConnectedLedOffHandler(EventLoopTimer* eventLoopTimer) {
	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}
	dx_gpioOff(&azureIotConnectedLed);
}

/// <summary>
/// Check status of connection to Azure IoT
/// </summary>
void ConnectedLedOnHandler(EventLoopTimer* eventLoopTimer) {
	static bool first_connect = true;
	static int init_sequence = 25;

	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}

	if (init_sequence-- > 0) {

		dx_gpioOn(&azureIotConnectedLed);
		// on for 100ms off for 100ms = 200 ms in total
		dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){0, 200 * OneMS});
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){0, 100 * OneMS});

	} else if (dx_isAzureConnected()) {

		dx_gpioOn(&azureIotConnectedLed);
		// on for 1300ms off for 100ms = 1400 ms in total
		dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 400 * OneMS});
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){1, 300 * OneMS});

	} else if (dx_isNetworkReady()) {

		dx_gpioOn(&azureIotConnectedLed);
		// on for 100ms off for 1300ms = 1400 ms in total
		dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 400 * OneMS});
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){0, 700 * OneMS});

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
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){0, 100 * OneMS});
	}
}
