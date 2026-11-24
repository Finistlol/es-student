#pragma once

#include <stdbool.h>
#include <stdint.h>

// Настраивает стандартный ввод-вывод и ведущий контроллер I²C0.
void platform_master_init(void);

// Записывает значение в регистр внешнего устройства по I²C.
bool platform_i2c_write(uint8_t address, uint8_t reg, uint8_t value);

// Указывает регистр и читает один байт с повторным START.
bool platform_i2c_read(uint8_t address, uint8_t reg, uint8_t *value);
