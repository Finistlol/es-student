#pragma once

#include <stdbool.h>
#include <stdint.h>

#define UART_TASK_MAX_TEXT 32

// Результат обмена: принятая строка и признаки ошибок UARTRSR.
typedef struct
{
    char text[UART_TASK_MAX_TEXT + 1];
    uint8_t length;
    uint32_t errors;
} uart_result_t;

// Функция, которой задача отдаёт принятую строку.
typedef void (*uart_task_callback_t)(const uart_result_t *result);

void uart_task_init(uart_task_callback_t callback);
bool uart_task_echo(const char *text, uint8_t length);
void uart_task_handle(void);