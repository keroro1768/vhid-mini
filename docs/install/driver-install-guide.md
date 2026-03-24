# Virtual HID 安裝與卸載指南

## 概述

本指南說明如何在 Windows 上安裝虛擬 HID 驅動程式，即使沒有實際硬體也能進行測試。

---

## 一、安裝前準備

### 1. 啟用測試簽名 (Test Signing)

由於驅動程式沒有正式簽名，必須啟用測試簽名：

```powershell
# 以系統管理員身份執行
bcdedit /set testsigning on

# 確認設定
bcdedit /enum | findstr testsigning
```

**重開機**後生效

### 2. 檢查驅動程式狀態

```powershell
# 查看目前已安裝的 OEM 驅動程式
pnputil /enum-drivers
```

---

## 二、安裝方式

### 方法一：pnputil (推薦)

PnPUtil 是 Windows 內建的驅動程式管理工具。

#### 基本語法

```powershell
# 以系統管理員身份執行

# 1. 新增驅動程式套件
pnputil /add-driver <INF路徑> [/install]

# 2. 查看已安裝的驅動程式
pnputil /enum-devices

# 3. 移除驅動程式
pnputil /delete-driver <oem編號> [/uninstall]
```

#### 範例

```powershell
# 安裝驅動程式
pnputil /add-driver C:\path\to\vhidmini.inf /install

# 或不立即安裝（先加入系統）
pnputil /add-driver C:\path\to\vhidmini.inf
```

#### 常見參數

| 參數 | 說明 |
|------|------|
| `/add-driver <INF>` | 新增驅動程式套件 |
| `/install` | 安裝後立即安裝裝置 |
| `/uninstall` | 卸載並移除套件 |
| `/delete-driver` | 刪除驅動程式套件 |
| `/enum-drivers` | 列出已安裝的驅動程式 |
| `/enum-devices` | 列出已安裝的裝置 |

### 方法二：devcon

devcon 是另一個強大的命令列工具：

```powershell
# 安裝
devcon install <INF路徑> <硬體ID>

# 移除
devcon remove <硬體ID>

# 查看狀態
devcon status <硬體ID>
```

#### 範例

```powershell
# 使用預設硬體 ID 安裝
devcon install C:\path\to\vhidmini.inf USB\VID_1234&PID_5678

# 或使用 devcon 產生的 ID
devcon install c:\drivers\mydevice.inf "root\mydevice"
```

---

## 三、INF 檔案結構

### 基本結構

```inf
[Version]
Signature   = "$Windows NT$"
Class        = HIDClass
ClassGuid   = {745a17a0-74d3-11d0-b6fe-00a0c955f3e7}
Provider    = %VendorName%
DriverVer   = 01/01/2026,1.0.0.0

[Manufacturer]
%VendorName% = DeviceSection

[DeviceSection]
%DeviceName% = DeviceInstall, HardwareID

[DeviceInstall]
AddReg = AddRegSection

[AddReg]
; 登錄檔設定

[Strings]
VendorName = "Your Company"
DeviceName = "Virtual HID Device"
```

### 關鍵欄位

| 欄位 | 說明 |
|------|------|
| Class | 裝置類別 (HIDClass) |
| ClassGuid | 類別 GUID |
| HardwareID | 硬體識別碼 (如 USB\VID_1234&PID_5678) |

---

## 四、硬體 ID 與匹配

### 硬體 ID 格式

```
USB\VID_XXXX&PID_YYYY
PCI\VEN_XXXX&DEV_YYYY
ROOT\DEVICE_NAME
```

### 如何指定

在 INF 的 `[DeviceSection]`：

```inf
; 精確匹配
DeviceName = InstallSection, USB\VID_1234&PID_5678

; 或萬用字元
DeviceName = InstallSection, USB\VID_1234&PID_*
```

---

## 五、INF 安裝問題排查

### 常見錯誤

#### 1. 簽名錯誤

```
The third-party INF does not contain digital signature information
```

**解決**：
- 啟用測試簽名
- 或暫時停用驅動程式簽章強制

```powershell
# 停用簽章強制 (開發階段)
bcdedit /set nointegritychecks on
```

#### 2. 硬體 ID 不匹配

```
No more data is available.
```

**解決**：
- 檢查 INF 中的 HardwareID 是否正確
- 確認 VID/PID 與程式碼中一致

#### 3. 權限不足

```
Access is denied.
```

**解決**：
- 以系統管理員身份執行
- 確認 INF 檔案權限

---

## 六、自動化腳本

### 安裝腳本

```powershell
# install-vhid.ps1

param(
    [string]$InfPath = ".\driver\vhidmini.inf"
)

# 檢查管理員權限
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Error "請以系統管理員身份執行此腳本"
    exit 1
}

# 啟用測試簽名
Write-Host "啟用測試簽名..."
bcdedit /set testsigning on

# 新增驅動程式
Write-Host "新增驅動程式套件..."
pnputil /add-driver $InfPath /install

# 列出結果
Write-Host "已安裝的驅動程式："
pnputil /enum-drivers

Write-Host "`n完成！請重新開機。"
```

### 卸載腳本

```powershell
# uninstall-vhid.ps1

param(
    [string]$DriverName = "vhidmini"
)

# 找出 OEM 編號
$oem = pnputil /enum-drivers | Select-String -Pattern $DriverName

if ($oem) {
    Write-Host "移除驅動程式..."
    # 取出 OEM 編號
    $oemNum = ($oem -replace '.*oem(\d+).*', '$1')
    pnputil /delete-driver oem$oemNum /uninstall
    Write-Host "已完成卸載"
} else {
    Write-Host "找不到驅動程式: $DriverName"
}
```

---

## 七、測試環境設定

### 虛擬機器

建議使用 VM 進行測試：

| VM 軟體 | 優點 |
|---------|------|
| **VMware** | 快照方便 |
| **Hyper-V** | Windows 原生 |
| **VirtualBox** | 免費 |

### VM 設定

1. **停用安全開機 (Secure Boot)**
2. **啟用測試簽名**
3. **允許未簽名驅動程式**

---

## 八、相關指令速查

| 指令 | 用途 |
|------|------|
| `pnputil /add-driver <inf> /install` | 安裝 |
| `pnputil /enum-drivers` | 列出驅動 |
| `pnputil /delete-driver <oem#> /uninstall` | 卸載 |
| `devcon status <id>` | 查詢狀態 |
| `devcon remove <id>` | 移除裝置 |
| `bcdedit /set testsigning on` | 啟用測試簽名 |
| `bcdedit /enum` | 開機選項 |

---

## 九、參考資源

| 資源 | 連結 |
|------|------|
| PnPUtil 文件 | https://learn.microsoft.com/en-us/windows-hardware/drivers/devapps/pnputil |
| devcon 文件 | https://learn.microsoft.com/en-us/windows-hardware/drivers/devapps/devcon |
| 測試簽名 | https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option |

---

*建立時間: 2026-03-25*
