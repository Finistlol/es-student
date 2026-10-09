#pragma once

#include <stdbool.h>
#include <stdint.h>

// Сколько аргументов максимум принимает кадр. Больше — кадр исполнять нельзя.
#define STDIO_TEXT_PROTOCOL_MAX_ARGS 4

// Сколько символов максимум вмещает кадр без конца строки.
#define STDIO_TEXT_PROTOCOL_MAX_LINE 63

// Команда, принятая из одной строки.
typedef struct
{
    const char *name;
    uint32_t argc;
    const char *argv[STDIO_TEXT_PROTOCOL_MAX_ARGS];

    // Строка длиннее буфера или аргументов больше, чем помещается в argv:
    // команда принята не целиком, выполнять её нельзя.
    bool truncated;
} command_t;

void stdio_text_protocol_init(void);

// Читает доступные символы без ожидания. Когда кадр принят целиком,
// возвращает команду; возвращённая команда действительна до следующего
// принятого кадра. Пока кадр не закончен, возвращает NULL.
const command_t *stdio_text_protocol_handle(void);