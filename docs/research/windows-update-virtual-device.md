# 虛擬裝置與 Windows Update

## 概述

本文件說明虛擬 HID 驅動程式無法透過 Windows Update 自動推送的原因，以及可行的發布方式。

---

## Windows Update 運作機制

### 推送條件

```
實際硬體連接 → 作業系統偵測硬體ID → 比對 Windows Update 目錄 → 自動下載安裝
     ↑
 需要真實的 VID/PID
```

| 元件 | 說明 |
|------|------|
| **硬體ID** | VID、PID、PCI ID 等，由實際硬體提供 |
| **列舉器** | USB、PCI、ACPI 等，負責偵測硬體 |
| **驅動程式目錄** | Microsoft 維護的驅動資料庫 |

### 推送流程

```
1. 使用者連接設備
2. 系統偵測到硬體 ID (如 USB\VID_1234&PID_5678)
3. 比對 Windows Update 驅動目錄
4. 找到匹配項目 → 自動下載並安裝
5. 找不到 → 使用本機驅動或等待
```

---

## 虛擬裝置的限制

### 為什麼無法自動推送？

| 限制原因 | 說明 |
|----------|------|
| **無實際硬體** | 虛擬裝置沒有真實的 VID/PID |
| **軟體定義ID** | 只能定義 `ROOT\VHIDMINI` 等軟體 ID |
| **列舉器不匹配** | WU 只對 USB/PCI 等實體匯流排生效 |
| **安全性** | 防止未授權軟體自動安裝 |

### 結論

```
虛擬裝置（無實際硬體）→ 無法透過 Windows Update 自動推送
                    → 只能手動安裝
```

---

## 手動安裝方式

### 1. pnputil（推薦）

```powershell
# 以系統管理員身份執行
pnputil /add-driver C:\path\to\vhidmini.inf /install
```

### 2. devcon

```powershell
devcon install C:\path\to\vhidmini.inf "ROOT\VHIDMINI"
```

### 3. 裝置管理員

```
1. 開啟裝置管理員
2. 點擊「動作」>「新增硬體精靈」
3. 選擇「安裝我手動選擇的硬體」
4. 從磁片安裝 → 選擇 INF
```

---

## 發布方式

### 選項 1：手動分發（適合開發/測試）

- 提供 INF + 驅動程式
- 附帶安裝腳本
- 讓使用者手動安裝

### 選項 2：企業部署（適合 IT 管理）

- **Microsoft Intune**：雲端管理
- **SCCM**：系統管理中心組態管理員
- **GPO**：群組原則

### 選項 3：WHQL 認證（適合正式產品）

| 項目 | 說明 |
|------|------|
| **WHQL** | Windows Hardware Quality Lab |
| **優點** | 加入 Windows Update 目錄 |
| **缺點** | 費用高、審核嚴格 |
| **流程** | 送審 → 測試 → 認證 → 發布 |

### 選項 4：Microsoft Store

| 項目 | 說明 |
|------|------|
| **方式** | 封裝為 AppX/MSIX |
| **優點** | 自動更新、使用者信任 |
| **限制** | 需要市集開發者帳號 |

---

## 比較表

| 發布方式 | 自動更新 | 技術難度 | 費用 | 適合對象 |
|----------|----------|----------|------|----------|
| 手動分發 | ❌ | 低 | 免費 | 開發測試 |
| 企業部署 | ✅ | 中 | 中 | IT 部門 |
| WHQL 認證 | ✅ | 高 | 高 | 硬體廠商 |
| Microsoft Store | ✅ | 中 | 中 | 軟體廠商 |

---

## VHID-Mini 的定位

### 目標用途

- **學習**：理解 Windows 驅動程式開發
- **開發**：作為開發測試環境
- **原型**：快速建立虛擬 HID 裝置

### 發布方式

```
提供 GitHub 下載 + 安裝腳本
```

不追求 WU 推送，因為：
1. 沒有實際硬體對應
2. 開發階段不需要 WHQL
3. 目標是讓開發者能快速測試

---

## 常見問題

### Q1: 為什麼有些虛擬裝置可以透過 WU 安裝？

**解答**：這些通常是虛擬化平台提供的驅動（如 VMware Tools、Hyper-V Integration Services），由作業系統供應商內建支援。

### Q2: 可以自己申請加入到 WU 嗎？

**解答**：可以，但需要：
- 有效的程式碼簽章憑證
- 通過 WHQL 測試（對硬體）
- 成為 Microsoft 合作夥伴

### Q3: 是否有辦法模擬硬體 ID 來繞過？

**解答**：技術上可以定義 VID/PID，但：
- 必須申請合法的 VID（花費數千美元）
- 未經認證的驅動會被攔截
- 不建議嘗試

---

## 結論

虛擬 HID 驅動程式的正確定位：

```
學習/測試 → 手動安裝即可
正式產品 → 需要 WHCI/WHQL 認證
```

---

## 參考資源

| 資源 | 連結 |
|------|------|
| WHQL 認證 | https://learn.microsoft.com/en-us/windows-hardware/test-suite/ |
| 驅動程式發布 | https://learn.microsoft.com/en-us/windows-hardware/drivers/ |
| Microsoft Intune | https://learn.microsoft.com/en-us/mem/intune/ |

---

*建立時間: 2026-03-25*
