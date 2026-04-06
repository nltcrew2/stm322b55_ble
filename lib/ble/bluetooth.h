#ifndef BLUETOOTH_CORE_H
#define BLUETOOTH_CORE_H

#include "app_ble.h"
#include "ble.h"
#include "cmsis_os.h"
#include "hci_tl.h"
#include "log.h"
#include "main.h"
#include "shci.h"
#include "svc_ctl.h"
#include "tl.h"

#include <cstdint>
#include <cstring>

extern "C" {

void APP_BLE_Init(void);
void hci_notify_asynch_evt(void *p_Data);
void hci_cmd_resp_release(uint32_t Flag);
void hci_cmd_resp_wait(uint32_t Timeout);
SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void *p_Pckt);

extern uint16_t gap_service_handle;
extern uint16_t gap_dev_name_char_handle;
extern uint16_t gap_appearance_char_handle;

} // extern "C"

class BluetoothCore
{
public:
    static BluetoothCore& Instance();

    BluetoothCore(const BluetoothCore&) = delete;
    BluetoothCore& operator=(const BluetoothCore&) = delete;

    void Init();

    void StartAdvertising();
    void StopAdvertising();
    void Disconnect();

    bool IsReady() const;
    bool IsConnected() const;

    const char* GetName() const;
    void SetName(const char* newName);

    uint16_t ConnectionHandle() const;

    bool Send(const uint8_t* data, uint8_t len);
    bool SendText(const char* text);

    bool HasNewRxData() const;
    uint8_t GetRxData(uint8_t* out, uint8_t maxLen);
    void ClearRxData();

    static void NotifyAsyncEvent();
    static void ReleaseCmdSemaphore();
    static void WaitCmdSemaphore();
    static SVCCTL_UserEvtFlowStatus_t HandleSvcEvent(void* p_Pckt);

private:
    BluetoothCore() = default;

    void TransportLayerInit();
    void HciEventThreadInit();
    void Cpu2BleStackInit();
    void GeneralConfigInit();
    void GattGapInit();
    void InitCustomService();
    void UpdateDeviceAddress();

    static void HciUserEventProcessThread(void* argument);
    static void HciStatusCallback(HCI_TL_CmdStatus_t status);
    static void BridgeEventCallback(void* payload);
    static void EventCallback(void* packet);

private:
    static constexpr uint32_t kHciEventNotifyFlag = 0x00000001;
    static constexpr size_t   kNameMaxLength      = 16;
    static constexpr uint8_t  kMaxCharValueLength = 20;

    inline static osMutexId_t     mutexHciId = nullptr;
    inline static osMutexId_t     mutexBleId = nullptr;
    inline static osSemaphoreId_t semaphoreHciId = nullptr;
    inline static osThreadId_t    hciUserEventProcessThreadId = nullptr;

    inline static bool     ready = false;
    inline static bool     connected = false;
    inline static uint16_t connectionHandle = 0;

    inline static uint8_t deviceAddress[6] = {0};

    inline static char deviceName[kNameMaxLength + 1] = "STM32WB_BLE";
    inline static uint8_t advName[kNameMaxLength + 2] = {0};

    inline static uint16_t customServiceHandle = 0;
    inline static uint16_t customTxCharHandle  = 0;
    inline static uint16_t customRxCharHandle  = 0;

    inline static uint8_t rxBuffer[kMaxCharValueLength] = {0};
    inline static uint8_t rxLength = 0;
    inline static bool    rxUpdated = false;
};

#endif // BLUETOOTH_CORE_H