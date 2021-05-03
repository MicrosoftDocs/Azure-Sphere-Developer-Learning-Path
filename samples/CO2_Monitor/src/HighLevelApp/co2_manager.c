#include "co2_manager.h"

bool InitializeSdc30(void) {
	uint16_t interval_in_seconds = 2;
	int retry = 0;
	uint8_t asc_enabled, enable_asc;

	sensirion_i2c_init();

	while (scd30_probe() != STATUS_OK && ++retry < 5) {
		Log_Debug("SCD30 sensor probing failed\n");
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