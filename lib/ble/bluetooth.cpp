#include "bluetooth.h"
#include "main.h"

#include <cassert>
#include <cstring>

PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t bleCmdBuffer;

uint16_t gap_service_handle = 0;
uint16_t gap_dev_name_char_handle = 0;
uint16_t gap_appearance_char_handle = 0;

/* =========================
 * C linkage (STM BLE stack)
 * ========================= */
extern "C" {

void APP_BLE_Init(void)
{
    BluetoothCore::Instance().Init();
}

void hci_notify_asynch_evt(void *p_Data)
{
    UNUSED(p_Data);
    BluetoothCore::Instance().NotifyAsyncEvent();
}

void hci_cmd_resp_release(uint32_t Flag)
{
    UNUSED(Flag);
    BluetoothCore::Instance().ReleaseCmdSemaphore();
}

void hci_cmd_resp_wait(uint32_t Timeout)
{
    UNUSED(Timeout);
    BluetoothCore::Instance().WaitCmdSemaphore();
}

SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void *p_Pckt)
{
    return BluetoothCore::Instance().HandleSvcEvent(p_Pckt);
}

} // extern "C"

/* =========================
 * Singleton
 * ========================= */
BluetoothCore &BluetoothCore::Instance()
{
    static BluetoothCore instance;
    return instance;
}

/* =========================
 * Init
 * ========================= */
void BluetoothCore::Init()
{
    if (ready) return;

    log_info("BluetoothCore init start");

    SetName(deviceName);

    TransportLayerInit();
    HciEventThreadInit();
    Cpu2BleStackInit();
    GeneralConfigInit();
    GattGapInit();
    SVCCTL_Init();
    InitCustomService();

    StartAdvertising();

    ready = true;

    log_info("BluetoothCore init done");
}

/* =========================
 * Transport Layer
 * ========================= */
void BluetoothCore::TransportLayerInit()
{
    HCI_TL_HciInitConf_t hciConfig = {};

    mutexHciId     = osMutexNew(nullptr);
    mutexBleId     = osMutexNew(nullptr);
    semaphoreHciId = osSemaphoreNew(1, 0, nullptr);

    assert(mutexHciId);
    assert(mutexBleId);
    assert(semaphoreHciId);

    hciConfig.p_cmdbuffer       = reinterpret_cast<uint8_t*>(&bleCmdBuffer);
    hciConfig.StatusNotCallBack = HciStatusCallback;

    hci_init(BridgeEventCallback, reinterpret_cast<void*>(&hciConfig));
}

/* =========================
 * HCI Thread
 * ========================= */
void BluetoothCore::HciEventThreadInit()
{
    const osThreadAttr_t attr = {
        .name       = "HciEvtThread",
        .stack_size = 1024,
        .priority   = osPriorityNormal,
    };

    hciUserEventProcessThreadId =
        osThreadNew(HciUserEventProcessThread, nullptr, &attr);

    assert(hciUserEventProcessThreadId);
}

/* =========================
 * CPU2 BLE Stack
 * ========================= */
void BluetoothCore::Cpu2BleStackInit()
{
    SHCI_C2_Ble_Init_Cmd_Packet_t cmd = {};
    auto *p = &cmd.Param;

    p->NumAttrRecord              = 68;
    p->NumAttrServ                = 8;
    p->AttrValueArrSize           = 1344;
    p->NumOfLinks                 = 2;
    p->ExtendedPacketLengthEnable = true;
    p->PrWriteListSize            = BLE_PREP_WRITE_X_ATT(245);
    p->MblockCount                = BLE_MBLOCKS_CALC(BLE_PREP_WRITE_X_ATT(245), 245, 2);
    p->AttMtu                     = 245;

    p->SlaveSca  = 500;
    p->MasterSca = 0;

    p->MaxConnEventLength = 0xFFFFFFFF;
    p->HsStartupTime      = 0x148;

    p->ViterbiEnable = true;
    p->Options       = (1 << 7);

    p->max_adv_set_nbr = 3;
    p->max_adv_data_len = 1650;

    p->ble_core_version = SHCI_C2_BLE_INIT_BLE_CORE_5_3;

    SHCI_CmdStatus_t status = SHCI_C2_BLE_Init(&cmd);
    assert(status == SHCI_Success);

    log_info("CPU2 BLE stack init success");
}

/* =========================
 * General Config
 * ========================= */
void BluetoothCore::GeneralConfigInit()
{
    tBleStatus ret = hci_reset();
    assert(ret == BLE_STATUS_SUCCESS);

    UpdateDeviceAddress();

    ret = aci_hal_write_config_data(
        CONFIG_DATA_PUBADDR_OFFSET,
        CONFIG_DATA_PUBADDR_LEN,
        deviceAddress
    );
    assert(ret == BLE_STATUS_SUCCESS);

    log_info("BLE address: %02X:%02X:%02X:%02X:%02X:%02X",
        deviceAddress[5], deviceAddress[4], deviceAddress[3],
        deviceAddress[2], deviceAddress[1], deviceAddress[0]);
}

/* =========================
 * GATT + GAP
 * ========================= */
void BluetoothCore::GattGapInit()
{
    tBleStatus ret;

    ret = aci_gatt_init();
    assert(ret == BLE_STATUS_SUCCESS);

    ret = aci_gap_init(
        GAP_PERIPHERAL_ROLE,
        PRIVACY_DISABLED,
        strlen(deviceName),
        &gap_service_handle,
        &gap_dev_name_char_handle,
        &gap_appearance_char_handle
    );
    assert(ret == BLE_STATUS_SUCCESS);

    /* Device Name */
    ret = aci_gatt_update_char_value(
        gap_service_handle,
        gap_dev_name_char_handle,
        0,
        strlen(deviceName),
        (uint8_t*)deviceName
    );
    assert(ret == BLE_STATUS_SUCCESS);

    /* Appearance */
    uint16_t appearance = 0x0000;
    ret = aci_gatt_update_char_value(
        gap_service_handle,
        gap_appearance_char_handle,
        0,
        sizeof(appearance),
        (uint8_t*)&appearance
    );
    assert(ret == BLE_STATUS_SUCCESS);

    ret = aci_gap_set_io_capability(IO_CAP_NO_INPUT_NO_OUTPUT);
    assert(ret == BLE_STATUS_SUCCESS);

    log_info("GATT/GAP init success, name=%s", deviceName);
}

/* =========================
 * Advertising
 * ========================= */
void BluetoothCore::StartAdvertising()
{
    osMutexAcquire(mutexBleId, osWaitForever);

    tBleStatus ret = aci_gap_set_discoverable(
        ADV_IND,
        0x80,
        0xA0,
        GAP_PUBLIC_ADDR,
        NO_WHITE_LIST_USE,
        strlen(deviceName) + 1,
        advName,
        0,
        nullptr,
        0,
        0
    );

    osMutexRelease(mutexBleId);

    if (ret == BLE_STATUS_SUCCESS)
        log_info("Advertising started: %s", deviceName);
    else
        log_error("Advertising failed: 0x%02X", ret);
}

/* =========================
 * Name
 * ========================= */
void BluetoothCore::SetName(const char *newName)
{
    if (!newName) return;

    memset(deviceName, 0, sizeof(deviceName));
    strncpy(deviceName, newName, kNameMaxLength);

    /* Advertising payload */
    memset(advName, 0, sizeof(advName));
    advName[0] = AD_TYPE_COMPLETE_LOCAL_NAME;
    memcpy(&advName[1], deviceName, strlen(deviceName));

    if (!ready) return;

    osMutexAcquire(mutexBleId, osWaitForever);

    tBleStatus ret = aci_gatt_update_char_value(
        gap_service_handle,
        gap_dev_name_char_handle,
        0,
        strlen(deviceName),
        (uint8_t*)deviceName
    );

    osMutexRelease(mutexBleId);

    if (ret != BLE_STATUS_SUCCESS)
        log_error("SetName failed: 0x%02X", ret);

    if (!connected)
    {
        StopAdvertising();
        StartAdvertising();
    }
}