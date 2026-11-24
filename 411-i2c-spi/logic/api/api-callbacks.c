#include "api/api-commands.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

static bool read_byte(const char *text, uint8_t *byte)
{
    if (strlen(text) != 4 || text[0] != '0' || text[1] != 'x')
    {
        return false;
    }

    int high = hex_digit(text[2]);
    int low = hex_digit(text[3]);
    if (high < 0 || low < 0)
    {
        return false;
    }

    *byte = (uint8_t)(high * 16 + low);
    return true;
}

static bool read_address(const char *text, uint8_t *address)
{
    return read_byte(text, address) && *address >= 0x08 && *address <= 0x77;
}

static void cmd_i2c_read_reg(const command_t *command)
{
    uint8_t address, reg;
    if (command->argc != 2 || !read_address(command->argv[0], &address) ||
        !read_byte(command->argv[1], &reg))
    {
        puts("error: usage i2c_read_reg <address> <register>");
        return;
    }

    uint8_t value;
    if (!platform_i2c_read(address, reg, &value))
    {
        puts("error: i2c transfer");
        return;
    }

    printf("i2c: addr 0x%02X reg 0x%02X value 0x%02X\n", address, reg, value);
}

static void cmd_i2c_write_reg(const command_t *command)
{
    uint8_t address, reg, value;
    if (command->argc != 3 || !read_address(command->argv[0], &address) ||
        !read_byte(command->argv[1], &reg) || !read_byte(command->argv[2], &value))
    {
        puts("error: usage i2c_write_reg <address> <register> <value>");
        return;
    }

    if (!platform_i2c_write(address, reg, value))
    {
        puts("error: i2c transfer");
        return;
    }

    printf("i2c: addr 0x%02X reg 0x%02X sent 0x%02X\n", address, reg, value);
}

const api_command_t api_commands[] = {
    {"i2c_write_reg", "<address> <register> <value>", cmd_i2c_write_reg},
    {"i2c_read_reg", "<address> <register>", cmd_i2c_read_reg},
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);
