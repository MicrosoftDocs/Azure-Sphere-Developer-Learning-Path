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

static LP_TIMER_HANDLER(IntercoreHeartBeatHandler)
{
	ic_control_block.cmd = LP_IC_HEARTBEAT;		// Prime RT Core with Component ID Signature
	lp_interCoreSendMessage(&ic_control_block, sizeof(ic_control_block));
}
LP_TIMER_HANDLER_END


/// <summary>
/// Callback handler for Inter-Core Messaging - Does Device Twin Update, and Event Message
/// </summary>
static void InterCoreHandler(LP_INTER_CORE_BLOCK* ic_message_block)
{
	static const char* msgTemplate = "{ \"DistanceLeft\": %d, \"DistanceRight\": %d, \"MsgId\":%d }";
	static int msgId = 0;

	switch (ic_message_block->cmd)
	{
	case LP_IC_REPORT_DISTANCE:
		lp_deviceTwinReportState(&reportDistanceLeft, &ic_message_block->distance_left);	// TwinType = TYPE_INT
		lp_deviceTwinReportState(&reportDistanceRight, &ic_message_block->distance_right);	// TwinType = TYPE_INT
		break;
	default:
		break;
	}

	if (snprintf(msgBuffer, JSON_MESSAGE_BYTES, msgTemplate, ic_message_block->distance_left, ic_message_block->distance_right, msgId++))
	{
		lp_azureMsgSendWithProperties(msgBuffer, messageProperties, NELEMS(messageProperties));
	}
}

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static void InitPeripheralGpiosAndHandlers(void)
{
	//lp_openPeripheralGpioSet(peripheralGpioSet, NELEMS(peripheralGpioSet));
	lp_azureInitialize(lp_config.scopeId, NULL);
	lp_deviceTwinSetOpen(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));

	lp_timerSetStart(timerSet, NELEMS(timerSet));
	lp_azureToDeviceStart();

	lp_interCoreCommunicationsEnable(REAL_TIME_COMPONENT_ID, InterCoreHandler);  // Initialize Inter Core Communications

	ic_control_block.cmd = LP_IC_HEARTBEAT;		// Prime RT Core with Component ID Signature
	lp_interCoreSendMessage(&ic_control_block, sizeof(ic_control_block));
}

/// <summary>
/// Close PeripheralGpios and handlers.
/// </summary>
static void ClosePeripheralGpiosAndHandlers(void)
{
	Log_Debug("Closing file descriptors\n");

	lp_timerSetStop(timerSet, NELEMS(timerSet));
	lp_azureToDeviceStop();

	lp_deviceTwinSetClose();

	lp_timerEventLoopStop();
}

int main(int argc, char* argv[])
{
	lp_registerTerminationHandler();
	if (!lp_configValidate(&lp_config))
	{
		return lp_getTerminationExitCode();
	}

	InitPeripheralGpiosAndHandlers();

    // This call blocks until termination requested
	lp_eventLoopRun();

	ClosePeripheralGpiosAndHandlers();

	Log_Debug("Application exiting.\n");
	return lp_getTerminationExitCode();
}