#pragma once

/**
 * oled-lcd-icons 12x12 bitmaps (https://github.com/simonmetcalfe/oled-lcd-icons).
 *
 * ESPHome copies the oli_icons component to esphome/components/oli_icons/; PlatformIO and host
 * builds put its folder on the include path instead.
 */

#if __has_include("esphome/components/oli_icons/icons_12x12.h")
#include "esphome/components/oli_icons/icons_12x12.h"
#else
#include <icons_12x12.h>
#endif
