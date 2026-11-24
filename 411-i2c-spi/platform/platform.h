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

// Запускает функцию на ядре 1.
void platform_start_core1(void (*entry)(void));

// Функция чтения регистра ведомого устройства.
typedef uint8_t (*platform_register_read_t)(uint8_t reg);

// Функция записи регистра ведомого устройства.
typedef void (*platform_register_write_t)(uint8_t reg, uint8_t value);

// Настраивает периферию ведомого: светодиод и I²C1 на заданном адресе.
void platform_slave_init(uint8_t address, platform_register_read_t read_register,
                         platform_register_write_t write_register);

// Управляет светодиодом со стороны ведомого.
void platform_led_set(bool on);

// Готовит SPI1 к приёму нового кадра ведомым.
void platform_spi_slave_reset(void);

// Истинно, пока ведущий держит CS низким.
bool platform_spi_slave_selected(void);

// Забирает байт из очереди приёма SPI1, если он есть.
bool platform_spi_slave_receive(uint8_t *byte);

// Кладёт байт ответа в очередь передачи SPI1.
void platform_spi_slave_reply(uint8_t byte);
