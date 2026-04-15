#include "OledDisplayTask.h"
#include "cmsis_os2.h"
#include "main.h"
#include <log.h>

extern "C" {
#include "u8g2.h"
}

// main.c 또는 stm32l4xx_hal_msp 쪽에 생성된 핸들 참조

extern I2C_HandleTypeDef hi2c1;

// u8g2 포팅 함수
extern "C" uint8_t u8x8_stm32_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
// extern "C" uint8_t u8x8_byte_stm32_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
extern "C" uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

static u8g2_t g_u8g2;

static void OledDisplayThread(void *argument)
{
    (void)argument;

    // -----------------------------
    // 1) u8g2 초기화
    // -----------------------------

    // SPI 사용 시
    // u8g2_Setup_ssd1306_128x64_noname_f(
    //     &g_u8g2,
    //     U8G2_R0,
    //     u8x8_byte_stm32_hw_spi,
    //     u8x8_stm32_gpio_and_delay
    // );

    // I2C 사용 시에는 위 대신 아래 사용
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &g_u8g2,
        U8G2_R0,
        u8x8_byte_stm32_hw_i2c,
        u8x8_stm32_gpio_and_delay
    );

    u8g2_InitDisplay(&g_u8g2);
    u8g2_SetPowerSave(&g_u8g2, 0);

    log_info("OLED initialized");

    // -----------------------------
    // 2) 반복 출력
    // -----------------------------
    for (;;)
    {
        u8g2_ClearBuffer(&g_u8g2);

        u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);
        u8g2_DrawStr(&g_u8g2, 0, 15, "Hello OLED");

        u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tr);
        u8g2_DrawStr(&g_u8g2, 0, 32, "running in task");

        u8g2_SendBuffer(&g_u8g2);

        log_info("hello oled");

        osDelay(500);
    }
}

void OledDisplayTask::start()
{
    static const osThreadAttr_t OledDisplayTaskAttr = {
        .name = "OledDisplayTask",
        .stack_size = 2048,
        .priority = (osPriority_t)osPriorityNormal
    };

    osThreadId_t tid = osThreadNew(OledDisplayThread, nullptr, &OledDisplayTaskAttr);
    (void)tid;
}