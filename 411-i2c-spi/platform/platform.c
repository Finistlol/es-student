#include "platform/platform.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

enum
{
    BUS_BAUD = 100000,
    I2C_TIMEOUT_US = 10000,
};

void platform_master_init(void)
{
    stdio_init_all();

    i2c_init(i2c0, BUS_BAUD);
    gpio_disable_pulls(8);
    gpio_set_function(8, GPIO_FUNC_I2C);   // SDA
    gpio_disable_pulls(9);
    gpio_set_function(9, GPIO_FUNC_I2C);   // SCL
}

bool platform_i2c_write(uint8_t address, uint8_t reg, uint8_t value)
{
    const uint8_t bytes[] = {reg, value};
    return i2c_write_timeout_us(i2c0, address, bytes, sizeof(bytes), false,
                                I2C_TIMEOUT_US) == sizeof(bytes);
}

bool platform_i2c_read(uint8_t address, uint8_t reg, uint8_t *value)
{
    if (i2c_write_timeout_us(i2c0, address, &reg, 1, true, I2C_TIMEOUT_US) != 1)
    {
        return false;
    }

    return i2c_read_timeout_us(i2c0, address, value, 1, false, I2C_TIMEOUT_US) == 1;
}
