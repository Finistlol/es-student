#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include "pico.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/regs/addressmap.h"
#include "led.h"
#include "command.h"
#include "device.h"

// Символы линкера: значения у них нет, есть только адрес — он берётся оператором &.
extern char __flash_binary_start[];
extern char __flash_binary_end[];
extern char __boot2_start__[];
extern char __boot2_end__[];
extern char __etext[];
extern char __data_start__[];
extern char __data_end__[];
extern char __bss_start__[];
extern char __bss_end__[];
extern char __HeapLimit[];
extern char __StackTop[];

#define SRAM_END 0x20042000u
#define ROM_SIZE 0x4000u
#define VECTOR_TABLE 0x10000100u
#define GPIO_IN_ADDR 0xd0000004u

int main(void);

// Начальное значение ненулевое: переменная попадает в .data.
uint32_t data_variable = 101;
// Начального значения нет: переменная попадает в .bss.
uint32_t bss_variable;

static void row(const char *name, uintptr_t start, uintptr_t end)
{
    printf("%-10s 0x%08x 0x%08x %8u\n",
           name, (unsigned)start, (unsigned)end, (unsigned)(end - start));
}

void mem_info(void)
{
    uintptr_t flash_end = XIP_BASE + PICO_FLASH_SIZE_BYTES;
    uintptr_t data_size = (uintptr_t)&__data_end__ - (uintptr_t)&__data_start__;

    printf("area       start      end        size\n");
    row("flash", XIP_BASE, flash_end);
    row("sram", SRAM_BASE, SRAM_END);
    row("rom", 0x00000000u, ROM_SIZE);
    row("image", (uintptr_t)&__flash_binary_start, (uintptr_t)&__flash_binary_end);
    row("free", (uintptr_t)&__flash_binary_end, flash_end);
    row("boot2", (uintptr_t)&__boot2_start__, (uintptr_t)&__boot2_end__);
    row("text", (uintptr_t)&__boot2_end__, (uintptr_t)&__etext);
    row("data flash", (uintptr_t)&__etext, (uintptr_t)&__etext + data_size);
    row("data ram", (uintptr_t)&__data_start__, (uintptr_t)&__data_end__);
    row("bss", (uintptr_t)&__bss_start__, (uintptr_t)&__bss_end__);
    row("heap", (uintptr_t)&__bss_end__, (uintptr_t)&__HeapLimit);
    row("stack", (uintptr_t)&__StackTop - 2048u, (uintptr_t)&__StackTop);

    printf("\n");
    printf("total\n");
    printf("  flash image %8u = boot2 %u + text %u + data %u\n",
           (unsigned)((uintptr_t)&__flash_binary_end - (uintptr_t)&__flash_binary_start),
           (unsigned)((uintptr_t)&__boot2_end__ - (uintptr_t)&__boot2_start__),
           (unsigned)((uintptr_t)&__etext - (uintptr_t)&__boot2_end__),
           (unsigned)data_size);
    printf("  flash free  %8u of %u\n",
           (unsigned)(flash_end - (uintptr_t)&__flash_binary_end),
           (unsigned)PICO_FLASH_SIZE_BYTES);
    printf("  ram used    %8u = data %u + bss %u\n",
           (unsigned)((uintptr_t)&__data_end__ - (uintptr_t)&__data_start__
                      + (uintptr_t)&__bss_end__ - (uintptr_t)&__bss_start__),
           (unsigned)data_size,
           (unsigned)((uintptr_t)&__bss_end__ - (uintptr_t)&__bss_start__));
    printf("  ram free    %8u for heap and %u for stack\n",
           (unsigned)((uintptr_t)&__HeapLimit - (uintptr_t)&__bss_end__),
           2048u);
}

void fw_info(void)
{
    data_variable = data_variable + 1;
    bss_variable = bss_variable + 1;

    uintptr_t main_addr = (uintptr_t)&main;
    uintptr_t fw_addr = (uintptr_t)&fw_info;
    uint16_t main_first = *(uint16_t *)(main_addr & ~1u);
    uint16_t fw_first = *(uint16_t *)(fw_addr & ~1u);

    int stack_variable = 1946;

    uint32_t *heap_variable = malloc(sizeof(uint32_t));
    if (heap_variable == NULL)
    {
        printf("malloc failed\n");
        return;
    }
    *heap_variable = data_variable + bss_variable;

    printf("object          address     value\n");
    printf("%-15s 0x%08x 0x%04x\n", "main", (unsigned)main_addr, main_first);
    printf("%-15s 0x%08x 0x%04x\n", "fw_info", (unsigned)fw_addr, fw_first);
    printf("%-15s 0x%08x\n", "commands", (unsigned)(uintptr_t)commands);
    for (uint i = 0; i < command_count; i++)
    {
        printf("- %-13s 0x%08x\n", commands[i].name, (unsigned)(uintptr_t)commands[i].handler);
    }
    printf("%-15s 0x%08x %s\n", "DEVICE_PROJECT", (unsigned)(uintptr_t)DEVICE_PROJECT, DEVICE_PROJECT);
    printf("%-15s 0x%08x %s\n", "DEVICE_BOARD", (unsigned)(uintptr_t)DEVICE_BOARD, DEVICE_BOARD);
    printf("%-15s 0x%08x %u\n", "data_variable", (unsigned)(uintptr_t)&data_variable, data_variable);
    printf("%-15s 0x%08x %u\n", "bss_variable", (unsigned)(uintptr_t)&bss_variable, bss_variable);
    printf("%-15s 0x%08x %d\n", "stack_variable", (unsigned)(uintptr_t)&stack_variable, stack_variable);
    printf("%-15s 0x%08x %u\n", "heap_variable", (unsigned)(uintptr_t)heap_variable, *heap_variable);

    free(heap_variable);
}

void boot_info(void)
{
    uint32_t *vectors = (uint32_t *)VECTOR_TABLE;
    volatile uint32_t *gpio_in = (volatile uint32_t *)GPIO_IN_ADDR;
    uint32_t level = (*gpio_in >> led_pin()) & 1u;

    printf("vector table   0x%08x\n", (unsigned)VECTOR_TABLE);
    printf("  stack top    0x%08x\n", (unsigned)vectors[0]);
    printf("  reset        0x%08x\n", (unsigned)vectors[1]);
    printf("  reset (even) 0x%08x\n", (unsigned)(vectors[1] & ~1u));
    printf("gpio in        0x%08x\n", (unsigned)GPIO_IN_ADDR);
    printf("  led bit      %u\n", (unsigned)level);
    printf("  gpio_get     %u\n", gpio_get(led_pin()) ? 1u : 0u);
}