# User Mode Driver Framework (UMDF) 開發指南

## 概述

UMDF (User Mode Driver Framework) 是 Microsoft 提供的驅動程式框架，允許開發者在**使用者模式**（Ring 3）撰寫驅動程式，而非核心模式（Ring 0）。

---

## UMDF vs KMDF vs WDM

| 特性 | UMDF | KMDF | WDM |
|------|------|------|-----|
| 執行模式 | 使用者模式 | 核心模式 | 核心模式 |
| 穩定性 | 最高（記憶體隔離） | 高 | 低 |
| 難度 | 較易 | 中等 | 困難 |
| 效能 | 較低 | 高 | 最高 |
| 適合 | HID, 檔案系統 | 任何 | 老舊驅動 |

---

## UMDF 版本

| 版本 | Windows 版本 | 時間 |
|------|--------------|------|
| UMDF 1.0 | Vista | 2006 |
| UMDF 1.5 | Win 8.1 | 2013 |
| UMDF 1.11 | Win 10 | 2015 |
| UMDF 2.0 | Win 10 1607+ | 2016 |
| UMDF 2.15 | Win 11 | 2021 |

---

## 開發環境設定

### 1. 安裝順序

```
1. Windows SDK (最新穩定版)
2. Visual Studio 2022
3. Windows Driver Kit (WDK)
```

### 2. VS2022 工作負載

安裝時選擇：
- **使用 C++ 的桌面開發**
- **Windows Driver**
- **C++ 通用 Windows 平台工具**

### 3. 驗證安裝

開啟 Visual Studio，確認出現：
- 檔案 > 新增 > 專案 > Windows Driver
- C++ > Windows Driver

---

## 建立第一個 UMDF 專案

### 1. 建立專案

```
1. Visual Studio > 新增專案
2. 搜尋 "User Mode Driver"
3. 選擇 "User Mode Driver (UMDF V2)"
4. 命名專案 (例如: MyUmdfDriver)
```

### 2. 專案範本結構

```
MyUmdfDriver/
├── MyUmdfDriver.cpp      # 驅動程式入口
├── MyUmdfDriver.h        # 標頭檔
├── Device.cpp            # 裝置物件
├── Driver.cpp            # 驅動物件
├── inf/                  # INF 檔案
│   └── MyUmdfDriver.inf
└── Package/             # 驅動程式封裝
```

### 3. 核心程式碼

#### Driver.cpp - 驅動程式入口

```cpp
#include <windows.h>
#include <wdf.h>

// WDF CALLBACK 宣告
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD EvtDriverDeviceAdd;

// 驅動程式進入點
NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;

    // 初始化驅動程式物件
    WDF_DRIVER_CONFIG_INIT(&config, EvtDriverDeviceAdd);

    // 建立 WDF 驅動程式
    return WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );
}

// 當有裝置插入時呼叫
NTSTATUS EvtDriverDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
)
{
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;

    // 設定裝置屬性
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    // 建立裝置物件
    return WdfDeviceCreate(
        DeviceInit,
        &attributes,
        &device
    );
}
```

---

## WDF 物件層級

```
WDFDRIVER          # 驅動程式物件 (每個驅動一個)
    └── WDFDEVICE   # 裝置物件 (每個裝置一個)
            ├── WDFQUEUE         # I/O 佇列
            ├── WDFREQUEST      # I/O 請求
            ├── WDFMEMORY       # 記憶體
            └── WDFWORKITEM    # 工作項目
```

---

## I/O 請求處理

### 1. 建立預設佇列

```cpp
NTSTATUS CreateQueue(WDFDEVICE Device)
{
    WDF_IO_QUEUE_CONFIG config;
    
    // 初始化佇列
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &config,
        WdfIoQueueDispatchSequential
    );

    // 設定請求處理回調
    config.EvtIoRead = EvtIoRead;
    config.EvtIoWrite = EvtIoWrite;
    config.EvtIoDeviceControl = EvtIoDeviceControl;

    // 建立佇列
    return WdfIoQueueCreate(
        Device,
        &config,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE
    );
}
```

### 2. 處理讀取請求

```cpp
VOID EvtIoRead(
    WDFREQUEST Request,
    WDFLENGTH OutputBufferLength,
    ULONG ReadOffset
)
{
    NTSTATUS status;
    WDFMEMORY memory;
    
    // 取得請求關聯的記憶體
    status = WdfRequestRetrieveOutputMemory(
        Request,
        &memory
    );

    if (NT_SUCCESS(status)) {
        // 取得記憶體指標
        PVOID buffer = WdfMemoryGetBuffer(memory, NULL);
        
        // 填充資料 (範例)
        RtlCopyMemory(buffer, "Hello from UMDF!", 18);
        
        // 完成請求
        WdfRequestCompleteWithInformation(
            Request,
            STATUS_SUCCESS,
            18  // 傳回的位元組數
        );
    } else {
        WdfRequestComplete(Request, status);
    }
}
```

---

## INF 檔案

### 基本結構

```inf
[Version]
Signature   = "$Windows NT$"
Class       = Sample
ClassGuid   = {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
Provider   = %ManufacturerName%
DriverVer   = 01/01/2026,1.0.0.0

[Manufacturer]
%ManufacturerName% = DeviceList, NTamd64

[DeviceList.NTamd64]
%DeviceName% = DeviceInstall, USB\VID_1234&PID_5678

[DeviceInstall]
CopyFiles = DriverFileList

[DriverFileList]
MyUmdfDriver.dll,,,0x00000001

[Strings]
ManufacturerName = "My Company"
DeviceName = "My UMDF Device"
```

---

## 建置與部署

### 1. 建置

```powershell
# 使用 Developer Command Prompt
msbuild MyUmdfDriver.sln /p:Configuration=Debug /p:Platform=x64
```

### 2. 部署

```powershell
# 安裝驅動程式
pnputil /add-driver .\MyUmdfDriver.inf /install
```

---

## 偵錯

### 紅色منتظر執行階段追蹤

```cpp
#include <wdftrace.h>

// 初始化追蹤
TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "Driver Entry\n");

// 在函數中追蹤
TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IO, 
    "Processing read request\n");
```

### 使用 WinDbg

```powershell
#附加到程序
.attach -p <PID>

# 設定中斷點
bp MyDriver!EvtIoRead

# 繼續執行
g
```

---

## 常見問題

### Q1: 驅動程式無法安裝

**解決**：
- 確認已啟用測試簽名
- 檢查 INF 語法
- 查看事件檢視器

### Q2: 程式崩潰

**解決**：
- 使用 WER (Windows Error Reporting)
- 分析記憶體傾印

### Q3: 效能問題

**解決**：
- 避免核心模式呼叫
- 使用非同步 I/O
- 減少鎖定時間

---

## 官方資源

| 資源 | 連結 |
|------|------|
| UMDF 文件 | https://learn.microsoft.com/en-us/windows-hardware/drivers/umdf/ |
| WDF 文件 | https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/ |
| 驅動程式範本 | https://github.com/microsoft/Windows-driver-samples |
| WDK 下載 | https://developer.microsoft.com/windows-hardware/drivers/ |

---

## UMDF HID 專案範例

| 專案 | 說明 |
|------|------|
| vhidmini2 | 微軟官方 UMDF HID 範本 |
| hidusbfx2 | USB FX2 HID 範本 |
| kmdfclub-hid | 社群範例 |

---

*建立時間: 2026-03-25*
