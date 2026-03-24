# IOCTL 通訊架構指南

## 概述

IOCTL (Input/Output Control) 是 Windows 中應用程式與驅動程式之間溝通的主要機制。本文說明所需的元件和設定。

---

## 通訊架構

```
┌────────────────────────────────────────────────────────────┐
│                    應用程式 (User Mode)                     │
│                                                            │
│   CreateFile("\\\\.\\MyDevice")                             │
│            ↓                                                │
│   DeviceIoControl(hDevice, IOCTL_CODE, ...)               │
│            ↓                                                │
└──────────────────────────┬─────────────────────────────────┘
                           │
                    ┌──────┴──────┐
                    │  IRP 封包   │
                    └──────┬──────┘
                           ↓
┌──────────────────────────┬─────────────────────────────────┐
│              驅動程式 (Kernel Mode)                          │
│                                                            │
│   EvtIoDeviceControl(hDevice, IoctlCode, ...)             │
└────────────────────────────────────────────────────────────┘
```

---

## 需要什麼？

### 1. 裝置名稱 (Device Name)

驅動程式需要建立一個 Symbolic Name 讓應用程式可以開啟：

```c
// 驅動程式中
UNICODE_STRING deviceName;
RtlInitUnicodeString(&deviceName, L"\\Device\\MyDevice");

// 建立裝置
IoCreateDevice(DriverObject, ..., &deviceName, ...);

// 建立符號連結（讓使用者模式可以開啟）
UNICODE_STRING symLink;
RtlInitUnicodeString(&symLink, L"\\DosDevices\\MyDevice");
IoCreateSymbolicLink(&symLink, &deviceName);
```

### 2. 符號連結 (Symbolic Link)

應用程式透過 `\\.\MyDevice` 開啟裝置：

```c
// 應用程式
HANDLE hDevice = CreateFile(
    "\\\\.\\MyDevice",           // 符號連結名稱
    GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    NULL, OPEN_EXISTING, 0, NULL
);
```

### 3. IOCTL 代碼定義

自訂 IOCTL 需要使用巨集定義：

```c
// 方法 1: CTL_CODE 巨集
#define IOCTL_MYDRIVER_READ_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// 方法 2: 手動定義
// | DeviceType(16) | Function(12) | Method(2) | Access(2) |
// |    0x8000     |   0x0800    |   0x00   |   0x0000 |
```

#### CTL_CODE 參數

| 參數 | 說明 | 範例 |
|------|------|------|
| DeviceType | 裝置類型 | `FILE_DEVICE_UNKNOWN` (0x8000) |
| Function | 功能代碼 | 0x800-0xFFF 自訂 |
| Method | 資料傳遞方式 | `METHOD_BUFFERED` |
| Access | 存取權限 | `FILE_ANY_ACCESS` |

#### 傳遞方式 (Method)

| 方式 | 說明 | 適用場景 |
|------|------|----------|
| `METHOD_BUFFERED` | 緩衝 I/O | 小資料 (< 4KB) |
| `METHOD_IN_DIRECT` | 直接寫入 | 大資料寫入 |
| `METHOD_OUT_DIRECT` | 直接讀取 | 大資料讀取 |
| `METHOD_NEITHER` | 無緩衝 | 直接記憶體操作 |

---

## 實作範例

### 驅動程式端 (KMDF)

```c
#include <wdf.h>

// IOCTL 代碼
#define IOCTL_GET_DEVICE_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SEND_DATA \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// 處理 IOCTL
VOID
EvtIoDeviceControl(
    WDFREQUEST Request,
    WDFQUEUE Queue,
    ULONG IoControlCode,
    size_t InputBufferLength,
    size_t OutputBufferLength,
    ULONG Flags
)
{
    NTSTATUS status = STATUS_SUCCESS;
    size_t bufLen = 0;

    switch (IoControlCode)
    {
        case IOCTL_GET_DEVICE_STATUS:
        {
            // 取得輸出緩衝區
            WDFMEMORY outputMem;
            status = WdfRequestRetrieveOutputMemory(Request, &outputMem);
            
            if (NT_SUCCESS(status)) {
                PVOID buffer = WdfMemoryGetBuffer(outputMem, &bufLen);
                
                // 填充資料
                *(PULONG)buffer = 0x1234;  // 範例狀態
                
                WdfRequestCompleteWithInformation(
                    Request, STATUS_SUCCESS, sizeof(ULONG));
            }
            break;
        }

        case IOCTL_SEND_DATA:
        {
            // 取得輸入資料
            WDFMEMORY inputMem;
            status = WdfRequestRetrieveInputMemory(Request, &inputMem);
            
            if (NT_SUCCESS(status)) {
                PVOID buffer = WdfMemoryGetBuffer(inputMem, &bufLen);
                
                // 處理資料...
                
                WdfRequestComplete(Request, STATUS_SUCCESS);
            }
            break;
        }

        default:
            status = STATUS_NOT_SUPPORTED;
            WdfRequestComplete(Request, status);
            break;
    }
}
```

### 應用程式端

```c
#include <windows.h>
#include <winioctl.h>

// IOCTL 代碼（需與驅動程式相同）
#define IOCTL_GET_DEVICE_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SEND_DATA \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

int main()
{
    // 開啟裝置
    HANDLE hDevice = CreateFile(
        "\\\\.\\MyDevice",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("開啟失敗: %d\n", GetLastError());
        return 1;
    }

    // 範例 1: 取得狀態
    DWORD bytesReturned;
    ULONG status;
    DeviceIoControl(
        hDevice,
        IOCTL_GET_DEVICE_STATUS,
        NULL, 0,                    // 輸入
        &status, sizeof(status),   // 輸出
        &bytesReturned,
        NULL
    );
    printf("Device Status: 0x%04X\n", status);

    // 範例 2: 傳送資料
    ULONG inputData = 0xABCD;
    DeviceIoControl(
        hDevice,
        IOCTL_SEND_DATA,
        &inputData, sizeof(inputData),  // 輸入
        NULL, 0,                        // 輸出
        &bytesReturned,
        NULL
    );

    CloseHandle(hDevice);
    return 0;
}
```

---

## UMDF 版本

### 驅動程式端

```c
#include <wdf.h>

// IOCTL 處理
EVT_WDF_REQUEST_PROCESS_IOCTL EvtIoDeviceControl;

VOID
EvtIoDeviceControl(
    WDFREQUEST Request,
    WDFFILEOBJECT FileObject,
    WDFQUEUE Queue,
    ULONG IoControlCode,
    size_t InputBufferLength,
    size_t OutputBufferLength,
    ULONG Flags
)
{
    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Flags);

    NTSTATUS status = STATUS_SUCCESS;
    WDFMEMORY memory;

    switch (IoControlCode)
    {
        case IOCTL_GET_DEVICE_STATUS:
            // 處理...
            break;

        default:
            status = STATUS_NOT_SUPPORTED;
            break;
    }

    WdfRequestComplete(Request, status);
}
```

---

## 常見問題

### Q1: 權限不足

**錯誤**：`Access Denied`

**解決**：
- 以系統管理員執行
- 或在 INF 中設定安全描述符

```inf
[DeviceInstall]
AddReg = SecuritySettings

[SecuritySettings]
HKR,,Security,,"D:P(A;;GA;;;BA)(A;;GA;;;SY)"
```

### Q2: 找不到裝置

**錯誤**：`The system cannot find the file`

**解決**：
- 確認驅動程式已正確安裝
- 確認符號連結已建立
- 檢查裝置名稱拼寫

### Q3: IOCTL 無法送達

**檢查清單**：
- [ ] 驅動程式已啟動
- [ ] IOCTL 代碼正確
- [ ] 緩衝區大小正確
- [ ] 權限足夠

---

## 安全考量

### 1. 驗證輸入

```c
case IOCTL_SEND_DATA:
    if (InputBufferLength < sizeof(MY_DATA_STRUCT)) {
        status = STATUS_BUFFER_TOO_SMALL;
        break;
    }
    // 驗證資料內容...
    break;
```

### 2. 權限檢查

```c
// 在 WDF 中
EVT_WDF_DEVICE_FILE_CREATE EvtDeviceFileCreate;

VOID
EvtDeviceFileCreate(
    WDFDEVICE Device,
    WDFFILEOBJECT FileObject,
    WDFFILE_REQUEST_COMpletionType CompletionType
)
{
    // 檢查程序權限
    // ...
}
```

### 3. 隔離模式

```inf
; INF 中設定
HKR,"Parameters","umba_MinimumSecurityLevel",0x00010001,0x10000
```

---

## 測試工具

### 1. 命令列工具

```powershell
# 使用 devcon 查詢
devcon status "ROOT\MyDevice"

# 或使用 powershell
Get-PnpDevice -FriendlyName "MyDevice"
```

### 2. Debugging

```c
// 加入追蹤
TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
    "IOCTL received: 0x%x\n", IoControlCode);
```

---

## 總結

| 元件 | 說明 |
|------|------|
| **Device Name** | `\\Device\MyDevice` |
| **Symbolic Link** | `\\.\MyDevice` |
| **IOCTL Code** | CTL_CODE 巨集定義 |
| **CreateFile** | 開啟裝置 |
| **DeviceIoControl** | 傳送 IOCTL |

---

## 參考資源

| 資源 | 連結 |
|------|------|
| IOCTL 簡介 | https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/introduction-to-i-o-control-codes |
| CTL_CODE | https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/defining-i-o-control-codes |
| WDF IOCTL | https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/

---

*建立時間: 2026-03-25*
