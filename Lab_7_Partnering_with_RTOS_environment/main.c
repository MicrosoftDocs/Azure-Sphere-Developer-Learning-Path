/*
 *   AzureSphereDevX
 *   ===============
 *   These labs are built on version 1 of the Azure Sphere Learning Path library.
 *   Version 2 of the Learning Path library is called AzureSphereDevX.
 *
 *   The AzureSphereDevX documentation and examples are located at https://github.com/Azure-Sphere-DevX/AzureSphereDevX.Examples.
 *   AzureSphereDevX builds on the Azure Sphere Learning Path library incorporating more customer experiences.
 *   Everything you learn completing these labs is relevant to AzureSphereDevX.
 * 	 
 * 
 *   LAB UPDATES
 *   ===========
 *   This lab now use the LP_TIMER_HANDLER macro to define timer handlers
 *   This lab now use the LP_DEVICE_TWIN_HANDLER macro to define device twin handlers
 *   This lab now use the LP_DIRECT_METHOD_HANDLER macro to define direct method handlers
 *   All application declarations are located in main.h
 *    
 *
 *   DISCLAIMER
 *   ==========
 *   The functions provided in the LearningPathLibrary folder:
 *
 *	   1. are prefixed with lp_, typedefs are prefixed with LP_
 *	   2. are built from the Azure Sphere SDK Samples at https://github.com/Azure/azure-sphere-samples
 *	   3. are not intended as a substitute for understanding the Azure Sphere SDK Samples.
 *	   4. aim to follow best practices as demonstrated by the Azure Sphere SDK Samples.
 *	   5. are provided as is and as a convenience to aid the Azure Sphere Developer Learning experience.
 *
 *
 *   DEVELOPER BOARD SELECTION
 *   =========================
 *   The following developer boards are supported.
 *
 *	   1. AVNET Azure Sphere Starter Kit.
 *     2. AVNET Azure Sphere Starter Kit Revision 2.
 *	   3. Seeed Studio Azure Sphere MT3620 Development Kit aka Reference Design Board or rdb.
 *	   4. Seeed Studio Seeed Studio MT3620 Mini Dev Board.
 *
 *   ENABLE YOUR DEVELOPER BOARD
 *   ===========================
 *   Each Azure Sphere developer board manufacturer maps pins differently. You need to select the configuration that matches your board.
 *
 *   Follow these steps:
 *
 *	   1. Open CMakeLists.txt.
 *	   2. Uncomment the set command that matches your developer board.
 *	   3. Click File, then Save to save the CMakeLists.txt file which will auto generate the CMake Cache.
 */

#include "main.h"

/// <summary>
/// Check status of connection to Azure IoT
/// </summary>
static LP_TIMER_HANDLER(AzureIoTConnectionStatusHandler)
{
	static bool toggleConnectionStatusLed = true;
	static bool firstConnect = true;

	if (lp_azureConnect())
	{
		lp_gpioStateSet(&azureIotConnectedLed, toggleConnectionStatusLed);
		toggleConnectionStatusLed = !toggleConnectionStatusLed;

		if (firstConnect)
		{
			lp_deviceTwinReportState(&dt_reportedDeviceStartTime, lp_getCurrentUtc(msgBuffer, sizeof(msgBuffer))); // LP_TYPE_STRING
			firstConnect = false;
		}
	}
	else
	{
		lp_gpioStateSet(&azureIotConnectedLed, false);
	}
}
LP_TIMER_HANDLER_END

/// <summary>
/// Request latest sensor data from the real-time core
/// </summary>
static LP_TIMER_HANDLER(MeasureSensorHandler)
{
	// send request to Real-Time core app to read temperature, pressure, and humidity
	ic_control_block.cmd = LP_IC_ENVIRONMENT_SENSOR;
	lp_interCoreSendMessage(&ic_control_block, sizeof(ic_control_block));
}
LP_TIMER_HANDLER_END

/// <summary>
/// Set the temperature status led.
/// Red if HVAC needs to be turned on to get to desired temperature.
/// Blue to turn on cooler.
/// Green equals just right, no action required.
/// </summary>
void SetHvacStatusColour(int temperature)
{
	static enum LEDS previous_led = UNKNOWN;

	// No desired temperature device twin update to date so return
	if (!dt_desiredTemperature.twinStateUpdated)
	{
		return;
	}

	int desired = (int)(*(float *)dt_desiredTemperature.twinState);

	current_led = temperature == desired ? GREEN : temperature > desired ? BLUE
																		 : RED;

	if (previous_led != current_led)
	{
		lp_gpioOff(ledRgb[(int)previous_led]); // turn off old current colour
		previous_led = current_led;
		lp_deviceTwinReportState(&dt_reportedHvacState, (void *)hvacState[(int)current_led]);
	}
	lp_gpioOn(ledRgb[(int)current_led]);
}

/// <summary>
/// Device Twin Handler to set the desired temperature value
/// </summary>
static LP_DEVICE_TWIN_HANDLER(DeviceTwinSetTemperatureHandler, deviceTwinBinding)
{
	// validate data is sensible range before applying
	if (deviceTwinBinding->twinType == LP_TYPE_FLOAT && *(float *)deviceTwinBinding->twinState >= -20.0f && *(float *)deviceTwinBinding->twinState <= 80.0f)
	{
		lp_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, LP_DEVICE_TWIN_COMPLETED);
		SetHvacStatusColour((int)previous_temperature);
	}
	else
	{
		lp_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, LP_DEVICE_TWIN_ERROR);
	}
}
LP_DEVICE_TWIN_HANDLER_END

/// <summary>
/// Device Twin Handler to set the sensor sample rate on the real-time core
/// </summary>
static LP_DEVICE_TWIN_HANDLER(DeviceTwinSetSampleRateHandler, deviceTwinBinding)
{
	// validate data is sensible range before applying
	if (deviceTwinBinding->twinType == LP_TYPE_INT && *(int *)deviceTwinBinding->twinState > 0 && *(int *)deviceTwinBinding->twinState <= 60)
	{
		ic_control_block.cmd = LP_IC_SAMPLE_RATE;
		ic_control_block.sample_rate = *(int *)deviceTwinBinding->twinState;
		lp_interCoreSendMessage(&ic_control_block, sizeof(ic_control_block));

		lp_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, LP_DEVICE_TWIN_COMPLETED);
	}
	else
	{
		lp_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, LP_DEVICE_TWIN_ERROR);
	}
}
LP_DEVICE_TWIN_HANDLER_END

/// <summary>
/// Callback handler for Inter-Core Messaging - Does Device Twin Update, and Event Message
/// </summary>
static void InterCoreHandler(LP_INTER_CORE_BLOCK *ic_message_block)
{
	static int msgId = 0;

	switch (ic_message_block->cmd)
	{
	case LP_IC_ENVIRONMENT_SENSOR:
		if (snprintf(msgBuffer, JSON_MESSAGE_BYTES, msgTemplate, ic_message_block->temperature,
					 ic_message_block->humidity, ic_message_block->pressure, msgId++) > 0)
		{

			Log_Debug("%s\n", msgBuffer);
			lp_azureMsgSendWithProperties(msgBuffer, telemetryMessageProperties, NELEMS(telemetryMessageProperties));

			SetHvacStatusColour((int)ic_message_block->temperature);

			// If the previous temperature not equal to the new temperature then update ReportedTemperature device twin
			if (previous_temperature != (int)ic_message_block->temperature)
			{
				lp_deviceTwinReportState(&dt_reportedTemperature, &ic_message_block->temperature);
				previous_temperature = (int)ic_message_block->temperature;
			}
		}
		break;
	default:
		break;
	}
}

/// <summary>
/// Restart the Device
/// </summary>
static LP_TIMER_HANDLER(DelayRestartDeviceTimerHandler)
{
	PowerManagement_ForceSystemReboot();
}
LP_TIMER_HANDLER_END

/// <summary>
/// Start Device Power Restart Direct Method 'ResetMethod' integer seconds eg 5
/// Update Restart UTC Device Twin
/// Set oneshot timer to restart the device
/// </summary>
static LP_DIRECT_METHOD_HANDLER(RestartDeviceHandler, json, directMethodBinding, responseMsg)
{
	if (json_value_get_type(json) != JSONNumber)
	{
		return LP_METHOD_FAILED;
	}

	int seconds = (int)json_value_get_number(json);

	if (seconds > 2 && seconds < 10) // leave enough time for the device twin dt_reportedRestartUtc to update before restarting the device
	{
		lp_deviceTwinReportState(&dt_reportedRestartUtc, lp_getCurrentUtc(msgBuffer, sizeof(msgBuffer))); // LP_TYPE_STRING
		lp_timerOneShotSet(&restartDeviceOneShotTimer, &(struct timespec){.tv_sec = seconds, .tv_nsec = 0});
		return LP_METHOD_SUCCEEDED;
	}
	else
	{
		return LP_METHOD_FAILED;
	}
}
LP_DIRECT_METHOD_HANDLER_END

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static void InitPeripheralAndHandlers(void)
{
	lp_azureInitialize(lp_config.scopeId, IOT_PLUG_AND_PLAY_MODEL_ID);

	lp_gpioSetOpen(gpioSet, NELEMS(gpioSet));
	lp_gpioSetOpen(ledRgb, NELEMS(ledRgb));
	lp_timerSetStart(timerSet, NELEMS(timerSet));
	lp_deviceTwinSetOpen(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));
	lp_directMethodSetOpen(directMethodBindingSet, NELEMS(directMethodBindingSet));

	lp_azureToDeviceStart();

	lp_interCoreCommunicationsEnable(REAL_TIME_COMPONENT_ID, InterCoreHandler); // Initialize Inter Core Communications
}

/// <summary>
/// Close PeripheralGpios and handlers.
/// </summary>
static void ClosePeripheralAndHandlers(void)
{
	Log_Debug("Closing file descriptors\n");

	lp_deviceTwinSetClose();
	lp_directMethodSetClose();

	lp_timerSetStop(timerSet, NELEMS(timerSet));
	lp_azureToDeviceStop();

	lp_gpioSetClose(gpioSet, NELEMS(gpioSet));
	lp_gpioSetClose(ledRgb, NELEMS(ledRgb));
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

	InitPeripheralAndHandlers();

    // This call blocks until termination requested
	lp_eventLoopRun();

	ClosePeripheralAndHandlers();

	Log_Debug("Application exiting.\n");
	return lp_getTerminationExitCode();
}