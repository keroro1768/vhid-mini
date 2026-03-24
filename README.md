# Virtual HID Mini Driver Project

## 專案目標

建立一個 Windows Virtual HID 裝置驅動程式，可在沒有實際硬體的情況下安裝與測試。

---

## 專案結構

```
vhid-mini/
├── docs/                    # 文件
│   ├── install/            # 安裝指南
│   │   └── driver-install-guide.md
│   ├── develop/            # 開發文件
│   │   └── TODO.md
│   └── research/          # 研究資料
│       └── TODO.md
├── driver/                 # 驅動程式原始碼
│   ├── vhidmini.c
│   └── inf/
│       └── vhidmini.inf
├── app/                    # 測試應用程式
│   └── TODO.md
├── README.md
└── BUILD.md
```

---

## 當前進度

### 已完成
- [x] 專案架構規劃
- [x] 基礎驅動程式框架
- [x] INF 安裝檔案
- [x] 安裝/卸載文件

### 待完成
- [ ] 完成 HID 報告描述符
- [ ] 實作 IOCTL 處理
- [ ] 開發測試應用程式
- [ ] 實際編譯測試

---

## 快速開始

### 1. 環境需求
- Windows 10/11
- Visual Studio 2022
- Windows Driver Kit (WDK)
- Windows SDK

### 2. 編譯
```powershell
# 開啟 Developer Command Prompt for VS2022
msbuild vhidmini.sln /p:Configuration=Debug /p:Platform=x64
```

### 3. 安裝
```powershell
# 啟用測試簽名
bcdedit /set testsigning on
restart-computer

# 安裝驅動程式
pnputil /add-driver .\driver\inf\vhidmini.inf /install
```

### 4. 測試
```powershell
# 查看裝置
pnputil /enum-devices
```

---

## 重要說明

### 為什麼需要測試簽名？

Windows 預設只允許有數位簽名的驅動程式。開發階段需要：

1. **測試簽名模式** - 允許未簽名驅動程式
2. **測試簽名證書** - 自我簽署測試用證書

### 開發完成後

正式發布需要：
- **EV Code Signing Certificate** - 程式碼簽章憑證
- **Microsoft Windows Hardware Compatibility** - 取得 WHQL 認證（可選）

---

## 文件索引

| 文件 | 說明 |
|------|------|
| [驅動程式安裝指南](docs/install/driver-install-guide.md) | INF 安裝完整教學 |
| [HID 報告描述符](docs/research/hid-report-descriptor.md) | HID 報告格式說明 |
| [WDK 環境設定](docs/develop/wdk-setup.md) | 開發環境設定 |

---

## 參考專案

| 專案 | 連結 |
|------|------|
| Microsoft vhidmini2 | https://github.com/microsoft/Windows-driver-samples/tree/main/hid/vhidmini2 |
| Windows-driver-samples | https://github.com/microsoft/Windows-driver-samples |
| WDK Document | https://learn.microsoft.com/en-us/windows-hardware/drivers/ |

---

## 貢獻指南

歡迎提交 Issue 和 Pull Request！

---

## 授權

MIT License

---

*最後更新: 2026-03-25*
