#include "api/api.h"
#include "platform/platform.h"
#include "stdio-text-protocol/stdio-text-protocol.h"

#include <stddef.h>

int main(void)
{
    platform_master_init();
    stdio_text_protocol_init();

    while (1)
    {
        const command_t *command = stdio_text_protocol_handle();
        if (command != NULL)
        {
            api_handle(command);
        }
    }
}
