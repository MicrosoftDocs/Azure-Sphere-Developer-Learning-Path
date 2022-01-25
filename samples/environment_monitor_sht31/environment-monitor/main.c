/*
 *   Please read the disclaimer
 *
 *
 *   DISCLAIMER
 *
 *   The functions provided in the LearningPathLibrary folder:
 *
 *	   1. are NOT supported Azure Sphere APIs.
 *	   2. are prefixed with lp_, typedefs are prefixed with LP_
 *	   3. are built from the Azure Sphere SDK Samples at https://github.com/Azure/azure-sphere-samples
 *	   4. are not intended as a substitute for understanding the Azure Sphere SDK Samples.
 *	   5. aim to follow best practices as demonstrated by the Azure Sphere SDK Samples.
 *	   6. are provided as is and as a convenience to aid the Azure Sphere Developer Learning experience.
 *
 */

#include "main.h"

/// <summary>
/// Check status of connection to Azure IoT
/// </summary>
static LP_TIMER_HANDLER(NetworkConnectionStatusHandler)
{
	static bool toggleConnectionStatusLed = true;

	if (lp_azureConnect())
	{
		lp_gpioStateSet(&azureIotConnectedLed, toggleConnectionStatusLed);
		toggleConnectionStatusLed = !toggleConnectionStatusLed;
	}
	else
	{
		lp_gpioStateSet(&azureIotConnectedLed, false);
	}
}
LP_TIMER_HANDLER_END

/// <summary>
/// Set the HVAC status led.
/// Red if heater needs to be turned on to get to desired temperature.
/// Blue to turn on cooler.
/// Green equals just right, no action required.
/// </summary>
static void SetHvacStatusColour(int temperature)
{
	if (!desiredTemperature.twinStateUpdated)
	{
		return;
	}

	static enum HVAC previous_hvac_state = OFF;
	int actual = (int)temperature;
	int desired = (int)*(float *)desiredTemperature.twinState;

	current_hvac_state = actual == desired ? OFF : actual > desired ? COOLING
																	: HEATING;

	if (previous_hvac_state != current_hvac_state)
	{
		previous_hvac_state = current_hvac_state;
		lp_deviceTwinReportState(&actualHvacState, hvacState[current_hvac_state]);
	}

	lp_gpioOff(&hvacHeatingLed);
	lp_gpioOff(&hvacCoolingLed);

	switch (current_hvac_state)
	{
	case HEATING:
		lp_gpioOn(&hvacHeatingLed);
		break;
	case COOLING:
		lp_gpioOn(&hvacCoolingLed);
		break;
	default:
		break;
	}
}

/// <summary>
/// Turn off CO2 Buzzer
/// </summary>
static LP_TIMER_HANDLER(TemperatureAlertBuzzerOffOneShotTimer)
{
	lp_gpioOff(&co2AlertPin);
}
LP_TIMER_HANDLER_END

/// <summary>
/// Turn on CO2 Buzzer if CO2 ppm greater than desiredTemperatureAlertLevel device twin
/// </summary>
static LP_TIMER_HANDLER(TemperatureAlertHandler)
{
	if (desiredTemperatureAlertLevel.twinStateUpdated && temperature > *(float *)desiredTemperatureAlertLevel.twinState)
	{
		lp_gpioOn(&co2AlertPin);
		lp_timerOneShotSet(&temperatureAlertBuzzerOffOneShotTimer, &co2AlertBuzzerPeriod);
	}
}
LP_TIMER_HANDLER_END

/// <summary>
/// Read sensor and send to Azure IoT
/// </summary>
static LP_TIMER_HANDLER(MeasureSensorHandler)
{
	static int msgId = 0;
	int32_t int32_temperature, int32_humidity;

	/* Measure temperature and relative humidity and store into variables
		 * temperature, humidity (each output multiplied by 1000).
		 */
	int16_t ret = sht3x_measure_blocking_read(&int32_temperature, &int32_humidity);

	temperature = (float)int32_temperature / 1000.0f;
	humidity = (float)int32_humidity / 1000.0f;

	if (ret == STATUS_OK && snprintf(msgBuffer, JSON_MESSAGE_BYTES, MsgTemplate, temperature, humidity, ++msgId) > 0)
	{
		Log_Debug("%s\n", msgBuffer);
		lp_azureMsgSendWithProperties(msgBuffer, telemetryMessageProperties, NELEMS(telemetryMessageProperties));
	}

	SetHvacStatusColour((int)temperature);

	// If the previous temperature not equal to the new temperature then update ReportedTemperature device twin
	if (previous_temperature != (int)temperature)
	{
		lp_deviceTwinReportState(&actualTemperature, &temperature);
	}
	previous_temperature = (int)temperature;
}
LP_TIMER_HANDLER_END

/// <summary>
/// Generic Device Twin Handler. It just sets reported state for the twin
/// </summary>
static LP_DEVICE_TWIN_HANDLER(DeviceTwinGenericHandler, deviceTwinBinding)
{
	lp_deviceTwinReportState(deviceTwinBinding, deviceTwinBinding->twinState);
	lp_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, LP_DEVICE_TWIN_COMPLETED);

	SetHvacStatusColour(previous_temperature);
}
LP_DEVICE_TWIN_HANDLER_END

static bool InitializeSht31(void)
{
	uint16_t interval_in_seconds = 2;
	int retry = 0;

	sensirion_i2c_init();

	while (sht3x_probe() != STATUS_OK && ++retry < 5)
	{
		Log_Debug("SHT sensor probing failed\n");
		sensirion_sleep_usec(1000000u);
	}

	if (retry < 5)
	{
		Log_Debug("SHT sensor probing successful\n");
	}
	else
	{
		Log_Debug("SHT sensor probing failed\n");
	}

	sensirion_sleep_usec(interval_in_seconds * 1000000u); // sleep for good luck

	return true;
}

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static void InitPeripheralGpiosAndHandlers(void)
{
	InitializeSht31();

	lp_azureInitialize(lp_config.scopeId, IOT_PLUG_AND_PLAY_MODEL_ID);

	lp_gpioSetOpen(PeripheralGpioSet, NELEMS(PeripheralGpioSet));
	lp_deviceTwinSetOpen(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));

	lp_timerSetStart(timerSet, NELEMS(timerSet));

	lp_azureToDeviceStart();
}

/// <summary>
///     Close PeripheralGpios and handlers.
/// </summary>
static void ClosePeripheralGpiosAndHandlers(void)
{
	Log_Debug("Closing file descriptors\n");

	lp_timerSetStop(timerSet, NELEMS(timerSet));

	lp_azureToDeviceStop();

	lp_gpioSetClose(PeripheralGpioSet, NELEMS(PeripheralGpioSet));
	lp_deviceTwinSetClose();

	lp_timerEventLoopStop();
}

int main(int argc, char *argv[])
{
	lp_registerTerminationHandler();

	lp_configParseCmdLineArguments(argc, argv, &lp_config);
	if (!lp_configValidate(&lp_config))
	{
		return lp_getTerminationExitCode();
	}

	InitPeripheralGpiosAndHandlers();

    // Run the main event loop. This call blocks until termination requested
	lp_eventLoopRun();

	ClosePeripheralGpiosAndHandlers();

	Log_Debug("Application exiting.\n");
	return lp_getTerminationExitCode();
}