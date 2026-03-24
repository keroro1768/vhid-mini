# 虛擬裝置驅動程式安裝方式

## 問題

對於虛擬 HID 裝置，沒有實際硬體，Windows 如何知道要載入哪個驅動程式？

---

## 解答

Windows 提供多種方式來安裝**軟體裝置**（Software Device），不需要實際硬體：

---

## 方法一：ROOT 匯流排枚舉器

### 概念

使用 `ROOT` 列舉器可以建立「虛擬」裝置，屬於軟體類型裝置。

### 硬體 ID 格式

```
ROOT\<設備名稱>
或
ROOT\<設備名稱>\<實例ID>
```

### 範例

```
ROOT\VHIDMINI
ROOT\MYVIRTUALDEVICE\001
```

### INF 設定

```inf
[Manufacturer]
%ManufacturerName% = DeviceList, NTamd64

[DeviceList.NTamd64]
%DeviceName% = DeviceInstall, ROOT\VHIDMINI
```

---

## 方法二：手動安裝 (Manual Install)

### 使用 devcon 安裝

```powershell
# 以系統管理員身份執行
devcon install <INF路徑> "ROOT\設備名稱"
```

### 範例

```powershell
# 安裝虛擬 HID 裝置
devcon install C:\path\to\vhidmini.inf "ROOT\VHIDMINI"
```

### 列出 ROOT 裝置

```powershell
devcon listclass root
```

---

## 方法三：使用類別 (Class) 安裝

### 軟體元件 (Software Device)

如果你的驅動程式是純軟體，可以註冊為 `SoftwareDevice` 類別：

```inf
[Version]
Class = SoftwareDevice
ClassGuid = {4d36e97d-e325-11ce-bfc1-08002be10318}
```

### 然後使用

```powershell
# 透過裝置管理員安裝
# 右鍵 > 新增硬體 > 安裝我手動選擇的硬體 > 從磁片安裝
```

---

## 方法四：HID 特定方式

### 虛擬 HID 裝置

對於 HID 類別，可以使用：

```inf
[Manufacturer]
%VendorName% = DeviceSection

[DeviceSection]
%DeviceName% = InstallSection, HID\VID_1234&PID_5678
```

但這需要實際的 HID 描述符。

### 主要差異

| 方式 | 硬體 ID | 需要實際裝置 | 用途 |
|------|---------|--------------|------|
| ROOT | `ROOT\XXX` | 否 | 軟體裝置 |
| HID | `HID\VID_...` | 否 | 虛擬 HID |
| USB | `USB\VID_...` | 否 | 虛擬 USB |
| PCI | `PCI\VEN_...` | 否 | 虛擬 PCI |

---

## 方法五：PnP 安裝 (軟體驅動)

### 步驟

1. **將驅動程式加入系統**
```powershell
pnputil /add-driver <INF路徑>
```

2. **手動建立軟體裝置**
- 開啟「裝置管理員」
- 點擊「Action」>「Add legacy hardware」
- 選擇「Install the hardware that I manually select」
- 選擇你的硬體 ID

### 程式碼方式

也可以使用 SetupAPI：

```c++
#include <setupapi.h>

// 建立軟體裝置
HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList(NULL, NULL);
SP_DEVINFO_DATA devInfo = {0};
devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

SetupDiCreateDeviceInfo(
    hDevInfo,
    "ROOT\\VHIDMINI",
    NULL, NULL, NULL, DICD_GENERATE_ID,
    &devInfo
);

SetupDiRegisterDeviceInfo(hDevInfo, &devInfo, 0, NULL, NULL, NULL);
```

---

## 方法六：UMDF 反射器 (Reflector)

### 概念

UMDF 驅動程式透過 `WUDFRedir.sys`（反射器）與核心模式通訊。

### 安裝流程

```
1. 使用 pnputil 新增驅動程式套件
2. 建立軟體裝置節點
3. 反射器載入 UMDF DLL
4. 使用者模式驅動程式執行
```

### 狀態查詢

```powershell
# 查看 UMDF 裝置
pnputil /enum-devices /connected
```

---

## 實務建議

### 對於 VHID-Mini 專案

建議使用 **ROOT 列舉器**：

```inf
[Manufacturer]
%VendorName% = DeviceList, NTamd64

[DeviceList.NTamd64]
%DeviceName% = DeviceInstall, ROOT\VHIDMINI
```

### 安裝測試

```powershell
# 測試安裝
devcon install .\driver\inf\vhidmini.inf "ROOT\VHIDMINI"

# 或使用 pnputil
pnputil /add-driver .\driver\inf\vhidmini.inf /install
```

---

## 查詢已安裝的虛擬裝置

```powershell
# 列出所有 ROOT 裝置
devcon findall =root

# 或使用 powershell
Get-PnpDevice -Class SoftwareDevice
```

---

## 常見錯誤

### 錯誤：找不到相符的裝置

**原因**：INF 中的 HardwareID 與實際不符

**解決**：使用 ROOT 列舉器

### 錯誤：拒絕存取

**原因**：權限不足

**解決**：以系統管理員執行

### 錯誤：驅動程式未啟動

**原因**：服務未設定

**解決**：在 INF 中設定服務

---

## 參考資源

| 資源 | 連結 |
|------|------|
| ROOT Enumerator | https://docs.microsoft.com/en-us/windows-hardware/drivers/install/ |
| SetupAPI | https://docs.microsoft.com/en-us/windows-hardware/drivers/setupapi/ |
| devcon | https://docs.microsoft.com/en-us/windows-hardware/drivers/devapps/devcon |

---

*建立時間: 2026-03-25*
