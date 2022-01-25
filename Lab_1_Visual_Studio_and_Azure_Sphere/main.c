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
static LP_TIMER_HANDLER(NetworkConnectionStatusHandler)
{
	static bool toggleConnectionStatusLed = true;

	if (lp_isNetworkReady())
	{
		lp_gpioStateSet(&networkConnectedLed, toggleConnectionStatusLed);
		toggleConnectionStatusLed = !toggleConnectionStatusLed;
	}
	else
	{
		lp_gpioStateSet(&networkConnectedLed, false);
	}
}
LP_TIMER_HANDLER_END

/// <summary>
/// Read sensor and send to Azure IoT
/// </summary>
static LP_TIMER_HANDLER(MeasureSensorHandler)
{
	static int msgId = 0;
	static LP_ENVIRONMENT environment;

	if (lp_readTelemetry(&environment) &&
		snprintf(msgBuffer, JSON_MESSAGE_BYTES, msgTemplate,
				 environment.temperature, environment.humidity, environment.pressure, msgId++) > 0)
	{
		Log_Debug(msgBuffer);
	}
}
LP_TIMER_HANDLER_END

/// <summary>
/// Handler to check for Button Presses
/// </summary>
static LP_TIMER_HANDLER(ButtonPressCheckHandler)
{
	static GPIO_Value_Type buttonAState;

	if (lp_gpioStateGet(&buttonA, &buttonAState))
	{
		lp_gpioOn(&alertLed);
		lp_timerOneShotSet(&alertLedOffOneShotTimer, &(struct timespec){1, 0});
	}
}
LP_TIMER_HANDLER_END

/// <summary>
/// One shot timer handler to turn off Alert LED
/// </summary>
static LP_TIMER_HANDLER(AlertLedOffToggleHandler)
{
	lp_gpioOff(&alertLed);
}
LP_TIMER_HANDLER_END

/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
	lp_initializeDevKit();

	lp_gpioSetOpen(gpioSet, NELEMS(gpioSet));
	lp_timerSetStart(timerSet, NELEMS(timerSet));
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
	Log_Debug("Closing file descriptors\n");

	lp_timerSetStop(timerSet, NELEMS(timerSet));
	lp_gpioSetClose(gpioSet, NELEMS(gpioSet));

	lp_closeDevKit();

	lp_timerEventLoopStop();
}

int main(int argc, char *argv[])
{
	lp_registerTerminationHandler();

	InitPeripheralsAndHandlers();

    // This call blocks until termination requested
	lp_eventLoopRun();

	ClosePeripheralsAndHandlers();

	Log_Debug("Application exiting.\n");
	return lp_getTerminationExitCode();
}