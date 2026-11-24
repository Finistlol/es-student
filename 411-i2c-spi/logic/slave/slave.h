#pragma once

enum
{
    SLAVE_I2C_ADDRESS = 0x42,
    SLAVE_REG_CMD = 0x00,  // Только запись: 'e' (0x65) включает светодиод, 'd' (0x64) - выключает
    SLAVE_REG_STATE = 0x01 // Чтение и запись: 0 — выключен, 1 — включён
};

void slave_device_init(void);
void slave_device_handle(void);
