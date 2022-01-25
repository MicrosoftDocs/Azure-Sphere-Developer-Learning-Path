#pragma once

#include "azure_iot.h"
#include "peripheral_gpio.h"

#define LP_DIRECT_METHOD_HANDLER(name, json, directMethodBinding, responseMsg)                                         \
    LP_DIRECT_METHOD_RESPONSE_CODE name(JSON_Value *json, LP_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg) \
    {

#define LP_DIRECT_METHOD_HANDLER_END \
    }

#define LP_DECLARE_DIRECT_METHOD_HANDLER(name) \
    LP_DIRECT_METHOD_RESPONSE_CODE name(JSON_Value *json, LP_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);

typedef enum 
{
	LP_METHOD_SUCCEEDED = 200,
	LP_METHOD_FAILED = 500,
	LP_METHOD_NOT_FOUND = 404
} LP_DIRECT_METHOD_RESPONSE_CODE;

struct _directMethodBinding {
	const char* methodName;
	LP_DIRECT_METHOD_RESPONSE_CODE(*handler)(JSON_Value* json, struct _directMethodBinding* peripheral, char** responseMsg);
};

typedef struct _directMethodBinding LP_DIRECT_METHOD_BINDING;

void lp_directMethodSetClose(void);
void lp_directMethodSetOpen(LP_DIRECT_METHOD_BINDING* directMethods[], size_t directMethodCount);
int lp_directMethodHandler(const char* method_name, const unsigned char* payload, size_t payloadSize,
	unsigned char** responsePayload, size_t* responsePayloadSize, void* userContextCallback);
