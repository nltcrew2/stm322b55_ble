#include <stdio.h>
#include "cmsis_os2.h"
#include "stm32_hal.h"

// printf retarget (ITM / SWO)
int _write(int file, char *ptr, int len)
{
    (void)file;

    for (int i = 0; i < len; i++)
    {
        ITM_SendChar((uint32_t)ptr[i]);
    }

    return len;
}

const char *DbgTraceGetFileName(const char *path)
{
    const char *file = path;

    while (*path != '\0')
    {
        if (*path == '/' || *path == '\\')
        {
            file = path + 1;
        }
        path++;
    }

    return file;
}