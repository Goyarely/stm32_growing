# 15 · RFID + ESP8266 + HAL + STM32CubeMX

> 对应课程：15-RFID+ESP8266+HAL+STM32Cube
> 前置要求：14 SPI + W25Q128
> 建议耗时：6-8 天

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)）
>
> | 用途 | 笔记示例 | **实板情况 / 改用** |
> |---|---|---|
> | RC522 SCK / MOSI / MISO | PA5 / PA7 / PA6 | **冲突**：已被板载 **LoRa** 占用 |
> | RC522 CS（NSS/SDA） | **PB12** | **冲突**：PB12 = SW2 按键 |
> | RC522 RST | PB11 | **PB11 空闲，可用** |
> | | | **推荐：RC522 也挂到 SPI1 总线**（与 LoRa 共用 SCK/MOSI/MISO），**CS = PA11**，**RST = PB11** |
> | ESP8266 通信 | USART2，外接模块 | **板载 ESP8266_12E**：把「NB-IOT/WIFI 通道选择」跳线拨到 **WIFI 侧**，它就挂在 **USART2（PA2=TX / PA3=RX）** 上，**不需要外接模块** |
> | 门禁继电器 / 电磁锁 | PA8 | **冲突**：PA8 = LED1 → 改用 **PB8 / PB9 / PC13**（都空闲） |
>
> **ESP8266 供电提醒**
> ESP8266 发射瞬间电流可达 **300mA 以上**。板载模块如果反复重启、AT 指令没响应，**先怀疑供电**：
> 在模块 3.3V 就近并联一个 **470uF 电解电容**，往往立刻就好。这是 ESP8266 最经典的一个坑。
>
> **换板更省事**
> 如果用**节点板（终端）**：它板载了真正的**继电器（PC14）**，"刷卡开锁 + 蜂鸣器提示（PB9）"这一套做起来比中控板更真实、更少飞线。

---

## 一、学习内容

### 1. RFID RC522 模块

| 参数 | 值 |
|---|---|
| 频率 | 13.56MHz（高频 HF） |
| 接口 | **SPI**（也有 IIC/UART 版本） |
| 支持卡 | MIFARE S50（1KB）、S70（4KB）、Ultralight |
| 读距 | 3~5cm |
| 供电 | 3.3V |

**接线（SPI 模式）**

| RC522 | STM32 |
|---|---|
| SCK | PA5 |
| MOSI | PA7 |
| MISO | PA6 |
| NSS/SDA | PB12（软件片选） |
| RST | 任意 GPIO |
| 3.3V / GND | 3.3V / GND |

> **注意**：RC522 只能 3.3V 供电，5V 会烧毁。

**MIFARE S50 卡存储结构**

```
1KB = 16 个扇区 × 4 个块 × 16 字节

扇区 0：
  块 0：厂商数据（UID + 校验，只读）
  块 1、2：数据块
  块 3：密钥 A(6B) + 存取控制(4B) + 密钥 B(6B)

扇区 1~15：结构相同（块 0~2 数据，块 3 密钥）
默认密钥：FF FF FF FF FF FF
```

**卡片操作流程**

```
1. 寻卡（REQA）      → 检测到场卡片
2. 防冲突（Anticoll）→ 获取 UID
3. 选卡（Select）    → 选中该卡
4. 认证（Auth）      → 用密钥校验权限
5. 读 / 写块
6. 休眠 / 停止
```

**常用 PICC 指令**

| 指令 | 码 | 作用 |
|---|---|---|
| `PICC_REQIDL` | 0x26 | 寻天线区内未休眠的卡 |
| `PICC_REQALL` | 0x52 | 寻所有卡 |
| `PICC_ANTICOLL` | 0x93 | 防冲突，获取 UID |
| `PICC_SELECTTAG` | 0x93 | 选卡 |
| `PICC_AUTHENT1A` | 0x60 | A 密钥认证 |
| `PICC_AUTHENT1B` | 0x61 | B 密钥认证 |
| `PICC_READ` | 0x30 | 读块 |
| `PICC_WRITE` | 0xA0 | 写块 |
| `PICC_HALT` | 0x50 | 休眠 |

**常用寄存器**

| 寄存器 | 地址 | 作用 |
|---|---|---|
| `CommandReg` | 0x01 | 启动/停止命令 |
| `ComIEnReg` / `ComIrqReg` | 0x02 / 0x04 | 中断使能/标志 |
| `ErrorReg` | 0x06 | 错误标志 |
| `FIFODataReg` | 0x09 | 数据 FIFO |
| `FIFOLevelReg` | 0x0A | FIFO 中数据量 |
| `ControlReg` | 0x0C | 控制 |
| `BitFramingReg` | 0x0D | 位帧调整 |
| `ModeReg` | 0x11 | 发送/接收模式 |
| `TxControlReg` | 0x14 | **天线驱动（bit0/1 控制）** |
| `TModeReg` / `TPrescalerReg` | 0x2A / 0x2B | 定时器 |
| `TReloadRegH/L` | 0x2C / 0x2D | 定时器重装值 |
| `VersionReg` | 0x37 | 版本号（正常应为 0x91/0x92） |

> **第一步永远是读 `VersionReg`**，读到 0x91 或 0x92 才说明 SPI 通信正常。

### 2. ESP8266 WiFi 模块

| 参数 | 值 |
|---|---|
| 供电 | **3.3V**（峰值电流可达 300mA，需独立 LDO） |
| 接口 | UART（AT 指令） |
| 默认波特率 | 115200（老固件 9600） |
| 功能 | STA / AP / STA+AP 三种模式 |

**AT 指令流程**

```
1. AT                              → 测试，返回 OK
2. AT+CWMODE=1                     → 设为 Station 模式
3. AT+CWJAP="ssid","password"      → 连接路由器
4. AT+CIFSR                        → 查询获得的 IP
5. AT+CIPMUX=0                     → 单连接模式
6. AT+CIPSTART="TCP","ip",port     → 建立 TCP 连接
7. AT+CIPSEND=len                  → 声明要发 len 字节，然后发数据
8. AT+CIPCLOSE                     → 关闭连接
```

**其他常用指令**

| 指令 | 作用 |
|---|---|
| `AT+RST` | 重启模块 |
| `AT+GMR` | 查看固件版本 |
| `AT+CIPMODE=1` | 开启透传模式（配合 `AT+CIPSEND`） |
| `AT+UART=115200,8,1,0,0` | 修改波特率 |
| `AT+CIPSTA?` | 查询 IP 配置 |
| `AT+CWJAP?` | 查询当前连接的 AP |

**透传模式（推荐）**：`AT+CIPMODE=1` + `AT+CIPSEND` 后进入，之后串口发的数据直接走 TCP，无需每条数据都加 AT 指令。

**常见问题**
- 供电不足：ESP8266 发送瞬间电流冲高，必须用独立 3.3V LDO（不能从 STM32 的 3.3V 取电）
- 电平：ESP8266 是 3.3V，与 STM32 直连即可
- 波特率：默认 115200，程序里串口要匹配

### 3. HAL 库 vs 标准库

| 对比项 | 标准库（StdPeriph） | HAL 库 |
|---|---|---|
| 状态 | 已停止维护（F1 之后无） | ST 主推，全系列统一 |
| 抽象层次 | 偏底层，接近寄存器 | 更高层，屏蔽细节 |
| 代码风格 | `GPIO_Init(GPIOA, &cfg)` | `HAL_GPIO_Init(GPIOA, &cfg)` |
| 状态返回 | 多为 void | 返回 `HAL_StatusTypeDef` |
| 超时机制 | 无 | 普遍带 timeout 参数 |
| 配合工具 | 无 | **STM32CubeMX** |
| 代码体积 | 小 | 大（多一层封装） |
| 跨系列移植 | 差 | 好（F1/F4/H7 风格一致） |

**HAL 库的三个函数族**

| 前缀 | 含义 | 示例 |
|---|---|---|
| `HAL_xxx_Init` | 初始化 | `HAL_UART_Init` |
| `HAL_xxx_Process` | 轮询方式处理 | `HAL_UART_Transmit` |
| `HAL_xxx_IT` | 中断方式 | `HAL_UART_Receive_IT` |
| `HAL_xxx_DMA` | DMA 方式 | `HAL_ADC_Start_DMA` |
| `HAL_xxx_Callback` | 回调（弱函数，用户重写） | `HAL_UART_RxCpltCallback` |

**HAL 的三层结构**

```
应用层（你的 main.c）
   ↓ 调用
HAL 库层（stm32f1xx_hal_xxx.c）
   ↓ 调用
LL / CMSIS 层（寄存器操作）
   ↓
硬件寄存器
```

### 4. STM32CubeMX

**核心功能**
- 图形化引脚分配（自动检测冲突）
- 时钟树图形配置（实时计算频率，防止超频）
- 外设参数配置
- 中间件集成（FreeRTOS、FatFS、USB、LwIP）
- 一键生成工程（Keil / IAR / Makefile / CubeIDE）

**使用流程**

```
1. 新建工程 → 选择芯片 STM32F103C8Tx
2. Pinout 视图 → 点击引脚选择功能
3. System Core → RCC 选 HSE 晶振 / SYS 选 SWD
4. Clock Configuration → 配置 72MHz
5. 配置外设参数（USART1 / TIM3 / I2C1 ...）→ 打开 NVIC
6. Project Manager → 工程名、路径、工具链（MDK-ARM 或 Makefile）
7. 勾选 "Generate peripheral initialization as a pair of .c/.h files"
8. GENERATE CODE
```

**生成的代码结构**

```
Core/
  ├─ Inc/  main.h、stm32f1xx_hal_conf.h、stm32f1xx_it.h
  └─ Src/  main.c、stm32f1xx_it.c、stm32f1xx_hal_msp.c、系统时钟
Drivers/
  ├─ STM32F1xx_HAL_Driver/
  └─ CMSIS/
```

**重要规则**
- 用户代码必须写在 `/* USER CODE BEGIN xxx */` 和 `/* USER CODE END xxx */` 之间
- 重新生成代码时，这些区间内的内容会被保留，区间外的会被覆盖
- `MX_xxx_Init()` 由 CubeMX 生成，不要手工修改

**与 EIDE 结合的方式**
1. CubeMX 生成 MDK-ARM 工程 → 用 EIDE 的"导入 Keil 项目"功能
2. 或 CubeMX 选 Makefile 工具链 → EIDE 里配置 Makefile 构建
3. 或只生成代码，手工在 EIDE 里配置包含路径与源文件

---

## 二、学习目标

- [ ] 能用 RC522 读取卡片的 UID，并显示在 OLED 上
- [ ] 能实现门禁逻辑：读取卡号 → 比对白名单 → 控制继电器
- [ ] 能用 ESP8266 的 AT 指令连上 WiFi 并与服务器通信
- [ ] 能说清 HAL 库与标准库的设计差异
- [ ] 能用 STM32CubeMX 从零生成一个工程并成功编译
- [ ] 能说清 `USER CODE BEGIN/END` 区间的作用

---

## 三、学习方法

1. **读 ID 验证通信**：RC522 先读 `VersionReg`（0x91/0x92），ESP8266 先发 `AT` 返回 OK——任何模块都先做最简通信验证。
2. **串口助手当中间人**：ESP8266 先用 USB-TTL + 串口助手手工发 AT 指令打通全流程，再写 MCU 代码。
3. **逻辑分析仪抓 SPI**：RC522 不响应时，抓 SCK/MOSI/MISO/CS 四线波形，一眼看出问题。
4. **CubeMX 逆向学习**：用 CubeMX 生成一个 UART 工程，把它生成的 `MX_USART1_UART_Init()` 与你手写的版本对比，理解 HAL 的标准结构。
5. **两种方式对照**：同一个功能分别用手写 HAL 和 CubeMX 生成，体会效率差异。

---

## 四、标准示例

### 1. RC522 基础驱动（要点）

```c
#define RC522_RST_PORT  GPIOB
#define RC522_RST_PIN   GPIO_PIN_11
#define RC522_CS_PORT   GPIOB
#define RC522_CS_PIN    GPIO_PIN_12

static void RC522_WriteReg(uint8_t addr, uint8_t val)
{
    RC522_CS_L();
    SPI_ReadWriteByte((addr << 1) & 0x7E);   /* bit7=0 表示写 */
    SPI_ReadWriteByte(val);
    RC522_CS_H();
}

static uint8_t RC522_ReadReg(uint8_t addr)
{
    uint8_t val;
    RC522_CS_L();
    SPI_ReadWriteByte(((addr << 1) & 0x7E) | 0x80);   /* bit7=1 表示读 */
    val = SPI_ReadWriteByte(0xFF);
    RC522_CS_H();
    return val;
}

static void RC522_SetBitMask(uint8_t reg, uint8_t mask)
{
    RC522_WriteReg(reg, RC522_ReadReg(reg) | mask);
}

static void RC522_ClearBitMask(uint8_t reg, uint8_t mask)
{
    RC522_WriteReg(reg, RC522_ReadReg(reg) & (~mask));
}

static void RC522_AntennaOn(void)
{
    uint8_t temp = RC522_ReadReg(0x14);      /* TxControlReg */
    if (!(temp & 0x03)) {
        RC522_SetBitMask(0x14, 0x03);        /* 打开天线驱动 */
    }
}

static void RC522_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = RC522_RST_PIN | RC522_CS_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    RC522_CS_H();

    /* 硬复位 */
    HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(50);

    /* 软复位 */
    RC522_WriteReg(0x01, 0x0F);              /* CommandReg = SoftReset */
    while (RC522_ReadReg(0x01) & 0x10);      /* 等复位完成 */

    /* 定时器配置（内部定时器，用于超时） */
    RC522_WriteReg(0x2A, 0x8D);              /* TModeReg: TAuto=1, TPrescaler_Hi */
    RC522_WriteReg(0x2B, 0x3E);              /* TPrescalerReg */
    RC522_WriteReg(0x2C, 30);                /* TReloadRegH */
    RC522_WriteReg(0x2D, 0);                 /* TReloadRegL */

    RC522_WriteReg(0x15, 0x40);              /* TxASKReg: 100% ASK */
    RC522_WriteReg(0x11, 0x3D);              /* ModeReg */

    RC522_AntennaOn();

    printf("RC522 Version: 0x%02X\r\n", RC522_ReadReg(0x37));   /* 应为 0x91/0x92 */
}

/* 寻卡：返回 0 表示成功找到卡 */
static uint8_t RC522_Request(uint8_t req_mode, uint8_t *tag_type)
{
    uint8_t status;
    uint16_t back_bits;

    RC522_WriteReg(0x0D, 0x07);              /* BitFramingReg：7 位 */
    RC522_WriteReg(0x09, req_mode);          /* FIFODataReg：写入指令 */
    RC522_WriteReg(0x01, 0x0C);              /* CommandReg：Transceive */

    RC522_SetBitMask(0x0D, 0x80);            /* 启动发送 */

    uint16_t i = 2000;
    do {
        status = RC522_ReadReg(0x04);        /* ComIrqReg */
        i--;
    } while ((i != 0) && !(status & 0x30) && !(status & 0x01));

    RC522_ClearBitMask(0x0D, 0x80);

    if (i != 0) {
        if (!(RC522_ReadReg(0x06) & 0x1B)) { /* ErrorReg 无错误 */
            back_bits = RC522_ReadReg(0x0A); /* FIFOLevelReg */
            if ((back_bits == 0x10) || (back_bits == 0x04)) {
                *tag_type = RC522_ReadReg(0x09);
                return 0;
            }
        }
    }
    return 1;
}

/* 防冲突，读取 UID（4 字节） */
static uint8_t RC522_Anticoll(uint8_t *uid)
{
    uint8_t status, i;
    uint8_t ser_num[5];

    RC522_WriteReg(0x0D, 0x00);
    RC522_WriteReg(0x09, 0x93);              /* PICC_ANTICOLL */
    RC522_WriteReg(0x09, 0x20);
    RC522_WriteReg(0x01, 0x0C);

    RC522_SetBitMask(0x0D, 0x80);

    uint16_t j = 2000;
    do {
        status = RC522_ReadReg(0x04);
        j--;
    } while ((j != 0) && !(status & 0x30) && !(status & 0x01));

    RC522_ClearBitMask(0x0D, 0x80);

    if (j != 0) {
        if (!(RC522_ReadReg(0x06) & 0x1B)) {
            for (i = 0; i < 5; i++) ser_num[i] = RC522_ReadReg(0x09);
            /* 校验 BCC */
            if (ser_num[0] ^ ser_num[1] ^ ser_num[2] ^ ser_num[3] == ser_num[4]) {
                for (i = 0; i < 4; i++) uid[i] = ser_num[i];
                return 0;
            }
        }
    }
    return 1;
}
```

### 2. 门禁主逻辑

```c
/* 白名单卡号 */
static const uint8_t CARD_WHITELIST[][4] = {
    {0x12, 0x34, 0x56, 0x78},
    {0xAB, 0xCD, 0xEF, 0x01},
};

uint8_t Check_Card(void)
{
    uint8_t tag_type[2];
    uint8_t uid[4];

    if (RC522_Request(0x52, tag_type) != 0) return 0;   /* 没卡 */
    if (RC522_Anticoll(uid) != 0)          return 0;

    printf("Card UID: %02X %02X %02X %02X\r\n", uid[0], uid[1], uid[2], uid[3]);

    for (int i = 0; i < sizeof(CARD_WHITELIST) / 4; i++) {
        if (memcmp(uid, CARD_WHITELIST[i], 4) == 0) return 1;   /* 认证通过 */
    }
    return 0;
}

/* 主循环 */
while (1) {
    if (Check_Card()) {
        /* 开锁 */
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);   /* 继电器/电磁锁 */
        printf("Access GRANTED\r\n");
        HAL_Delay(2000);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    }
    HAL_Delay(300);
}
```

### 3. ESP8266 连接 WiFi 并上传数据

```c
/* 串口发送 AT 指令并等待期望响应 */
static uint8_t ESP_SendAT(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    printf("AT >> %s\r\n", cmd);
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 1000);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 100);

    /* 等待响应（用第 07 章的接收框架收集到 g_rx_buf） */
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        if (g_rx_done) {
            g_rx_done = 0;
            g_rx_buf[g_rx_len] = '\0';
            if (strstr((char *)g_rx_buf, expect) != NULL) {
                g_rx_len = 0;
                return 1;
            }
            g_rx_len = 0;
        }
        HAL_Delay(10);
        elapsed += 10;
    }
    return 0;
}

static uint8_t ESP_Init(void)
{
    HAL_Delay(1000);
    if (!ESP_SendAT("AT", "OK", 2000))                  return 0;
    if (!ESP_SendAT("AT+CWMODE=1", "OK", 2000))         return 0;
    if (!ESP_SendAT("AT+CWJAP=\"MyWiFi\",\"12345678\"", "OK", 15000)) return 0;
    if (!ESP_SendAT("AT+CIPMUX=0", "OK", 2000))         return 0;
    if (!ESP_SendAT("AT+CIPSTART=\"TCP\",\"192.168.1.100\",8080", "CONNECT", 8000)) return 0;
    return 1;
}

/* 发送数据到服务器 */
static uint8_t ESP_SendData(const char *data)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", (int)strlen(data));
    if (!ESP_SendAT(cmd, ">", 3000)) return 0;         /* 等 ">" 提示符 */

    HAL_UART_Transmit(&huart2, (uint8_t *)data, strlen(data), 2000);
    return ESP_SendAT("", "SEND OK", 5000);
}

/* 采集温湿度上报 */
while (1) {
    uint8_t t, h;
    if (DHT11_Read(&t, &h)) {
        char json[64];
        snprintf(json, sizeof(json), "{\"temp\":%d,\"humi\":%d}", t, h);
        ESP_SendData(json);
    }
    HAL_Delay(10000);
}
```

### 4. CubeMX 生成代码的典型结构

```c
/* main.c —— CubeMX 生成，注意 USER CODE 区间 */

/* USER CODE BEGIN Includes */
#include "my_app.h"
/* USER CODE END Includes */

int main(void)
{
  HAL_Init();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  /* 你的初始化代码写在这里 */
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN WHILE */
    /* 你的主循环代码写在这里 */
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* USER CODE END 3 */
  }
}

/* 用户自定义函数放在文件末尾的 USER CODE 4 区间 */
/* USER CODE BEGIN 4 */
void My_Function(void)
{
}
/* USER CODE END 4 */
```

> **规则**：所有自定义代码必须写在 `USER CODE BEGIN/END` 之间，否则重新生成时会被删除。

---

## 五、自我检测

1. RC522 的 SPI 读写地址为什么要左移 1 位？如何区分读和写？
2. 读 `VersionReg` 得到什么值说明通信正常？
3. MIFARE S50 卡的存储结构是怎样的？密钥在哪个块？
4. 寻卡、防冲突、选卡、认证四步各起什么作用？
5. ESP8266 为什么必须用独立 3.3V 供电？
6. 说出至少 6 条常用的 ESP8266 AT 指令。
7. 透传模式和普通 AT 模式有什么区别？
8. HAL 库相比标准库的三个主要差异是什么？
9. `HAL_xxx_Callback` 是什么机制？为什么要用弱函数？
10. CubeMX 生成代码时，为什么自定义代码必须写在 `USER CODE` 区间？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| RC522 读版本号是 0x00 或 0xFF | SPI 模式不对；CS/RST 接线错；供电不是 3.3V |
| 寻不到卡 | 天线未打开（TxControlReg）；卡离得太远；卡是低频卡 |
| 认证总是失败 | 密钥不对（默认应为全 FF）；扇区号算错 |
| ESP8266 无响应 | 供电不足（需独立 LDO）；波特率不匹配；TX/RX 没交叉 |
| 连不上 WiFi | SSID/密码错；仅支持 2.4GHz（不支持 5G） |
| `AT+CIPSEND` 后卡住 | 没等 `>` 提示符就发数据 |
| 响应解析错乱 | 没做超时；接收缓冲区没清空；用 `strstr` 时没加 `\0` |
| CubeMX 重新生成后代码丢失 | 自定义代码写在了 `USER CODE` 区间外 |
| CubeMX 生成后编译报错 | 工具链路径没配；缺少 HAL 源文件 |

---

## 七、拓展练习

1. **刷卡门禁系统（完整版）**：RC522 + OLED 显示卡号 + 继电器开锁 + 蜂鸣器提示 + 白名单存 W25Q128。
2. **刷卡记录**：每次刷卡把 UID + RTC 时间写入 W25Q128，通过串口导出。
3. **WiFi 数据上云**：ESP8266 连接 MQTT 服务器，把温湿度上报到物联网平台。
4. **远程控制**：手机通过 WiFi 发指令控制 LED / 继电器。
5. **CubeMX 重构**：把第 07 章的串口工程用 CubeMX 重新生成一遍，对比手写代码的差异。
6. **HAL 库追源码**：在 `HAL_UART_Transmit()` 里打断点，追到寄存器层，画出调用链。
7. **写一份 HAL 与标准库对照表**：挑 5 个外设，把两种库的初始化写法并列对比。

---

## 补充：从本章开始的思维转变

学到这里，"写代码"的比重应该下降，"**选方案、读手册、查资料**"的比重应该上升：

| 能力 | 具体表现 |
|---|---|
| 读手册 | 拿到新模块，先找 datasheet 的寄存器表和时序图 |
| 拆解协议 | 把复杂协议拆成"起始-地址-数据-结束"的标准结构 |
| 复用代码 | 前 14 章的驱动应该已经能拼装出新功能 |
| 选择工具 | 知道什么场景用 HAL、什么时候直接操作寄存器 |

下一章的 FreeRTOS，是从"写功能"到"设计系统"的又一次转变。
