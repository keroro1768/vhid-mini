/*
 * Virtual HID Mini Driver - 基礎版本
 * 參考: microsoft/Windows-driver-samples/hid/vhidmini2
 */

#include <windows.h>
#include <hidport.h>
#include <wdf.h>

// WDF 函式庫
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WDFDEVICE, GetDeviceObjectContext)

// 驅動程式全域資料
DRIVER_GLOBAL_DATA g_DriverData;

// 報告描述符 - 簡化鍵盤
const UCHAR g_ReportDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,        //   Usage Minimum (Left Control)
    0x29, 0xE7,        //   Usage Maximum (Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8) - Modifier keys
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8) - Reserved
    0x81, 0x01,        //   Input (Constant)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x05, 0x06,        //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        //   Usage Minimum (Reserved)
    0x29, 0x65,        //   Usage Maximum (Application)
    0x81, 0x00,        //   Input (Data, Array, Absolute) - Key array
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Constant)
    0xC0               // End Collection
};

#define VHIDMINI_POOL_TAG 'mini'

// 裝置上下文
typedef struct _DEVICE_CONTEXT
{
    WDFDEVICE       WdfDevice;
    PHID_DEVICE_ATTRIBUTES  DeviceAttributes;
    PHID_DESCRIPTOR     HidDescriptor;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

// 初始化驅動程式
DRIVER_INITIALIZE DriverEntry;

// EvtDriverDeviceAdd 回調
EVT_WDF_DRIVER_DEVICE_ADD VhidEvtDeviceAdd;

// 裝置建立完成
EVT_WDF_DEVICE_D0_ENTRY VhidEvtDeviceD0Entry;

// 處理 HID 請求
EVT_WDF_REQUEST_PROCESS_IOCTL VhidEvtIoDeviceControl;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    // 初始化追蹤
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "VHID Mini Driver Starting\n");

    // 建立 WDF 驅動程式物件
    WDF_DRIVER_CONFIG_INIT(&config, VhidEvtDeviceAdd);

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, 
            "WdfDriverCreate failed 0x%x\n", status);
    }

    return status;
}

NTSTATUS
VhidEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _In_ PWDFDEVICE_INIT DeviceInit
)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);

    // 設定 PnP 電源回調
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDeviceD0Entry = VhidEvtDeviceD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    // 設定 IO 請求
    WdfDeviceInitSetIoInternalDeviceControl(DeviceInit, VhidEvtIoDeviceControl);

    // 建立裝置物件屬性
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    // 建立裝置
    status = WdfDeviceCreate(
        DeviceInit,
        &deviceAttributes,
        &deviceContext->WdfDevice
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT,
            "WdfDeviceCreate failed 0x%x\n", status);
        return status;
    }

    // 設定 HID 屬性
    deviceContext->DeviceAttributes = &g_DeviceAttributes;
    deviceContext->DeviceAttributes->Size = sizeof(HID_DEVICE_ATTRIBUTES);
    deviceContext->DeviceAttributes->VendorID = 0x1234;  // 自訂 VID
    deviceContext->DeviceAttributes->ProductID = 0x5678; // 自訂 PID
    deviceContext->DeviceAttributes->VersionNumber = 0x0100;

    // 設定 HID 描述符
    deviceContext->HidDescriptor = &g_HidDescriptor;
    deviceContext->HidDescriptor->bLength = sizeof(HID_DESCRIPTOR);
    deviceContext->HidDescriptor->bDescriptorType = HID_HID_DESCRIPTOR_TYPE;
    deviceContext->HidDescriptor->wDescriptorLength = sizeof(g_ReportDescriptor);

    // 設定序列號 (可選)
    UNICODE_STRING serialNumber;
    RtlInitUnicodeString(&serialNumber, L"001");
    status = WdfDeviceCreateSymbolicLink(
        deviceContext->WdfDevice,
        &serialNumber
    );

    return STATUS_SUCCESS;
}

NTSTATUS
VhidEvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
        "Device entering D0\n");

    return STATUS_SUCCESS;
}

VOID
VhidEvtIoDeviceControl(
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject,
    _In_ WDFQUEUE Queue,
    _In_ ULONG IoControlCode,
    _In_ size_t InputBufferLength,
    _In_ size_t OutputBufferLength,
    _In_ ULONG Flags
)
{
    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(Flags);

    WDFREQUEST memoryRequest;
    NTSTATUS status = STATUS_SUCCESS;
    PHID_XFER_PARAMS transferParams;

    switch (IoControlCode)
    {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        // 回傳 HID 描述符
        TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
            "IOCTL_HID_GET_DEVICE_DESCRIPTOR\n");
        break;

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        // 回傳報告描述符
        TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
            "IOCTL_HID_GET_REPORT_DESCRIPTOR\n");
        break;

    case IOCTL_HID_READ_REPORT:
        // 讀取報告 (從裝置)
        TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
            "IOCTL_HID_READ_REPORT\n");
        break;

    case IOCTL_HID_WRITE_REPORT:
        // 寫入報告 (到裝置)
        TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
            "IOCTL_HID_WRITE_REPORT\n");
        break;

    case IOCTL_HID_GET_STRING:
        // 回傳字串描述符
        TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
            "IOCTL_HID_GET_STRING\n");
        break;

    default:
        TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL,
            "Unknown IOCTL 0x%x\n", IoControlCode);
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    WdfRequestComplete(Request, status);
}
