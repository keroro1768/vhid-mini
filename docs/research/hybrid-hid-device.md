# HID 複合架構：實體 + 虛擬裝置

## 問題

如何讓一個 HID over I2C 裝置，同時掛載另一個獨立的虛擬軟體元件？

---

## 解答

可以的！有幾種架構方式：

---

## 方案一：Composite HID Device（複合裝置）

### 概念

在同一個實體裝置上，建立多個 HID Collections（頂層集合），每個 Collection 代表一個獨立功能。

### HID Descriptor 設計

```c
// 範例：鍵盤 + 觸控板複合裝置
UCHAR CompositeHIDDescriptor[] = {
    // ====== Collection 1: 鍵盤 ======
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    // ... 鍵盤報告描述符 ...
    0xC0,              // End Collection
    
    // ====== Collection 2: 觸控板 ======
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    // ... 觸控板報告描述符 ...
    0xC0               // End Collection
};
```

### 優點
- 單一實體裝置
- 系統自動識別為多個 HID 裝置

### 缺點
- 需要修改韌體的 HID Descriptor
- 所有功能綁定在一起

---

## 方案二：雙驅動程式架構（推薦）

### 概念

安裝**兩個獨立的驅動程式**，分別負責：
1. **實體 HID over I2C** → 實際硬體通訊
2. **虛擬 HID** → 軟體模擬的輸入裝置

### 架構圖

```
┌─────────────────────────────────────────────┐
│              系統匯流排                      │
│                                             │
│  ┌──────────────────┐  ┌─────────────────┐ │
│  │ HID over I2C     │  │ Virtual HID     │ │
│  │ (實體裝置)        │  │ (軟體元件)       │ │
│  │                  │  │                 │ │
│  │ I2C 通訊         │  │ VHF/UMDF        │ │
│  │ KMDF 驅動       │  │ 驅動程式        │ │
│  └──────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────┘
```

### 實作方式

#### 1. 實體 HID over I2C
- 使用 Microsoft 內建的 `hidi2c.sys`
- 或自訂 KMDF 驅動程式

#### 2. 虛擬 HID
- 使用 **Virtual HID Framework (VHF)**
- 或自訂 **UMDF** 驅動程式

### INF 安裝

```inf
; 實體裝置
[DeviceList.HW]
%DeviceName% = Install1, ACPI\I2C_HID_DEVICE

; 虛擬裝置
[DeviceList.Virtual]
%DeviceName% = Install2, ROOT\VHIDMINI
```

---

## 方案三：USB Composite Device

### 概念

如果使用 USB，可以使用複合裝置：

```
USB Device
├── Interface 0: HID Keyboard
├── Interface 1: HID Mouse
└── Interface 2: Vendor Specific (虛擬功能)
```

### Windows 載入邏輯

```
USB\VID_XXXX&PID_YYYY
    ↓
載入 USBCCGP (Composite Generic Parent)
    ↓
建立多個 PDO
    ├── USB\VID_XXXX&PID_YYYY&MI_00  (鍵盤)
    ├── USB\VID_XXXX&PID_YYYY&MI_01  (滑鼠)
    └── USB\VID_XXXX&PID_YYYY&MI_02  (虛擬)
```

---

## 方案四：Software Component + Filter Driver

### 概念

```
┌─────────────────────────────────────────────┐
│           應用程式 (Software)                │
│                    ↓                         │
│         Filter Driver (可選)                 │
│                    ↓                         │
│     ┌─────────────────────────────┐         │
│     │   真實 HID over I2C 裝置     │         │
│     │   (實際硬體)                 │         │
│     └─────────────────────────────┘         │
└─────────────────────────────────────────────┘
```

### 使用情境

- 攔截或修改真實裝置的輸入
- 注入虛擬輸入
- 錄製/回放功能

---

## 比較表

| 方案 | 複雜度 | 彈性 | 需要硬體修改 | 範例 |
|------|--------|------|--------------|------|
| Composite HID | 低 | 低 | 是 | 鍵盤+滑鼠一體 |
| 雙驅動程式 | 高 | 高 | 否 | VHF + HID-I2C |
| USB Composite | 中 | 中 | 是 | 多介面裝置 |
| Filter Driver | 高 | 高 | 否 | 輸入模擬 |

---

## VHID-Mini 專案應用

### 目標架構

```
┌─────────────────────────────────────┐
│       HID over I2C 實體裝置           │
│  (觸控螢幕感測器)                     │
└─────────────────────────────────────┘
                    │
                    ↓
         ┌───────────────────┐
         │  軟體過濾/虛擬化元件 │
         │  (Software       │
         │   Component)     │
         └───────────────────┘
                    │
                    ↓
         ┌───────────────────┐
         │  Virtual HID      │
         │  (虛擬鍵盤/滑鼠)   │
         └───────────────────┘
```

### 實作重點

1. **分離職責**
   - 真實裝置：處理原始感測器資料
   - 虛擬化層：將資料轉換為 HID 輸入

2. **通訊機制**
   - 共享記憶體（Shared Memory）
   - 命名管道（Named Pipe）
   - IOCTL

---

## 常見問題

### Q1: 兩個驅動程式會衝突嗎？

**解答**：不會，只要硬體 ID 不同。系統會分別載入。

### Q2: 可以讓虛擬裝置依賴真實裝置嗎？

**解答**：可以，在 INF 中設定：
```inf
[Install2]
Depends = Install1
```

### Q3: 如何確保開機順序？

**解答**：使用服務相依性：
```inf
[Install2.Services]
AddService = vhidmini, , ServiceFlags
Depends17 = 實體驅動服務名
```

---

## 結論

| 需求 | 建議方案 |
|------|----------|
| 單一韌體控制 | Composite HID |
| 獨立開發/測試 | 雙驅動程式 |
| USB 硬體 | USB Composite |
| 輸入攔截 | Filter Driver |

**推薦**：方案二（雙驅動程式），彈性最大，可獨立開發測試。

---

## 參考資源

| 資源 | 連結 |
|------|------|
| VHF 文件 | https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/virtual-hid-framework--vhf- |
| HID Collections | https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/ |
| USB Composite | https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/ |

---

*建立時間: 2026-03-25*
