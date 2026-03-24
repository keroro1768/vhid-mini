# Virtual HID Mini Driver - 第一版

## 專案簡介

本專案實作一個 Windows Virtual HID 裝置，可模擬簡易的 HID 裝置（如鍵盤、滑鼠），讓上層應用程式可以與虛擬裝置通訊。

**目標**：建立可安裝、可測試的基礎版本

---

## 參考專案

| 專案 | 連結 | 說明 |
|------|------|------|
| Microsoft vhidmini2 | [GitHub](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/vhidmini2) | UMDF HID Minidriver 範本 |
| Microsoft hidusbfx2 | [GitHub](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/hidusbfx2) | USB FX2 範本 |
| VHF (Virtual HID Framework) | [MS Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/virtual-hid-framework--vhf-) | 微軟官方 VHF |

---

## 環境需求

### 1. Windows SDK
- 下載並安裝 Windows SDK
- 需要包含 Debugging Tools

### 2. Windows Driver Kit (WDK)
- 下載對應 Windows SDK 版本的 WDK
- WDK 會整合到 Visual Studio

### 3. Visual Studio 2022
- 安裝「使用 C++ 的桌面開發」
- 安裝「單元測試工具」

---

## 建置步驟

### 1. 安裝順序

```
1. Windows SDK (最新穩定版)
2. Visual Studio 2022
3. WDK (自動整合到 VS)
```

### 2. 驗證安裝

確認 VS2022 出現：
- 驅動程式 > Windows Driver
- 測試 > Windows Driver Testing

---

## 專案結構

```
vhid-mini/
├── driver/              # 核心驅動程式
│   ├── vhidmini.c      # 主要程式碼
│   ├── vhidmini.h     # 標頭檔
│   ├── device.c       # 裝置物件
│   ├── driver.c       # 驅動程式入口
│   └── inf           # 安裝資訊
│       └── vhidmini.inf
├── app/                # 使用者模式測試應用
│   ├── vhidtest.cpp
│   └── CMakeLists.txt
├── README.md
└── BUILD.md
```

---

## 安裝與卸載

### 安裝驅動程式

```powershell
# 開啟測試簽名 (開發階段)
bcdedit /set testsigning on
restart-computer

# 安裝
pnputil /add-driver vhidmini.inf /install

# 或使用 devcon
devcon install vhidmini.inf root\vhidmini
```

### 卸載驅動程式

```powershell
# 使用裝置管理員
# 或命令列
devcon remove root\vhidmini
```

---

## HID 報告描述符

基礎版使用簡化的報告描述符：

```c
// 簡化版鍵盤報告描述符 (8 bytes)
UCHAR ReportDescriptor[] = {
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
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Constant)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x05, 0x06,        //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        //   Usage Minimum (Reserved)
    0x29, 0x65,        //   Usage Maximum (Application)
    0x81, 0x00,        //   Input (Data, Array, Absolute)
    0xC0               // End Collection
};
```

---

## 測試應用

### 開啟裝置

```c++
#include <windows.h>
#include <hid.h>

HANDLE OpenVirtualDevice()
{
    // 開啟 HID 裝置
    HANDLE hDevice = CreateFile(
        "\\\\.\\VHIDMINI",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL);
    
    return hDevice;
}
```

### 傳送報告

```c++
BOOL SendKeyReport(HANDLE hDevice, UCHAR key)
{
    UCHAR report[9] = {0x01, 0x00, 0x00, key, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    DWORD written;
    return WriteFile(hDevice, report, sizeof(report), &written, NULL);
}
```

---

## 常見問題

### Q1: 驅動程式無法安裝

**解決**：
- 確認已啟用測試簽名
- 檢查 INF 語法
- 查看事件檢視器

### Q2: 裝置無法開啟

**解決**：
- 確認驅動程式已正確安裝
- 檢查裝置名稱是否正確
- 使用管理員權限執行

### Q3: 編譯錯誤

**解決**：
- 確認 WDK 正確整合
- 檢查 Windows SDK 路徑

---

## 學習資源

| 資源 | 連結 |
|------|------|
| WDK 下載 | https://developer.microsoft.com/windows-hardware/windows-driver-kit/ |
| UMDF 文件 | https://docs.microsoft.com/windows-hardware/drivers/wdf/ |
| HID 文件 | https://docs.microsoft.com/windows-hardware/drivers/hid/ |
| Windows-driver-samples | https://github.com/microsoft/Windows-driver-samples |

---

## 下一步

- [ ] 完成基礎驅動程式編譯
- [ ] 製作安裝與卸載腳本
- [ ] 實作簡單的按鍵模擬
- [ ] 建立測試應用程式
- [ ] 建立 CI/CD 自動化建置

---

*建立時間: 2026-03-25*
