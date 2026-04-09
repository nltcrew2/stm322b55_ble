#include "bluetooth.h"

#include "main.h"

#include <cassert>
#include <cstring>

PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t bleCmdBuffer;

uint16_t gap_service_handle = 0;
uint16_t gap_dev_name_char_handle = 0;
uint16_t gap_appearance_char_handle = 0;

extern "C"
{

void APP_BLE_Init(void)
{
    BluetoothCore::Instance().Init();
}

void hci_notify_asynch_evt(void *p_Data)
{
    UNUSED(p_Data);
    BluetoothCore::NotifyAsyncEvent();
}

void hci_cmd_resp_release(uint32_t Flag)
{
    UNUSED(Flag);
    BluetoothCore::ReleaseCmdSemaphore();
}

void hci_cmd_resp_wait(uint32_t Timeout)
{
    UNUSED(Timeout);
    BluetoothCore::WaitCmdSemaphore();
}

SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void *p_Pckt)
{
    return BluetoothCore::HandleSvcEvent(p_Pckt);
}

} // extern "C"

BluetoothCore &BluetoothCore::Instance()
{
    static BluetoothCore instance;
    return instance;
}

void BluetoothCore::Init()
{
    if (ready)
    {
        return;
    }

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

void BluetoothCore::TransportLayerInit()
{
    HCI_TL_HciInitConf_t hciConfig = {};

    mutexHciId = osMutexNew(nullptr);
    mutexBleId = osMutexNew(nullptr);
    semaphoreHciId = osSemaphoreNew(1, 0, nullptr);

    assert(mutexHciId != nullptr);
    assert(mutexBleId != nullptr);
    assert(semaphoreHciId != nullptr);

    hciConfig.p_cmdbuffer = reinterpret_cast<uint8_t *>(&bleCmdBuffer);
    hciConfig.StatusNotCallBack = HciStatusCallback;

    hci_init(BridgeEventCallback, reinterpret_cast<void *>(&hciConfig));
}

void BluetoothCore::HciEventThreadInit()
{
    const osThreadAttr_t attr = {
        .name = "HciEvtThread",
        .stack_size = 1024,
        .priority = osPriorityNormal,
    };

    hciUserEventProcessThreadId = osThreadNew(HciUserEventProcessThread, nullptr, &attr);
    assert(hciUserEventProcessThreadId != nullptr);
}

void BluetoothCore::Cpu2BleStackInit()
{
    SHCI_C2_Ble_Init_Cmd_Packet_t bleInitCmdPacket = {};
    auto *param = &bleInitCmdPacket.Param;

    param->NumAttrRecord = 68;
    param->NumAttrServ = 8;
    param->AttrValueArrSize = 1344;
    param->NumOfLinks = 2;
    param->ExtendedPacketLengthEnable = true;
    param->PrWriteListSize = BLE_PREP_WRITE_X_ATT(245);
    param->MblockCount = BLE_MBLOCKS_CALC(BLE_PREP_WRITE_X_ATT(245), 245, 2);
    param->AttMtu = 245;
    param->PeripheralSca = 500;  // peripheral sleep clock accuracy (ppm)
    param->CentralSca    = 0;    // 251~500 ppm
    param->LsSource = 0;
    param->MaxConnEventLength = 0xFFFFFFFF;
    param->HsStartupTime = 0x148;
    param->ViterbiEnable = true;
    param->Options = (1 << 7);
    param->HwVersion = 0;
    param->max_coc_initiator_nbr = 32;
    param->min_tx_power = -40;
    param->max_tx_power = 6;
    param->rx_model_config = SHCI_C2_BLE_INIT_RX_MODEL_AGC_RSSI_LEGACY;
    param->max_adv_set_nbr = 3;
    param->max_adv_data_len = 1650;
    param->tx_path_compens = 0;
    param->rx_path_compens = 0;
    param->ble_core_version = SHCI_C2_BLE_INIT_BLE_CORE_5_3;
    param->Options_extension =
        SHCI_C2_BLE_INIT_OPTIONS_APPEARANCE_READONLY |
        SHCI_C2_BLE_INIT_OPTIONS_ENHANCED_ATT_NOTSUPPORTED;

    SHCI_CmdStatus_t status = SHCI_C2_BLE_Init(&bleInitCmdPacket);
    assert(status == SHCI_Success);

    log_info("CPU2 BLE stack init success");
}

void BluetoothCore::GeneralConfigInit()
{
    tBleStatus ret = hci_reset();
    assert(ret == BLE_STATUS_SUCCESS);

    UpdateDeviceAddress();

    ret = aci_hal_write_config_data(
        CONFIG_DATA_PUBADDR_OFFSET,
        CONFIG_DATA_PUBADDR_LEN,
        deviceAddress);
    assert(ret == BLE_STATUS_SUCCESS);

    log_info(
        "BLE address: %02X:%02X:%02X:%02X:%02X:%02X",
        deviceAddress[5],
        deviceAddress[4],
        deviceAddress[3],
        deviceAddress[2],
        deviceAddress[1],
        deviceAddress[0]);

    SetTxPower(0);
}

void BluetoothCore::GattGapInit()
{
    tBleStatus ret = aci_gatt_init();
    assert(ret == BLE_STATUS_SUCCESS);

    ret = aci_gap_init(
        GAP_PERIPHERAL_ROLE,
        PRIVACY_DISABLED,
        static_cast<uint8_t>(strlen(deviceName)),
        &gap_service_handle,
        &gap_dev_name_char_handle,
        &gap_appearance_char_handle);
    assert(ret == BLE_STATUS_SUCCESS);

    ret = aci_gatt_update_char_value(
        gap_service_handle,
        gap_dev_name_char_handle,
        0,
        static_cast<uint8_t>(strlen(deviceName)),
        reinterpret_cast<uint8_t *>(deviceName));
    assert(ret == BLE_STATUS_SUCCESS);

    uint16_t appearance = 0x0000;
    ret = aci_gatt_update_char_value(
        gap_service_handle,
        gap_appearance_char_handle,
        0,
        sizeof(appearance),
        reinterpret_cast<uint8_t *>(&appearance));
    assert(ret == BLE_STATUS_SUCCESS);

    ret = aci_gap_set_io_capability(IO_CAP_NO_INPUT_NO_OUTPUT);
    assert(ret == BLE_STATUS_SUCCESS);

    log_info("GATT/GAP init success, name=%s", deviceName);
}

void BluetoothCore::InitCustomService()
{
    tBleStatus ret;

    Service_UUID_t serviceUuid = {};
    Char_UUID_t txCharUuid = {};
    Char_UUID_t rxCharUuid = {};

    // 12345678-1234-5678-1234-56789ABCDEF0
    const uint8_t serviceUuid128[16] = {
        0xF0, 0xDE, 0xBC, 0x9A,
        0x78, 0x56,
        0x34, 0x12,
        0x78, 0x56,
        0x34, 0x12,
        0x78, 0x56, 0x34, 0x12};

    // 12345678-1234-5678-1234-56789ABCDEF1
    const uint8_t txCharUuid128[16] = {
        0xF1, 0xDE, 0xBC, 0x9A,
        0x78, 0x56,
        0x34, 0x12,
        0x78, 0x56,
        0x34, 0x12,
        0x78, 0x56, 0x34, 0x12};

    // 12345678-1234-5678-1234-56789ABCDEF2
    const uint8_t rxCharUuid128[16] = {
        0xF2, 0xDE, 0xBC, 0x9A,
        0x78, 0x56,
        0x34, 0x12,
        0x78, 0x56,
        0x34, 0x12,
        0x78, 0x56, 0x34, 0x12};

    memcpy(serviceUuid.Service_UUID_128, serviceUuid128, 16);
    memcpy(txCharUuid.Char_UUID_128, txCharUuid128, 16);
    memcpy(rxCharUuid.Char_UUID_128, rxCharUuid128, 16);

    osMutexAcquire(mutexBleId, osWaitForever);

    ret = aci_gatt_add_service(
        UUID_TYPE_128,
        &serviceUuid,
        PRIMARY_SERVICE,
        8,
        &customServiceHandle);
    assert(ret == BLE_STATUS_SUCCESS);

    ret = aci_gatt_add_char(
        customServiceHandle,
        UUID_TYPE_128,
        &txCharUuid,
        kMaxCharValueLength,
        CHAR_PROP_READ | CHAR_PROP_NOTIFY,
        ATTR_PERMISSION_NONE,
        GATT_DONT_NOTIFY_EVENTS,
        16,
        CHAR_VALUE_LEN_VARIABLE,
        &customTxCharHandle);
    assert(ret == BLE_STATUS_SUCCESS);

    ret = aci_gatt_add_char(
        customServiceHandle,
        UUID_TYPE_128,
        &rxCharUuid,
        kMaxCharValueLength,
        CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP,
        ATTR_PERMISSION_NONE,
        GATT_NOTIFY_ATTRIBUTE_WRITE,
        16,
        CHAR_VALUE_LEN_VARIABLE,
        &customRxCharHandle);
    assert(ret == BLE_STATUS_SUCCESS);

    osMutexRelease(mutexBleId);

    log_info("Custom service init success");
    log_info("Service handle=0x%04X", customServiceHandle);
    log_info("TX char handle=0x%04X", customTxCharHandle);
    log_info("RX char handle=0x%04X", customRxCharHandle);
}

void BluetoothCore::StartAdvertising()
{
    osMutexAcquire(mutexBleId, osWaitForever);

    tBleStatus ret = aci_gap_set_discoverable(
        ADV_IND,
        0x80,
        0xA0,
        GAP_PUBLIC_ADDR,
        NO_WHITE_LIST_USE,
        static_cast<uint8_t>(strlen(deviceName) + 1),
        advName,
        0,
        nullptr,
        0,
        0);

    osMutexRelease(mutexBleId);

    if (ret == BLE_STATUS_SUCCESS)
    {
        log_info("Advertising started, name=%s", deviceName);
    }
    else
    {
        log_error("Advertising start failed: 0x%02X", ret);
    }
}

void BluetoothCore::StopAdvertising()
{
    osMutexAcquire(mutexBleId, osWaitForever);
    tBleStatus ret = aci_gap_set_non_discoverable();
    osMutexRelease(mutexBleId);

    if (ret == BLE_STATUS_SUCCESS)
    {
        log_info("Advertising stopped");
    }
    else
    {
        log_error("Advertising stop failed: 0x%02X", ret);
    }
}

void BluetoothCore::SetTxPower(int8_t dBm)
{
    if (dBm > 6)
    {
        log_warn("Tx power over 6 dBm is not supported");
        dBm = 6;
    }

    if (dBm < -40)
    {
        log_warn("Tx power below -40 dBm is not supported");
        dBm = -40;
    }

    osMutexAcquire(mutexBleId, osWaitForever);

    constexpr uint8_t kZeroDbOffset = 0x19;
    uint8_t level = static_cast<uint8_t>(kZeroDbOffset + dBm);

    tBleStatus ret = aci_hal_set_tx_power_level(1, level);
    assert(ret == BLE_STATUS_SUCCESS);

    osMutexRelease(mutexBleId);

    log_info("Tx power: %d dBm", dBm);
}

void BluetoothCore::Disconnect()
{
    if (!connected)
    {
        return;
    }

    osMutexAcquire(mutexBleId, osWaitForever);
    tBleStatus ret = hci_disconnect(connectionHandle, 0x13);
    osMutexRelease(mutexBleId);

    if (ret != BLE_STATUS_SUCCESS)
    {
        log_error("Disconnect failed: 0x%02X", ret);
    }
}

bool BluetoothCore::IsReady() const
{
    return ready;
}

bool BluetoothCore::IsConnected() const
{
    return connected;
}

const char *BluetoothCore::GetName() const
{
    return deviceName;
}

void BluetoothCore::SetName(const char *newName)
{
    if (newName == nullptr)
    {
        return;
    }

    memset(deviceName, 0, sizeof(deviceName));
    strncpy(deviceName, newName, kNameMaxLength);
    deviceName[kNameMaxLength] = '\0';

    memset(advName, 0, sizeof(advName));
    advName[0] = AD_TYPE_COMPLETE_LOCAL_NAME;
    memcpy(&advName[1], deviceName, strlen(deviceName));

    if (ready)
    {
        osMutexAcquire(mutexBleId, osWaitForever);

        tBleStatus ret = aci_gatt_update_char_value(
            gap_service_handle,
            gap_dev_name_char_handle,
            0,
            static_cast<uint8_t>(strlen(deviceName)),
            reinterpret_cast<uint8_t *>(deviceName));

        osMutexRelease(mutexBleId);

        if (ret != BLE_STATUS_SUCCESS)
        {
            log_error("SetName GAP update failed: 0x%02X", ret);
        }

        if (!connected)
        {
            StopAdvertising();
            StartAdvertising();
        }
    }
}

uint16_t BluetoothCore::ConnectionHandle() const
{
    return connectionHandle;
}

bool BluetoothCore::Send(const uint8_t *data, uint8_t len)
{
    if (data == nullptr || len == 0)
    {
        return false;
    }

    if (len > kMaxCharValueLength)
    {
        len = kMaxCharValueLength;
    }

    osMutexAcquire(mutexBleId, osWaitForever);

    tBleStatus ret = aci_gatt_update_char_value(
        customServiceHandle,
        customTxCharHandle,
        0,
        len,
        const_cast<uint8_t *>(data));

    osMutexRelease(mutexBleId);

    if (ret != BLE_STATUS_SUCCESS)
    {
        log_error("Send failed: 0x%02X", ret);
        return false;
    }

    return true;
}

bool BluetoothCore::SendText(const char *text)
{
    if (text == nullptr)
    {
        return false;
    }

    uint8_t len = static_cast<uint8_t>(strlen(text));
    return Send(reinterpret_cast<const uint8_t *>(text), len);
}

bool BluetoothCore::HasNewRxData() const
{
    return rxUpdated;
}

uint8_t BluetoothCore::GetRxData(uint8_t *out, uint8_t maxLen)
{
    if (out == nullptr || maxLen == 0)
    {
        return 0;
    }

    uint8_t copyLen = rxLength;
    if (copyLen > maxLen)
    {
        copyLen = maxLen;
    }

    memcpy(out, rxBuffer, copyLen);
    rxUpdated = false;

    return copyLen;
}

void BluetoothCore::ClearRxData()
{
    rxLength = 0;
    rxUpdated = false;
    memset(rxBuffer, 0, sizeof(rxBuffer));
}

void BluetoothCore::UpdateDeviceAddress()
{
    uint32_t udn = LL_FLASH_GetUDN();

    deviceAddress[0] = static_cast<uint8_t>(udn & 0xFF);
    deviceAddress[1] = static_cast<uint8_t>((udn >> 8) & 0xFF);
    deviceAddress[2] = static_cast<uint8_t>((udn >> 16) & 0xFF);
    deviceAddress[3] = 0xAB;
    deviceAddress[4] = 0xAF;
    deviceAddress[5] = 0x00;
}

void BluetoothCore::NotifyAsyncEvent()
{
    osThreadFlagsSet(hciUserEventProcessThreadId, kHciEventNotifyFlag);
}

void BluetoothCore::ReleaseCmdSemaphore()
{
    osSemaphoreRelease(semaphoreHciId);
}

void BluetoothCore::WaitCmdSemaphore()
{
    osSemaphoreAcquire(semaphoreHciId, osWaitForever);
}

SVCCTL_UserEvtFlowStatus_t BluetoothCore::HandleSvcEvent(void *p_Pckt)
{
    EventCallback(p_Pckt);
    return SVCCTL_UserEvtFlowEnable;
}

void BluetoothCore::HciUserEventProcessThread(void *argument)
{
    UNUSED(argument);

    for (;;)
    {
        osThreadFlagsWait(kHciEventNotifyFlag, osFlagsWaitAny, osWaitForever);
        hci_user_evt_proc();
    }
}

void BluetoothCore::HciStatusCallback(HCI_TL_CmdStatus_t status)
{
    switch (status)
    {
        case HCI_TL_CmdBusy:
            osMutexAcquire(mutexHciId, osWaitForever);
            break;

        case HCI_TL_CmdAvailable:
            osMutexRelease(mutexHciId);
            break;

        default:
            break;
    }
}

void BluetoothCore::BridgeEventCallback(void *payload)
{
    auto *p_param = reinterpret_cast<tHCI_UserEvtRxParam *>(payload);

    SVCCTL_UserEvtFlowStatus_t status =
        SVCCTL_UserEvtRx(reinterpret_cast<void *>(&(p_param->pckt->evtserial)));

    if (status != SVCCTL_UserEvtFlowDisable)
    {
        p_param->status = HCI_TL_UserEventFlow_Enable;
    }
    else
    {
        p_param->status = HCI_TL_UserEventFlow_Disable;
    }
}

void BluetoothCore::EventCallback(void *packet)
{
    auto *hciEventPacket =
        reinterpret_cast<hci_event_pckt *>(reinterpret_cast<hci_uart_pckt *>(packet)->data);

    switch (hciEventPacket->evt)
    {
        case HCI_DISCONNECTION_COMPLETE_EVT_CODE:
        {
            connected = false;
            connectionHandle = 0;

            log_info("Disconnected");
            Instance().StartAdvertising();
            break;
        }

        case HCI_LE_META_EVT_CODE:
        {
            auto *metaEvent = reinterpret_cast<evt_le_meta_event *>(hciEventPacket->data);

            switch (metaEvent->subevent)
            {
                case HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE:
                {
                    auto *pEvent =
                        reinterpret_cast<hci_le_connection_complete_event_rp0 *>(metaEvent->data);

                    connected = true;
                    connectionHandle = pEvent->Connection_Handle;

                    log_info("Connected, handle=0x%04X", connectionHandle);
                    log_info(
                        "Peer=%02X:%02X:%02X:%02X:%02X:%02X",
                        pEvent->Peer_Address[5],
                        pEvent->Peer_Address[4],
                        pEvent->Peer_Address[3],
                        pEvent->Peer_Address[2],
                        pEvent->Peer_Address[1],
                        pEvent->Peer_Address[0]);
                    break;
                }

                default:
                    break;
            }
            break;
        }

        case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
        {
            auto *bleCoreEvent = reinterpret_cast<evt_blecore_aci *>(hciEventPacket->data);

            switch (bleCoreEvent->ecode)
            {
                case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
                {
                    auto *pEvent =
                        reinterpret_cast<aci_gatt_attribute_modified_event_rp0 *>(
                            bleCoreEvent->data);

                    log_info(
                        "Attribute modified, handle=0x%04X, len=%d",
                        pEvent->Attr_Handle,
                        pEvent->Attr_Data_Length);

                    // RX characteristic value handle은 보통 char handle + 1
                    if (pEvent->Attr_Handle == (customRxCharHandle + 1U))
                    {
                        rxLength = pEvent->Attr_Data_Length;

                        if (rxLength > kMaxCharValueLength)
                        {
                            rxLength = kMaxCharValueLength;
                        }

                        memset(rxBuffer, 0, sizeof(rxBuffer));
                        memcpy(rxBuffer, pEvent->Attr_Data, rxLength);
                        rxUpdated = true;

                        log_info("RX data updated, len=%d", rxLength);
                    }

                    break;
                }

                default:
                    break;
            }
            break;
        }

        default:
            break;
    }
}