#include "platform.h"
#include "cmsis_os2.h"
#include "WhoAmITask.h"
#include "LedTask.h"
#include <stdio.h>

volatile int g_platform_entered = 0;

extern "C" void startPlatform(void *argument)
{
    (void)argument;

    g_platform_entered = 1;
    printf("startPlatform begin\r\n");

    WhoAmITask::start();
    LedTask::start();

    printf("startPlatform done\r\n");

    osThreadExit();
}