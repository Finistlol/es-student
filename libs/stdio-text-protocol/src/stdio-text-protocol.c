#include "stdio-text-protocol/stdio-text-protocol.h"

#include <stdio.h>

#include "pico/stdio.h"

static char line[STDIO_TEXT_PROTOCOL_MAX_LINE + 1];
static uint32_t line_length = 0;
static bool overflowed = false;
static command_t accepted;

void stdio_text_protocol_init(void)
{
    line_length = 0;
    overflowed = false;
}

// Делит набранную строку на слова: имя и аргументы. Слова разделяют пробелы,
// пробелы в начале и в конце допустимы. argv указывает внутрь line,
// поэтому команда действительна, пока не начат следующий кадр.
static const command_t *parse(void)
{
    uint32_t argc = 0;
    uint32_t i = 0;

    while (line[i] != '\0')
    {
        while (line[i] == ' ')
        {
            line[i] = '\0';
            i = i + 1;
        }

        if (line[i] == '\0')
        {
            break;
        }

        if (argc == 0)
        {
            accepted.name = &line[i];
        }
        else if (argc <= STDIO_TEXT_PROTOCOL_MAX_ARGS)
        {
            accepted.argv[argc - 1] = &line[i];
        }

        while (line[i] != '\0' && line[i] != ' ')
        {
            i = i + 1;
        }

        argc = argc + 1;
    }

    accepted.argc = argc > 0 ? argc - 1 : 0;
    accepted.truncated = overflowed || accepted.argc > STDIO_TEXT_PROTOCOL_MAX_ARGS;

    // Строка из одних пробелов командой не считается.
    if (argc == 0)
    {
        return NULL;
    }

    return &accepted;
}

const command_t *stdio_text_protocol_handle(void)
{
    int symbol = getchar_timeout_us(0);

    if (symbol == PICO_ERROR_TIMEOUT)
    {
        return NULL;
    }

    if (symbol == '\r' || symbol == '\n')
    {
        putchar('\r');
        putchar('\n');

        line[line_length] = '\0';

        const command_t *result = NULL;
        if (line_length > 0)
        {
            result = parse();
        }

        line_length = 0;
        overflowed = false;
        return result;
    }

    if (symbol == 0x08 || symbol == 0x7f)
    {
        if (line_length > 0)
        {
            line_length = line_length - 1;
            overflowed = false;
            // Стирает символ на экране терминала.
            putchar(0x08);
            putchar(' ');
            putchar(0x08);
        }
        return NULL;
    }

    if (symbol >= 0x20 && symbol <= 0x7e)
    {
        if (line_length < STDIO_TEXT_PROTOCOL_MAX_LINE)
        {
            line[line_length] = (char)symbol;
            line_length = line_length + 1;
            putchar(symbol);
        }
        else
        {
            overflowed = true;
        }
    }

    return NULL;
}