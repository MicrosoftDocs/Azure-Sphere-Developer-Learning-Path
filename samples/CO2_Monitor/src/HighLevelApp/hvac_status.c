#include "hvac_status.h"

enum LEDS { RED, GREEN, BLUE, UNKNOWN };
static enum LEDS current_led = UNKNOWN;

static DX_GPIO_BINDING* ledRgb[] = {
	&(DX_GPIO_BINDING) { .pin = LED_RED, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true,.name = "red led" },
	&(DX_GPIO_BINDING) {.pin = LED_GREEN, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "green led" },
	&(DX_GPIO_BINDING) {.pin = LED_BLUE, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "blue led" }
};

static bool rgb_initialized = false;

static inline bool InRange(int low, int high, int x) {
	return  low <= x && x <= high;
}

/// <summary>
/// Set the temperature status led. 
/// Red if HVAC needs to be turned on to get to desired temperature. 
/// Blue to turn on cooler. 
/// Green equals just right, no action required.
/// </summary>
void SetHvacStatusColour(int32_t temperature) {
	static enum LEDS previous_led = UNKNOWN;

	if (!rgb_initialized) {
		rgb_initialized = true;
		dx_gpioSetOpen(ledRgb, NELEMS(ledRgb));
	}

	// desired temperature device twin not yet updated
	if (!desiredTemperature.twinStateUpdated) { return; }

	int desired_temperature = *(int*)desiredTemperature.twinState;

	current_led = InRange(desired_temperature - 1, desired_temperature + 1, temperature)
		? GREEN : temperature > desired_temperature + 1 ? BLUE : RED;

	if (previous_led != current_led) {
		if (previous_led != UNKNOWN) {
			dx_gpioOff(ledRgb[(int)previous_led]); // turn off old current colour
		}
		previous_led = current_led;
	}
	dx_gpioOn(ledRgb[(int)current_led]);
}