#include "logic/slave/slave.h"
#include "platform/platform.h"

#include <stdatomic.h>

// Состояние меняет обработчик I²C, а суперцикл читает его.
static _Atomic uint8_t led_state;

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

void slave_device_init(void)
{
    platform_slave_init(SLAVE_I2C_ADDRESS, read_register, write_register);
}

void slave_device_handle(void)
{
    platform_led_set(atomic_load_explicit(&led_state, memory_order_relaxed) != 0);
}
