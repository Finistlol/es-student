#include "logic/slave/slave.h"
#include "platform/platform.h"

#include <stdatomic.h>

// Состояние меняет обработчик I²C, а суперцикл читает его.
static _Atomic uint8_t led_state;

static enum
{
    SPI_COMMAND,
    SPI_DATA,
    SPI_DONE
} spi_phase;

static uint8_t spi_register;
static bool spi_reading;

static uint8_t read_register(uint8_t reg)
{
    if (reg == SLAVE_REG_STATE)
    {
        return atomic_load_explicit(&led_state, memory_order_relaxed);
    }

    return 0xFF;
}

static void write_register(uint8_t reg, uint8_t value)
{
    if (reg == SLAVE_REG_CMD && (value == 'e' || value == 'd'))
    {
        atomic_store_explicit(&led_state, value == 'e', memory_order_relaxed);
    }
    else if (reg == SLAVE_REG_STATE && value <= 1)
    {
        atomic_store_explicit(&led_state, value, memory_order_relaxed);
    }
}

static void handle_spi_byte(uint8_t byte)
{
    switch (spi_phase)
    {
    case SPI_COMMAND:
        spi_register = byte & 0x7F;
        spi_reading = (byte & 0x80) != 0;
        if (spi_reading)
        {
            platform_spi_slave_reply(read_register(spi_register));
        }
        else
        {
            platform_spi_slave_reply(0x00);
        }
        spi_phase = SPI_DATA;
        break;
    case SPI_DATA:
        if (!spi_reading)
        {
            write_register(spi_register, byte);
        }
        spi_phase = SPI_DONE;
        break;
    case SPI_DONE:
        break;
    }
}

void slave_device_init(void)
{
    platform_slave_init(SLAVE_I2C_ADDRESS, read_register, write_register);
}

void slave_device_handle(void)
{
    uint8_t byte;
    while (platform_spi_slave_receive(&byte))
    {
        handle_spi_byte(byte);
    }

    if (spi_phase != SPI_COMMAND && !platform_spi_slave_selected())
    {
        // Новый кадр начинается с пустых очередей, даже после обрыва.
        platform_spi_slave_reset();

        spi_phase = SPI_COMMAND;
    }

    platform_led_set(atomic_load_explicit(&led_state, memory_order_relaxed) != 0);
}
