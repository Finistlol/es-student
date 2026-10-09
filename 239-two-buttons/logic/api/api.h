#pragma once

#include "stdio-text-protocol/stdio-text-protocol.h"

// Выполняет принятую команду: печатает ответ или ошибку.
// Порядок разбора: переполненный кадр, help, таблица, неизвестное имя.
void api_handle(const command_t *command);