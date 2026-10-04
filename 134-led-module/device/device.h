#pragma once

#define DEVICE_PROJECT "134-led-module"
#define DEVICE_REPO "https://github.com/Finistlol/es-student"

#define DEVICE_NAME "es-led-module"
#define FIRMWARE_VERSION "1.0.0"

#ifndef DEVICE_BOARD
#define DEVICE_BOARD "unknown"
#endif

void device_info(void);