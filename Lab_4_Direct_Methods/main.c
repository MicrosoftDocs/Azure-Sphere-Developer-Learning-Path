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
 *   This lab now use the LP_DIRECT_METHOD_HANDLER macro to define direct method handlers
 *   All application declarations are located in main.h
 *    
 *
 *   DISCLAIMER
 *   ==========
 *   The learning_path_libs functions provided in the learning_path_libs folder:
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
        Log_Debug("%s\n", msgBuffer);
        lp_azureMsgSendWithProperties(msgBuffer, telemetryMessageProperties, NELEMS(telemetryMessageProperties));
    }
}
LP_TIMER_HANDLER_END

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
/// </summary>
static LP_DIRECT_METHOD_HANDLER(RestartDeviceHandler, json, directMethodBinding, responseMsg)
{
    const size_t responseLen = 100; // Allocate and initialize a response message buffer. The calling function is responsible for the freeing memory
    static struct timespec period;

    *responseMsg = (char *)malloc(responseLen);
    memset(*responseMsg, 0, responseLen);

    if (json_value_get_type(json) != JSONNumber)
    {
        return LP_METHOD_FAILED;
    }

    int seconds = (int)json_value_get_number(json);

    // leave enough time for the device twin dt_reportedRestartUtc to update before restarting the device
    if (seconds > 2 && seconds < 10)
    {
        // Report Device Restart UTC
        lp_deviceTwinReportState(&dt_reportedRestartUtc, lp_getCurrentUtc(msgBuffer, sizeof(msgBuffer))); // LP_TYPE_STRING

        // Create Direct Method Response
        snprintf(*responseMsg, responseLen, "%s called. Restart in %d seconds", directMethodBinding->methodName, seconds);

        // Set One Shot LP_TIMER
        period = (struct timespec){.tv_sec = seconds, .tv_nsec = 0};
        lp_timerOneShotSet(&restartDeviceOneShotTimer, &period);

        return LP_METHOD_SUCCEEDED;
    }
    else
    {
        snprintf(*responseMsg, responseLen, "%s called. Restart Failed. Seconds out of range: %d", directMethodBinding->methodName, seconds);
        return LP_METHOD_FAILED;
    }
}
LP_DIRECT_METHOD_HANDLER_END

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralAndHandlers(void)
{
    lp_azureInitialize(lp_config.scopeId, IOT_PLUG_AND_PLAY_MODEL_ID);

    lp_initializeDevKit();

    lp_gpioSetOpen(gpioSet, NELEMS(gpioSet));

    lp_deviceTwinSetOpen(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));

    lp_directMethodSetOpen(directMethodBindingSet, NELEMS(directMethodBindingSet));

    lp_timerSetStart(timerSet, NELEMS(timerSet));

    lp_azureToDeviceStart();
}

/// <summary>
///     Close PeripheralGpios and handlers.
/// </summary>
static void ClosePeripheralAndHandlers(void)
{
    lp_timerSetStop(timerSet, NELEMS(timerSet));
    lp_azureToDeviceStop();

    lp_gpioSetClose(gpioSet, NELEMS(gpioSet));
    lp_deviceTwinSetClose();
    lp_directMethodSetClose();

    lp_closeDevKit();

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