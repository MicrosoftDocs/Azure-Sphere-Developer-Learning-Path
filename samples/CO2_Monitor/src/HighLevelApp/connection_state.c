#include "connection_state.h"


/// <summary>
/// Check status of connection to Azure IoT
/// </summary>
static void ConnectedLedOnHandler(EventLoopTimer* eventLoopTimer) {
	bool first_connect = true;
	struct location_info* locInfo = NULL;
	float lng, lat;

	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}
	if (dx_azureIsConnected()) {
		dx_gpioOn(&azureIotConnectedLed);
		// on for 1300ms off for 100ms = 1400 ms in total
		dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 400 * OneMS});
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){1, 300 * OneMS});

		if (first_connect) {
			first_connect = false;
			locInfo = GetLocationData();

			lat = (float)locInfo->lat;
			lng = (float)locInfo->lng;

			dx_deviceTwinReportState(&reportedLatitude, &lat);
			dx_deviceTwinReportState(&reportedLongitude, &lng);
			dx_deviceTwinReportState(&reportedCountryCode, &locInfo->countryCode);
		}
	} else if (dx_isNetworkReady()) {
		dx_gpioOn(&azureIotConnectedLed);
		// on for 100ms off for 1300ms = 1400 ms in total
		dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 400 * OneMS});
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){0, 100 * OneMS});
	} else {
		dx_gpioOn(&azureIotConnectedLed);
		// on for 700ms off for 700ms = 1400 ms in total
		dx_timerOneShotSet(&connectedLedOnTimer, &(struct timespec){1, 400 * OneMS});
		dx_timerOneShotSet(&connectedLedOffTimer, &(struct timespec){0, 700 * OneMS});
	}
}