#pragma once

#include <stdbool.h>
#include <stdint.h>

enum
{
    PLATFORM_SPI_CS_COUNT = 2,
};

// Настраивает стандартный ввод-вывод и ведущие контроллеры I²C0 и SPI0.
void platform_master_init(void);

// Записывает значение в регистр внешнего устройства по I²C.
bool platform_i2c_write(uint8_t address, uint8_t reg, uint8_t value);

// Указывает регистр и читает один байт с повторным START.
bool platform_i2c_read(uint8_t address, uint8_t reg, uint8_t *value);

// Передаёт управляющий байт и значение по SPI под сигналом выбора cs.
void platform_spi_write(uint8_t cs, uint8_t command, uint8_t value);

// Передаёт управляющий байт и принимает ответ; пауза между байтами в мкс.
uint8_t platform_spi_read(uint8_t cs, uint8_t command, uint8_t pause_us);
