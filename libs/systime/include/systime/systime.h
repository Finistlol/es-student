#pragma once

#include <stdbool.h>
#include <stdint.h>

// Время с момента сброса.
uint64_t systime_us(void);
uint64_t systime_ms(void);

// Проверка периода для периодической задачи. Когда с момента *last_us
// прошло не меньше period_us, сдвигает метку на period_us и возвращает true.
// Отсчёт идёт от сетки периода: опоздание одного срабатывания не сдвигает
// следующие. Если отставание больше периода, пропущенные срабатывания
// не навёрстываются, отсчёт начинается от текущего момента.
bool systime_period_elapsed(uint64_t *last_us, uint64_t period_us);