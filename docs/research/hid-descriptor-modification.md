# HID Descriptor 分析與修改指南

## 概述

本文說明如何分析現有 HID 裝置的 Descriptor，並在其中插入鍵盤/滑鼠 Collection。

---

## 一、取得 HID Descriptor

### 方法 1：使用 Windows 工具

```powershell
# 使用 PowerShell 取得 HID Descriptor
Get-PnpDevice -Class HIDClass | Format-List

# 或使用 devcon
devcon findall =HIDClass
```

### 方法 2：使用 USB 監控工具

| 工具 | 說明 |
|------|------|
| **Wireshark + USBPcap** | 擷取 USB 流量 |
| **Bus Hound** | 商業工具 |
| **USBlyzer** | 商業工具 |

### 方法 3：從驅動程式取得

```c
// 在 Filter Driver 中攔截
case IOCTL_HID_GET_REPORT_DESCRIPTOR:
{
    // 取得 Descriptor
    PHID_DESCRIPTOR desc = (PHID_DESCRIPTOR)Buffer;
    
    // 儲存原始 Descriptor
    RtlCopyMemory(SavedDescriptor, desc, desc->bLength);
    break;
}
```

---

## 二、分析 HID Descriptor

### HID Descriptor 結構

```c
typedef struct _HID_DESCRIPTOR {
    UCHAR bLength;           // 描述符長度
    UCHAR bDescriptorType;   // 描述符類型 (0x21 = HID)
    USHORT bcdHID;          // HID 版本
    UCHAR bCountry;          // 國家代碼
    UCHAR bNumDescriptors;  // 描述符數量
    
    // 描述符列表
    struct {
        UCHAR bReportType;
        USHORT wReportLength;
    } DescriptorList[1];
} HID_DESCRIPTOR;
```

### Report Descriptor 解析工具

#### 1. USB-IF HID Descriptor Tool (推薦)

| 資源 | 連結 |
|------|------|
| 下載 | https://usb.org/document-library/hid-descriptor-tool |
| 功能 | 圖形化建立/編輯 HID Descriptor |

#### 2. 線上解析器

| 工具 | 連結 |
|------|------|
| eleccelerator | https://eleccelerator.com/usbdescreqparser/ |
| Frank Zhao Parser | http://www.frank-zhao.com/usbhidreportdescparser/ |

#### 3. 命令列工具

```bash
# 使用 hidrd 有 Linux
hidrd /dev/usb/hiddev0 -o output.txt
```

---

## 三、讀懂 HID Report Descriptor

### 常用項

| 代碼 | 名稱 | 說明 |
|------|------|------|
| 0x05, xx | Usage Page | 用途頁面 |
| 0x09 | Usage | 用途 |
| 0xA1 | Collection | 集合開始 |
| 0xC0 | End Collection | 集合結束 |
| 0x85 | Report ID | 報告 ID |
| 0x15 | Logical Minimum | 邏輯最小值 |
| 0x25 | Logical Maximum | 邏輯最大值 |
| 0x75 | Report Size | 報告大小 |
| 0x95 | Report Count | 報告數量 |
| 0x81 | Input | 輸入欄位 |
| 0x91 | Output | 輸出欄位 |

### 用途頁面 (Usage Page) 常用值

| 值 | 用途頁面 |
|----|----------|
| 0x01 | Generic Desktop |
| 0x06 | Keyboard/Keypad |
| 0x09 | Button |
| 0x0D | Digitizer (觸控) |
| 0x12 | Consumer |

---

## 四、修改 Descriptor：插入鍵盤/滑鼠

### 原始 Descriptor 範例（觸控）

```c
UCHAR OriginalDescriptor[] = {
    // === Touch Screen ===
    0x05, 0x0D,             // Usage Page (Digitizer)
    0x09, 0x04,             // Usage (Touch Screen)
    0xA1, 0x01,             // Collection (Application)
    0x85, 0x01,             //   Report ID (1)
    
    // Touch data...
    0x09, 0x22,             //   Usage (Finger)
    0xA1, 0x00,             //   Collection (Physical)
    0x15, 0x00,             //   Logical Minimum (0)
    0x25, 0x01,             //   Logical Maximum (1)
    0x75, 0x01,             //   Report Size (1)
    0x95, 0x01,             //   Report Count (1)
    0x81, 0x02,             //   Input (Data, Variable, Absolute)
    // ... more fields ...
    0xC0,                   //   End Collection
    0xC0                    // End Collection
};
```

### 增強 Descriptor（觸控 + 鍵盤）

```c
UCHAR EnhancedDescriptor[] = {
    // === Collection 1: Touch (Report ID 1) ===
    0x05, 0x0D,             // Usage Page (Digitizer)
    0x09, 0x04,             // Usage (Touch Screen)
    0xA1, 0x01,             // Collection (Application)
    0x85, 0x01,             //   Report ID (1)
    
    // Touch data... (同原來)
    0x09, 0x22,             //   Usage (Finger)
    0xA1, 0x00,             //   Collection (Physical)
    0x15, 0x00,             //   Logical Minimum (0)
    0x25, 0x01,             //   Logical Maximum (1)
    0x75, 0x01,             //   Report Size (1)
    0x95, 0x01,             //   Report Count (1)
    0x81, 0x02,             //   Input
    // ... more touch fields ...
    0xC0,                   //   End Collection
    0xC0,                   // End Collection
    
    // === Collection 2: Keyboard (Report ID 2) ===
    0x05, 0x01,             // Usage Page (Generic Desktop)
    0x09, 0x06,             // Usage (Keyboard)
    0xA1, 0x01,             // Collection (Application)
    0x85, 0x02,             //   Report ID (2)
    
    // Modifiers (1 byte)
    0x05, 0x07,             //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,             //   Usage Minimum (Left Control)
    0x29, 0xE7,             //   Usage Maximum (Right GUI)
    0x15, 0x00,             //   Logical Minimum (0)
    0x25, 0x01,             //   Logical Maximum (1)
    0x75, 0x01,             //   Report Size (1)
    0x95, 0x08,             //   Report Count (8)
    0x81, 0x02,             //   Input (Data, Variable, Absolute)
    
    // Reserved (1 byte)
    0x75, 0x08,             //   Report Size (8)
    0x95, 0x01,             //   Report Count (1)
    0x81, 0x01,             //   Input (Constant)
    
    // Key codes (5 bytes)
    0x95, 0x05,             //   Report Count (5)
    0x75, 0x08,             //   Report Size (8)
    0x15, 0x00,             //   Logical Minimum (0)
    0x25, 0x65,             //   Logical Maximum (101)
    0x05, 0x07,             //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,             //   Usage Minimum (0)
    0x29, 0x65,             //   Usage Maximum (101)
    0x81, 0x00,             //   Input (Data, Array, Absolute)
    
    // LED Output (5 bytes)
    0x95, 0x05,             //   Report Count (5)
    0x75, 0x01,             //   Report Size (1)
    0x05, 0x08,             //   Usage Page (LEDs)
    0x19, 0x01,             //   Usage Minimum (Num Lock)
    0x29, 0x05,             //   Usage Maximum (Kana)
    0x91, 0x02,             //   Output (Data, Variable, Absolute)
    
    // LED Padding
    0x95, 0x01,             //   Report Count (1)
    0x75, 0x03,             //   Report Size (3)
    0x91, 0x01,             //   Output (Constant)
    
    0xC0,                   // End Collection
    
    // === Collection 3: Mouse (Report ID 3) ===
    0x05, 0x01,             // Usage Page (Generic Desktop)
    0x09, 0x02,             // Usage (Mouse)
    0xA1, 0x01,             // Collection (Application)
    0x85, 0x03,             //   Report ID (3)
    0x09, 0x01,             //   Usage (Pointer)
    0xA1, 0x00,             //   Collection (Physical)
    
    // Buttons (3 bytes)
    0x95, 0x03,             //   Report Count (3)
    0x75, 0x01,             //   Report Size (1)
    0x05, 0x09,             //   Usage Page (Button)
    0x19, 0x01,             //   Usage Minimum (Button 1)
    0x29, 0x03,             //   Usage Maximum (Button 3)
    0x81, 0x02,             //   Input (Data, Variable, Absolute)
    
    // Padding
    0x95, 0x01,             //   Report Count (1)
    0x75, 0x05,             //   Report Size (5)
    0x81, 0x01,             //   Input (Constant)
    
    // X, Y (2 bytes)
    0x95, 0x02,             //   Report Count (2)
    0x75, 0x10,             //   Report Size (16)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x7F,       //   Logical Maximum (32767)
    0x09, 0x30,             //   Usage (X)
    0x09, 0x31,             //   Usage (Y)
    0x81, 0x06,             //   Input (Data, Variable, Relative)
    
    0xC0,                   //   End Collection
    0xC0                    // End Collection
};
```

### 報告緩衝區配置

```c
// 原始裝置：Report ID 1
// 大小：假設 64 bytes
UCHAR touchReport[64];

// 增強裝置：多個 Report ID
// Report ID 1: Touch (64 bytes)
// Report ID 2: Keyboard (8 bytes)  
// Report ID 3: Mouse (6 bytes)

// 組合緩衝區大小
ULONG totalSize = 64 + 8 + 6; // = 78 bytes

// 或者使用動態大小，根據 Report ID 決定
ULONG GetReportSize(UCHAR reportId) {
    switch(reportId) {
        case 1: return 64;  // Touch
        case 2: return 8;   // Keyboard
        case 3: return 6;   // Mouse
        default: return 0;
    }
}
```

---

## 五、程式碼實作

### Filter Driver 中修改 Descriptor

```c
// 取得原始 Descriptor 並修改
NTSTATUS
ModifyReportDescriptor(
    PHID_DESCRIPTOR Original,
    PHID_DESCRIPTOR Modified,
    PULONG Size
)
{
    // 複製原始 HID Descriptor 標頭
    Modified->bLength = sizeof(HID_DESCRIPTOR);
    Modified->bDescriptorType = HID_HID_DESCRIPTOR_TYPE;
    Modified->bcdHID = Original->bcdHID;
    Modified->bCountry = Original->bCountry;
    Modified->bNumDescriptors = Original->bNumDescriptors + 1;  // 新增一個
    
    // Report Descriptor 改變
    Modified->DescriptorList[0].bReportType = HID_REPORT_DESCRIPTOR_TYPE;
    Modified->DescriptorList[0].wReportLength = sizeof(EnhancedReportDescriptor);
    
    *Size = Modified->bLength;
    
    return STATUS_SUCCESS;
}
```

### 注入虛擬鍵盤報告

```c
VOID
InjectKeyboardReport(
    PUCHAR Buffer,
    ULONG BufferLength
)
{
    // 檢查是否有待發送的按鍵
    if (!g_KeyEventQueue.HasEvents) {
        return;
    }
    
    // 取得下一個事件
    KEY_EVENT event = g_KeyEventQueue.Dequeue();
    
    // 建立鍵盤報告 (Report ID 2)
    UCHAR report[8] = {0};
    report[0] = 0x02;  // Report ID
    report[1] = event.Modifiers;  // Modifiers
    report[2] = event.KeyCode;    // Key code
    
    // 複製到緩衝區
    // 注意：這只是示例，實際需要根據原始 Descriptor 調整
    RtlCopyMemory(Buffer, report, sizeof(report));
}
```

---

## 六、驗證修改後的 Descriptor

### 使用 HID Descriptor Tool 驗證

1. 下載 USB-IF HID Descriptor Tool
2. 匯入修改後的 Descriptor
3. 檢查是否有錯誤

### 使用 Linux hidrd 驗證

```bash
# 轉換為人類可讀格式
hidrd-convert enhanced_descriptor.bin -f text
```

---

## 七、衝突避免

### 常見衝突與解決

| 衝突 | 原因 | 解決 |
|------|------|------|
| Report ID 衝突 | 原始已使用該 ID | 使用未使用的 ID (如 0xFE, 0xFF) |
| Usage 衝突 | 相同 Usage Page | 使用不同的 Collection |
| 報告長度錯誤 | 緩衝區太小 | 重新計算正確大小 |
| 中斷訊息錯誤 | Report Count 錯誤 | 驗證每個欄位 |

### Report ID 分配建議

| 用途 | Report ID |
|------|-----------|
| 原始功能 | 0x01-0x10 |
| 鍵盤 | 0xE0 (避開常用) |
| 滑鼠 | 0xE1 |
| 自訂功能 | 0xF0-0xFF |

---

## 總結

### 修改流程

```
1. 取得原始 HID Descriptor
        │
        ▼
2. 使用工具分析結構
        │
        ▼
3. 設計新增的 Collection
        │
        ▼
4. 合併 Descriptor
        │
        ▼
5. 在 Filter Driver 中實作
        │
        ▼
6. 驗證並測試
```

### 關鍵點

- 使用 **Report ID** 區分不同 Collection
- 確保 **報告長度** 正確
- 避免 Usage/ID 衝突

---

## 參考資源

| 資源 | 連結 |
|------|------|
| USB-IF HID Descriptor Tool | https://usb.org/document-library/hid-descriptor-tool |
| HID Usage Tables | https://usb.org/document-library/hid-usage-tables |
| HID Parser | https://eleccelerator.com/usbdescreqparser/ |

---

*建立時間: 2026-03-25*
