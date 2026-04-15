#include "platform.h"
#include "cmsis_os2.h"
#include "LedTask.h"
#include "BleTask.h"
#include <stdio.h>
#include "log.h"



extern "C" void startPlatform(void *argument)
{
    (void)argument;

  
    log_info("startPlatform begin");

    BleTask::start();
    printf("1\r\n");
    LedTask::start();
    printf("2\r\n");

    log_info("startPlatform done");

    osThreadExit();
    printf("3\r\n");
}