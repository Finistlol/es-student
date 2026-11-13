#pragma once

#include <stdbool.h>
#include <stdint.h>

#define UART_TASK_MAX_TEXT 32
#define UART_TASK_BLOCK_SIZE 64

// Результат обмена: принятая строка и признаки ошибок UARTRSR.
typedef struct
{
    char text[UART_TASK_MAX_TEXT + 1];
    uint8_t length;
    uint32_t errors;
} uart_result_t;

// Результат блочного обмена.
typedef struct
{
    uint8_t match_count;
    uint8_t total;
    uint32_t errors;
    bool irq_mode;
} uart_block_result_t;

// Функция, которой задача отдаёт принятую строку.
typedef void (*uart_task_callback_t)(const uart_result_t *result);

// Функция, которой задача отдаёт результат блока.
typedef void (*uart_block_callback_t)(const uart_block_result_t *result);

void uart_task_init(uart_task_callback_t callback,
                    uart_block_callback_t block_callback);
bool uart_task_echo(const char *text, uint8_t length);
bool uart_task_link(const char *text, uint8_t length);
bool uart_task_text(void);
bool uart_task_block(bool irq_mode);
void uart_task_handle(void);