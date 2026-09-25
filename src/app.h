#pragma once
#include "led_controller.h"

extern LedController led;

// Push current settings into the controller after load or a web save.
void applySettings();
