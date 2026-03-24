# HID Filter Driver 方案：修改與注入

## 概述

 Filter Driver 方案是一種在**現有 HID 裝置**上攔截並修改資料的技術，可以：
1. 修改 HID Descriptor 以新增虛擬功能
2. 注入額外的 HID 報告
3. 過濾/修改來自硬體的原始資料

---

## 架構比較

### 方案一：雙驅動程式（之前討論）

```
┌─────────────────┐    IOCTL     ┌─────────────────┐
│  虛擬 HID       │◄───────────►│  實體 HID-I2C   │
│  Driver         │   通訊      │  Driver         │
└─────────────────┘             └─────────────────┘
```

### 方案二：Filter Driver（本文重點）

```
┌─────────────────────────────────────────────────────────┐
│                   驅動程式堆疊                           │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  [HID Class Driver]  ◄── 上層                          │
│         ↑                                              │
│  [Upper Filter]  ◄── 我們的 Filter Driver               │
│         │                                              │
│  [HID Minidriver]  ◄── 原始 HID-I2C 驅動               │
│         │                                              │
│  [Hardware]  ◄── I2C 介面                               │
└─────────────────────────────────────────────────────────┘
```

---

## Filter Driver 類型

### 1. Upper Filter（上位過濾器）

```
位置：在功能驅動程式之上
用途：攔截來自上層的請求 (IRP_MJ_READ, IOCTL 等)
```

### 2. Lower Filter（下位過濾器）

```
位置：在功能驅動程式之下
用途：攔截來自硬體的資料
```

### 3. 對 HID 的意義

| Filter 類型 | 適合用途 |
|-------------|----------|
| Upper Filter | 攔截應用程式的讀取請求 |
| Lower Filter | 修改硬體回傳的資料、注入虛擬資料 |

---

## HID 堆疊結構

### 標準 HID 裝置堆疊

```
User Mode Application
         │
    [HID Class Driver (hidclass.sys)]
         │ IOCTL
    [HID Minidriver (hidusb.sys / hidi2c.sys)]
         │
    [USB / I2C 硬體]
```

### 加入 Filter 後

```
User Mode Application
         │
    [HID Class Driver]
         │
    [Upper Filter Driver]  ◄── 我們的 Filter
         │
    [HID Minidriver]  ◄── 原始 hidusb/hidi2c
         │
    [USB / I2C 硬體]
```

---

## 實作方式

### 1. 在 INF 中註冊 Filter

```inf
[DestinationDirs]
DefaultDestDir = 12

[Manufacturer]
%MfgName% = DeviceList, NTamd64

[DeviceList.NTamd64]
%DeviceName% = Install, USB\VID_1234&PID_5678

[Install]
AddReg = Filter.AddReg

[Filter.AddReg]
; 加入為 Lower Filter
HKR,,"LowerFilters",0x00010000,"MyHIDFilter"

; 或加入為 Upper Filter
; HKR,,"UpperFilters",0x00010000,"MyHIDFilter"
```

### 2. 建立 Filter Driver

```c
// Filter Driver 框架
#include <wdf.h>

// 支援 HID 類別
#include <hidclass.h>

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD EvtDeviceAdd;
EVT_WDF_DEVICE_FILTER_WDM_PREPARE_HARDWARE EvtFilterPrepareHardware;

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    
    WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}

NTSTATUS
EvtDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
)
{
    // 設定為 HID Filter
    WdfDeviceInitSetFileObjectConfig(DeviceInit, ...);
    
    // 建立裝置
    return WdfDeviceCreate(DeviceInit, ...);
}
```

---

## 修改 HID Descriptor

### 方法一：攔截描述符查詢

```c
// 處理 IRP_MJ_DEVICE_CONTROL
EVT_WDF_REQUEST_PROCESS_IOCTL EvtIoDeviceControl;

VOID
EvtIoDeviceControl(
    WDFREQUEST Request,
    ULONG IoControlCode,
    size_t InputBufferLength,
    size_t OutputBufferLength
)
{
    switch (IoControlCode)
    {
        case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        {
            // 取得原始描述符
            PVOID buffer = WdfRequestGetUnsafeUserOutputBuffer(...);
            
            // 修改描述符：新增鍵盤 Collection
            ModifyReportDescriptor(buffer, outputLength);
            break;
        }
    }
}
```

### 方法二：使用 Filter 的 PrepareHardware

```c
EVT_WDF_DEVICE_FILTER_WDM_PREPARE_HARDWARE EvtFilterPrepareHardware;

NTSTATUS
EvtFilterPrepareHardware(
    WDFDEVICE Device,
    WDFCMPARTLIST ResourceList
)
{
    // 在硬體初始化前修改描述符
    PHID_DESCRIPTOR hidDesc = GetOriginalDescriptor();
    
    // 建立新的描述符（加入鍵盤）
    PHID_DESCRIPTOR newDesc = CreateEnhancedDescriptor(hidDesc);
    
    // 儲存供後續使用
    FilterContext->Descriptor = newDesc;
    
    return STATUS_SUCCESS;
}
```

---

## 注入 HID 報告

### 方法一：攔截 Read IRP

```c
EVT_WDF_REQUEST_COMPLETION EvtReadCompletion;

VOID
EvtIoRead(
    WDFREQUEST Request,
    size_t Length
)
{
    // 設定完成回調
    WdfRequestSetCompletionRoutine(Request, EvtReadCompletion, Context);
    
    // 轉發到底層
    WdfRequestSend(Request, WdfNoTarget, WDF_NO_SEND_OPTIONS);
}

VOID
EvtReadCompletion(
    WDFREQUEST Request,
    WDFIORESULT Result
)
{
    if (Result == WDF_IORESULT_COMPLETED)
    {
        PVOID buffer = WdfRequestGetUnsafeUserOutputBuffer(...);
        ULONG length = WdfRequestGetCompletionInformation(...);
        
        // 在這裡注入虛擬鍵盤/滑鼠資料
        InjectVirtualKeyboard(buffer, length);
    }
    
    // 完成請求
    WdfRequestComplete(Request, Result);
}
```

### 方法二：使用 Hidden Collection

```c
// 在 HID Descriptor 中隱藏一個額外的 Collection
// 讓驅動程式專門用於注入

UCHAR EnhancedReportDescriptor[] = {
    // 原始觸控 Collection
    0x05, 0x0D,        // Usage Page (Digitizer)
    0x09, 0x04,        // Usage (Touch Screen)
    0xA1, 0x01,        // Collection (Application)
    // ... 原始描述符 ...
    0xC0,              // End Collection
    
    // Hidden Keyboard Collection (被隱藏)
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0xFF,        // Report ID (0xFF - 特殊用途)
    // ... 鍵盤描述符 ...
    0xC0               // End Collection
};
```

---

## 完整流程圖

```
┌─────────────────────────────────────────────────────────────┐
│                    Filter Driver 運作流程                   │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  1. 硬體初始化                                               │
│     └─► EvtFilterPrepareHardware                            │
│         ├─► 取得原始 HID Descriptor                         │
│         └─► 修改 Descriptor (加入鍵盤/滑鼠)                  │
│                                                              │
│  2. 應用程式讀取                                             │
│     └─► IRP_MJ_READ                                          │
│         ├─► 轉發到底層驅動                                   │
│         └─► 完成時注入虛擬資料                                │
│                                                              │
│  3. 報告傳輸                                                 │
│     └─► IOCTL_HID_READ_REPORT                               │
│         └─► 注入虛擬鍵盤/滑鼠報告                            │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 實際範例：將觸控板變成鍵盤

### 目標

```
原始裝置：觸控板 (單一功能)
         │
    + Filter Driver
         │
增強裝置：觸控板 + 虛擬鍵盤
```

### HID Descriptor 設計

```c
// 原始Descriptor (觸控)
UCHAR OriginalDescriptor[] = {
    0x05, 0x0D,     // Digitizer
    0x09, 0x04,    // Touch Screen
    0xA1, 0x01,    // Application
    // ...
    0xC0
};

// 增強Descriptor (觸控 + 鍵盤)
UCHAR EnhancedDescriptor[] = {
    // === Collection 1: Touch ===
    0x05, 0x0D,     // Digitizer
    0x09, 0x04,    // Touch Screen
    0xA1, 0x01,    // Application
    0x85, 0x01,    // Report ID 1
    // ... Touch report ...
    0xC0,
    
    // === Collection 2: Virtual Keyboard ===
    0x05, 0x01,     // Generic Desktop
    0x09, 0x06,    // Keyboard
    0xA1, 0x01,    // Application
    0x85, 0x02,    // Report ID 2 (虛擬鍵盤)
    // ... Keyboard report ...
    0xC0
};
```

### 報告注入邏輯

```c
VOID InjectVirtualKeyboard(
    PUCHAR ReportBuffer,
    ULONG BufferLength
)
{
    // 檢查是否有待發送的虛擬鍵盤事件
    if (HasPendingKeyEvent())
    {
        // 建立鍵盤報告 (Report ID = 2)
        UCHAR keyboardReport[8] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        
        // 取得按鍵
        KEY_EVENT event = GetNextKeyEvent();
        keyboardReport[2] = event.Modifiers;  // Shift, Ctrl, etc.
        keyboardReport[3] = event.KeyCode;     // Key code
        
        // 注入到回應緩衝區
        // 由於有兩個 Collection，長度需要包含兩個報告
    }
}
```

---

## 優勢與限制

### 優勢

| 項目 | 說明 |
|------|------|
| **單一裝置** | 只需要一個 HID 裝置 |
| **無需虛擬** | 利用現有硬體擴展功能 |
| **可通過認證** | 如實體部分符合規範，有機會認證 |
| **低延遲** | 直接在硬體驅動層處理 |

### 限制

| 項目 | 說明 |
|------|------|
| **需要硬體** | 仍需要一個實體 HID-I2C 裝置 |
| **Descriptor 修改** | 需要正確設計避免衝突 |
| **複雜度較高** | 需要深入了解 HID 堆疊 |

---

## 適合場景

| 場景 | 適合度 |
|------|--------|
| 現有觸控板 + 虛擬鍵盤 | ✅ 極適合 |
| I3C Bridge 轉接頭 | ✅ 適合 |
| 純軟體虛擬鍵盤/滑鼠 | ❌ 不適合（用 VHF） |
| 需要通過認證 | ⚠️ 有機會但需努力 |

---

## 下一步

1. **取得 Microsoft HID I2C 驅動程式原始碼** (或使用現成範本)
2. **建立 KMDF Filter Driver**
3. **測試 Descriptor 修改**
4. **實作報告注入**
5. **如有需要，嘗試 HLK 認證**

---

## 參考資源

| 資源 | 連結 |
|------|------|
| HID Filter Driver | https://github.com/microsoft/Windows-driver-samples/tree/main/hid/hidusbfx2 |
| HID Minidriver | https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/minidriver-operations |
| VHF 文件 | https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/virtual-hid-framework--vhf- |

---

*建立時間: 2026-03-25*
