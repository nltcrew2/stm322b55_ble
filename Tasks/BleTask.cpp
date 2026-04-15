#include "BleTask.h"
#include "bluetooth.h"
#include "cmsis_os2.h"
#include "log.h"

#include <cstdio>
#include <cstring>

static void bleThread(void *argument)
{
    (void)argument;

    BluetoothCore::Instance().Init();
    BluetoothCore::Instance().SetName("WB55_TEST");

    uint32_t counter = 0;

    for (;;)
    {
        if (BluetoothCore::Instance().IsConnected())
        {
            log_info("BLE connected");

            char msg[20] = {0};
            snprintf(msg, sizeof(msg), "count:%lu", (unsigned long)counter++);
            BluetoothCore::Instance().SendText(msg);

            if (BluetoothCore::Instance().HasNewRxData())
            {
                uint8_t rxBuf[21] = {0};
                uint8_t rxLen = BluetoothCore::Instance().GetRxData(rxBuf, 20);
                rxBuf[rxLen] = '\0';

                log_info("RX data len=%d", rxLen);
                log_info("RX text=%s", rxBuf);

                BluetoothCore::Instance().Send(rxBuf, rxLen);
            }
        }
        else
        {
            log_info("BLE not connected");
        }

        osDelay(1000);
    }
}

void BleTask::start()
{
    static const osThreadAttr_t bleTaskAttr = {
        .name       = "bleTask",
        .stack_size = 2048,
        .priority   = osPriorityNormal,
    };

    osThreadId_t tid = osThreadNew(bleThread, nullptr, &bleTaskAttr);

    if (tid == nullptr)
    {
        log_info("ble task create failed\r\n");
    }
    else
    {
        log_info("ble task create success\r\n");
    }
}