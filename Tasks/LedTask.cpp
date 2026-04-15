#include "LedTask.h"
#include "cmsis_os2.h"
#include "main.h"
#include "log.h"

#define LED_GPIO      GPIOE
#define LED_PIN       GPIO_PIN_4
#define LED_ON_STAT   GPIO_PIN_RESET
#define LED_OFF_STAT  GPIO_PIN_SET

static void ledThread(void *argument)
{
    (void)argument;

    for (;;)
    {
        HAL_GPIO_WritePin(LED_GPIO, LED_PIN, LED_ON_STAT);
        osDelay(1000);

        HAL_GPIO_WritePin(LED_GPIO, LED_PIN, LED_OFF_STAT);
        osDelay(1000);

        log_info("led on");
    }
}

void LedTask::start()
{
    static const osThreadAttr_t ledTaskAttr = {
        .name = "ledTask",
        .stack_size = 1024,
        .priority = osPriorityNormal,
    };

    osThreadId_t tid = osThreadNew(ledThread, nullptr, &ledTaskAttr);

    if (tid == nullptr)
    {
        log_info("led task create failed\r\n");
    }
    else
    {
        log_info("led task create success\r\n");
    }
}