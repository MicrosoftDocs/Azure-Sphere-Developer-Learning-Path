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
 

static LP_TIMER_HANDLER(ReadSensorHandler)
{
	static int msgId = 0;
	static LP_ENVIRONMENT environment;

	if (lp_readTelemetry(&environment))
	{
		if (snprintf(msgBuffer, JSON_MESSAGE_BYTES, MsgTemplate, environment.temperature, environment.humidity, environment.pressure, environment.light, msgId++) > 0)
		{
			lp_azureMsgSendWithProperties(msgBuffer, telemetryMessageProperties, NELEMS(telemetryMessageProperties));
		}
	}
}
LP_TIMER_HANDLER_END

static LP_DEVICE_TWIN_HANDLER(SampleRateHandler, deviceTwinBinding)
{
	struct timespec sampleRateSeconds = { *(int*)deviceTwinBinding->twinState, 0 };

	if (sampleRateSeconds.tv_sec > 0 && sampleRateSeconds.tv_sec < (5 * 60))
	{
		lp_timerChange(&readSensorTimer, &sampleRateSeconds);
	}
}
LP_DEVICE_TWIN_HANDLER_END

static void InitPeripheralsAndHandlers(void)
{
	lp_initializeDevKit();

	lp_gpioSetOpen(gpioSet, NELEMS(gpioSet));
	lp_deviceTwinSetOpen(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));

	lp_timerSetStart(timerSet, NELEMS(timerSet));
	lp_azureToDeviceStart();
}

static void ClosePeripheralsAndHandlers(void)
{
	Log_Debug("Closing file descriptors\n");

	lp_timerSetStop(timerSet, NELEMS(timerSet));
	lp_azureToDeviceStop();

	lp_gpioSetClose(gpioSet, NELEMS(gpioSet));
	lp_deviceTwinSetClose();

	lp_closeDevKit();

	lp_timerEventLoopStop();
}

int main(int argc, char* argv[])
{
	lp_registerTerminationHandler();
	if (!lp_configValidate(&lp_config))
	{
		return lp_getTerminationExitCode();
	}

	InitPeripheralsAndHandlers();

    // Run main event loop. This call blocks until termination requested
	lp_eventLoopRun();

	ClosePeripheralsAndHandlers();

	Log_Debug("Application exiting.\n");
	return lp_getTerminationExitCode();
}