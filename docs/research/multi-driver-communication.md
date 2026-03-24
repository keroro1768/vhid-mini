# 多驅動程式通訊架構

## 問題場景

當我們有兩個獨立的驅動程式（例如）：
1. **實體 HID over I2C 驅動程式** - 負責與真實硬體通訊
2. **虛擬 HID 驅動程式** - 負責模擬輸入裝置

它們安裝在不同匯流排、不同裝置堆疊中，如何相互通訊？

---

## 通訊機制

### 方式一：IOCTL 直接通訊（最推薦）

#### 概念

```
┌─────────────────────┐         ┌─────────────────────┐
│  Driver A           │         │  Driver B           │
│  (虛擬 HID)         │         │  (實體 HID-I2C)    │
│                     │         │                     │
│  Device A           │◄───────►│  Device B           │
│  \\.\VHIDMINI        │  IOCTL  │  \Device\HIDI2C     │
└─────────────────────┘         └─────────────────────┘
```

#### 實作

**Driver A (開啟 Driver B)**

```c
// Driver A 中
HANDLE hDriverB;
NTSTATUS OpenDriverB(VOID)
{
    UNICODE_STRING deviceName;
    RtlInitUnicodeString(&deviceName, L"\\Device\\HIDI2C");
    
    OBJECT_ATTRIBUTES objAttr;
    InitializeObjectAttributes(&objAttr, &deviceName,
        OBJ_KERNEL_HANDLE, NULL, NULL);
    
    return ZwOpenFile(&hDriverB, GENERIC_READ | GENERIC_WRITE,
        &objAttr, NULL, FILE_SHARE_READ | FILE_SHARE_WRITE, 0);
}

NTSTATUS SendToDriverB(PVOID input, ULONG inLen, PVOID output, ULONG outLen)
{
    IO_STATUS_BLOCK ioStatus;
    return ZwDeviceIoControlFile(hDriverB, NULL, NULL, NULL,
        &ioStatus, IOCTL_FORWARD_TO_HID, input, inLen, output, outLen);
}
```

**Driver B (處理 IOCTL)**

```c
// Driver B 中
case IOCTL_FORWARD_TO_HID:
{
    // 接收來自 Driver A 的資料
    PVOID buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG len = Irp->IoctlInformation;
    
    // 處理並回傳
    // ...
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = outputLen;
    break;
}
```

---

### 方式二：共用記憶體 (Shared Memory)

#### 概念

```
┌────────────────────────────────────────────┐
│           NonPaged Pool                    │
│  ┌──────────────────────────────────────┐ │
│  │  Shared Buffer                        │ │
│  │  [Driver A] ◄───────► [Driver B]     │ │
│  └──────────────────────────────────────┘ │
└────────────────────────────────────────────┘
```

#### 實作

**建立共用記憶體**

```c
// 在 Driver A 中
PVOID g_SharedBuffer;
PHANDLE g_SharedEvent;

NTSTATUS CreateSharedMemory(VOID)
{
    // 建立共用緩衝區 (NonPaged)
    g_SharedBuffer = ExAllocatePool(NonPagedPool, 4096);
    
    // 建立 Event 用於同步
    g_SharedEvent = IoCreateNotificationEvent(
        &eventName, &g_SharedEvent);
    
    return STATUS_SUCCESS;
}
```

**配置給 Driver B**

```c
// 透過 IOCTL 傳遞指標
case IOCTL_GET_SHARED_BUFFER:
{
    SHARED_BUFFER_INFO info;
    info.Buffer = g_SharedBuffer;
    info.BufferSize = 4096;
    info.EventHandle = g_SharedEvent;
    
    // 複製到輸出
    RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer,
        &info, sizeof(info));
    
    Irp->IoStatus.Information = sizeof(info);
    break;
}
```

---

### 方式三：命名管道 (Named Pipe)

#### 概念

```
 Driver A                    Driver B
    │                           │
    └────── Named Pipe ─────────┘
```

#### 限制
- 較少用於核心模式間通訊
- 需要額外設定

---

### 方式四：IOMgr 通知

#### 概念

透過作業系統的 I/O Manager 轉發：

```
Driver A 
   │ IRP_MJ_DEVICE_CONTROL (公共 IOCTL)
   ▼
I/O Manager
   │ 路由到 Driver B
   ▼
Driver B
```

---

## 實際應用範例

### 虛擬 HID + 實體 HID-I2C 通訊

#### 架構圖

```
┌─────────────────────────────────────────────────────────┐
│                     應用程式層                           │
└──────────────────────────┬────────────────────────────────┘
                           │
                    ┌──────┴──────┐
                    │ Shared API │
                    └──────┬──────┘
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
        ▼                  ▼                  ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│  虛擬 HID     │  │  實體 HID    │  │  共享緩衝區   │
│  Driver      │◄─►│  I2C Driver  │  │  (Shared Mem) │
│  (VHF/UMDF)  │  │  (KMDF)      │  │               │
└───────────────┘  └───────────────┘  └───────────────┘
```

#### 通訊流程

```
1. 應用程式發送命令
   │
   ▼
2. 虛擬 HID Driver 接收
   │
   ▼
3. 透過 IOCTL 轉發到實體 Driver
   │
   ▼
4. 實體 HID-I2C Driver 執行硬體操作
   │
   ▼
5. 結果回傳給虛擬 Driver
   │
   ▼
6. 虛擬 Driver 產生 HID 報告
   │
   ▼
7. 作業系統當作一般 HID 裝置處理
```

---

## 程式碼範例

### IOCTL 定義

```c
// 共同的 IOCTL 代碼（可放在共用標頭）
#define IOCTL_HID_FORWARD \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_HID_GET_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_HID_SEND_DATA \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
```

### 通訊結構

```c
typedef struct _HID_COMM_PACKET {
    ULONG Command;      // 命令碼
    ULONG DataLength;  // 資料長度
    UCHAR Data[256];   // 資料
} HID_COMM_PACKET, *PHID_COMM_PACKET;
```

---

## 比較表

| 通訊方式 | 複雜度 | 速度 | 適合場景 |
|----------|--------|------|----------|
| IOCTL | 中 | 快 | 大多數場景 |
| Shared Memory | 中 | 最快 | 大量資料 |
| Named Pipe | 高 | 中 | 特殊需求 |
| I/O Manager | 低 | 慢 | 簡單轉發 |

---

## 安全性考量

### 1. 權限檢查

```c
// 確認來源是受信任的驅動程式
if (!IsTrustedDriver(Irp->IoctlHeader->SecurityContext)) {
    return STATUS_ACCESS_DENIED;
}
```

### 2. 驗證資料

```c
// 驗證輸入緩衝區
if (InputLength < sizeof(HID_COMM_PACKET)) {
    return STATUS_INVALID_PARAMETER;
}
```

### 3. 使用 OBJ_KERNEL_HANDLE

```c
// 開啟其他驅動程式時
InitializeObjectAttributes(&objAttr, &deviceName,
    OBJ_KERNEL_HANDLE,  // 重要：核心權限
    NULL, NULL);
```

---

## 結論

| 方式 | 建議 |
|------|------|
| **一般通訊** | IOCTL - 簡單且靈活 |
| **高速資料** | Shared Memory - 效能最佳 |
| **簡易轉發** | I/O Manager - 無需額外程式碼 |

**最推薦**：IOCTL + Shared Memory 組合

---

*建立時間: 2026-03-25*
