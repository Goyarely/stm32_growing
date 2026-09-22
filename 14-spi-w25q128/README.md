# 14 · 硬件 IIC + SPI + W25Q128

> 对应课程：14-硬件IIC+SPI+W25Q128
> 前置要求：13 软件 IIC + AT24C02 + OLED
> 建议耗时：5-7 天

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)；学习用板 = **中控板（网关）**）
>
> **笔记里两套 SPI 引脚在实板上都冲突，本章统一改成第三套。**
>
> | 用途 | 笔记示例 | **实板情况** |
> |---|---|---|
> | 软件 SPI | PB12 / PB13 / PB14 / PB15 | **冲突**：PB12=SW2、PB13=SW3、PB15=LED2（PB14 空闲） |
> | 硬件 SPI1 | PA5 / PA6 / PA7 + CS=PB12 | **冲突**：PA5/6/7 已接板载 **LoRa**；PB12 是按键 |
>
> **统一后的方案（推荐）**
>
> | 用途 | 引脚 | 说明 |
> |---|---|---|
> | SCK / MOSI / MISO | **PA5 / PA7 / PA6** | **与板载 LoRa 共用 SPI1 总线**，不用另占引脚 |
> | W25Q128 CS | **PA11**（或 PB3） | 空闲引脚，独立片选 |
> | LoRa NSS | PA4 | 板上已有 |
>
> 如果不想共用总线，也可以纯软件 SPI：`SCK=PA11 / MOSI=PA12 / MISO=PA15 / CS=PB3`（四个都空闲）。
>
> **一条铁律**
> **同一时刻只能有一个 SPI 从机被片选。** LoRa 的 `NSS(PA4)` 和 W25Q128 的 `CS(PA11)` 必须互斥，
> 否则两个从机同时驱动 MISO 线，读到的一定是垃圾。这也是"**多个 SPI 设备共总线**"的设计范式：**SCK/MOSI/MISO 共享，CS 各自独立**。

---

## 一、学习内容

### 1. 硬件 IIC vs 软件模拟 IIC

| 对比项 | 硬件 IIC | 软件模拟 IIC |
|---|---|---|
| 实现 | STM32 内部 I2C 外设 | GPIO 翻转时序 |
| CPU 占用 | 低（可中断/DMA） | 高（全程占用） |
| 速度 | 稳定高速 | 受延时函数限制 |
| 灵活性 | 引脚固定，受限于复用表 | 任意引脚 |
| 调试难度 | 高（F1 的 I2C 有已知缺陷） | 低（时序自己控制） |
| 中断影响 | 无 | 易被中断破坏时序 |

**STM32F1 硬件 I2C 的已知问题**
- 某些版本存在总线死锁、`BUSY` 标志永久置位
- 需要在出错时执行"总线恢复"流程（发 9 个时钟 + 重新初始化外设）
- 很多实际项目仍然使用软件模拟 IIC 就是因为这个

**硬件 IIC 关键寄存器**：`CR1`/`CR2`、`SR1`/`SR2`、`DR`、`CCR`、`TRISE`、`OAR1`/`OAR2`。

### 2. SPI 协议

**物理层：4 根线**

| 线 | 全称 | 作用 |
|---|---|---|
| `SCK` | Serial Clock | 时钟，主机产生 |
| `MOSI` | Master Out Slave In | 主机→从机 |
| `MISO` | Master In Slave Out | 从机→主机 |
| `CS`/`NSS` | Chip Select | 片选，低有效 |

**特点**
- **全双工**：收发同时进行（与 IIC 半双工不同）
- **无应答机制**：没有 ACK，靠片选和协议约定
- **速度快**：F1 的 SPI 最高 18MHz（APB2 72MHz / 4）
- **一主多从**：每个从机一根独立 CS 线
- 推挽输出（不像 IIC 需要上拉）

**四种模式（CPOL / CPHA）**

| 模式 | CPOL | CPHA | 空闲电平 | 采样边沿 |
|---|---|---|---|---|
| Mode 0 | 0 | 0 | 低 | 第一个边沿（上升） |
| Mode 1 | 0 | 1 | 低 | 第二个边沿（下降） |
| Mode 2 | 1 | 0 | 高 | 第一个边沿（下降） |
| Mode 3 | 1 | 1 | 高 | 第二个边沿（上升） |

- **CPOL**：时钟极性，空闲时 SCK 是高还是低
- **CPHA**：时钟相位，在第几个边沿采样数据
- **W25Q128 用 Mode 0（CPOL=0, CPHA=0）**

**SPI 相关的关键寄存器**：`CR1`（`SPE`/`MSTR`/`BR`/`CPOL`/`CPHA`/`SSM`/`SSI`）、`SR`（`TXE`/`RXNE`/`BSY`）、`DR`。

### 3. W25Q128 SPI Flash

| 参数 | 值 |
|---|---|
| 容量 | 128Mbit = **16MB** |
| 页 | 256 字节（编程单位） |
| 扇区 | **4KB**（最小擦除单位） |
| 块 | 64KB |
| 接口 | SPI，最高 104MHz（普通模式） |
| 寿命 | 10 万次擦写，数据保存 20 年 |

**地址空间**：24 位地址（0x000000 ~ 0xFFFFFF），正好覆盖 16MB。

**常用指令**

| 指令 | 码 | 作用 |
|---|---|---|
| 读状态寄存器 1 | 0x05 | 查 `BUSY` 位（bit0） |
| 写使能 | 0x06 | **每次写/擦除前必须发** |
| 读数据 | 0x03 | 读任意长度 |
| 页编程 | 0x02 | 写最多 256 字节（**页内**） |
| 扇区擦除 | 0x20 | 擦除 4KB |
| 块擦除 | 0xD8 | 擦除 64KB |
| 整片擦除 | 0xC7 | 擦除全部（很慢） |
| 读 ID | 0x9F | 读厂商与器件 ID（0xEF4018） |
| 写状态寄存器 | 0x01 | 配置 |
| 复位使能/复位 | 0x66 / 0x99 | 软复位 |

**关键时序规则**
1. 写/擦除前必须发 **写使能（0x06）**
2. 发完写命令后必须**轮询状态寄存器的 BUSY 位**直到为 0
3. 页编程**不能跨页**（和 AT24C02 一样，超出会回卷）
4. 扇区擦除需要约 **45ms**，页编程约 **0.7ms**

**读写流程**

```
读：CS↓ → 0x03 → 24位地址 → 读数据... → CS↑
写：CS↓ → 0x06(写使能) → CS↑          ← 必须先发写使能
    CS↓ → 0x02 → 24位地址 → 数据(≤256B) → CS↑
    → 轮询 BUSY 直到 0
擦：CS↓ → 0x06 → CS↑
    CS↓ → 0x20 → 24位地址 → CS↑
    → 轮询 BUSY 直到 0
```

---

## 二、学习目标

- [ ] 能说清硬件 IIC 与软件 IIC 的优劣，能做出合理选择
- [ ] 能手绘 SPI 的四种模式波形图
- [ ] 能独立编写软件模拟 SPI 与硬件 SPI 两套驱动
- [ ] 能读懂 W25Q128 手册，实现读 ID、读、写、擦全流程
- [ ] 能实现"跨页自动处理"的通用写函数
- [ ] 能用 W25Q128 做一个小容量文件系统（键值存储）

---

## 三、学习方法

1. **先用软件模拟 SPI 打通**：GPIO 翻转实现，完全掌握时序后再用硬件 SPI。
2. **读 ID 是第一步**：任何 SPI 设备，先读器件 ID 确认通信正常，再进行复杂操作。
3. **逻辑分析仪必用**：SPI 是 4 线，逻辑分析仪能同时看 SCK/MOSI/MISO/CS，一眼看出问题。
4. **先擦后写**：先做"擦除扇区 → 读回全 0xFF → 写入 → 读回校验"的完整流程，再封装。
5. **对比 IIC 和 SPI**：把同一份数据用两种总线各存一次，对比速度（SPI 明显快）。

---

## 四、标准示例

### 1. 软件模拟 SPI 核心

```c
#define SPI_SCK_H()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET)
#define SPI_SCK_L()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET)
#define SPI_MOSI_H()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET)
#define SPI_MOSI_L()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET)
#define SPI_MISO_R()  HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14)
#define SPI_CS_H()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET)
#define SPI_CS_L()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET)

/* 交换一个字节：Mode 0（CPOL=0, CPHA=0，上升沿采样） */
static uint8_t SPI_SwapByte(uint8_t byte)
{
    uint8_t recv = 0;

    for (uint8_t i = 0; i < 8; i++) {
        /* 1. 在时钟低电平时准备数据 */
        if (byte & 0x80) SPI_MOSI_H();
        else             SPI_MOSI_L();
        byte <<= 1;

        SPI_SCK_H();                        /* 2. 上升沿，从机采样 MOSI */
        __NOP(); __NOP();

        recv <<= 1;
        if (SPI_MISO_R()) recv |= 1;        /* 3. 同时从机输出 MISO，主机读取 */

        SPI_SCK_L();                        /* 4. 下降沿 */
        __NOP(); __NOP();
    }
    return recv;
}
```

### 2. 硬件 SPI 初始化（Mode 0，主机）

```c
static SPI_HandleTypeDef hspi1;

static void SPI1_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_SPI1_CLK_ENABLE();        /* SPI1 在 APB2 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* SCK(PA5) / MOSI(PA7) → 复用推挽 */
    gpio.Pin   = GPIO_PIN_5 | GPIO_PIN_7;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* MISO(PA6) → 浮空/上拉输入 */
    gpio.Pin  = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* CS(PB12) → 普通推挽输出，软件控制 */
    gpio.Pin   = GPIO_PIN_12;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    SPI_CS_H();                          /* 空闲时片选拉高 */

    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;      /* CPOL = 0 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;       /* CPHA = 0 → Mode 0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;          /* 软件控制 CS */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* 72/8 = 9MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi1);
}

static uint8_t SPI_ReadWriteByte(uint8_t tx)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, 100);
    return rx;
}
```

### 3. W25Q128 驱动

```c
/* ---------- 基础操作 ---------- */
#define W25Q_CS_L()   SPI_CS_L()
#define W25Q_CS_H()   SPI_CS_H()

static uint8_t W25Q_ReadSR1(void)
{
    uint8_t sr;
    W25Q_CS_L();
    SPI_ReadWriteByte(0x05);            /* 读状态寄存器 1 */
    sr = SPI_ReadWriteByte(0xFF);
    W25Q_CS_H();
    return sr;
}

static void W25Q_WriteEnable(void)
{
    W25Q_CS_L();
    SPI_ReadWriteByte(0x06);
    W25Q_CS_H();
}

/* 等待忙碌结束（写/擦除后必须调用） */
static void W25Q_WaitBusy(void)
{
    while (W25Q_ReadSR1() & 0x01);      /* bit0 = BUSY */
}

/* ---------- 读 ID（第一步验证） ---------- */
uint32_t W25Q_ReadID(void)
{
    uint32_t id;
    W25Q_CS_L();
    SPI_ReadWriteByte(0x9F);
    id  = (uint32_t)SPI_ReadWriteByte(0xFF) << 16;   /* 厂商 ID：0xEF */
    id |= (uint32_t)SPI_ReadWriteByte(0xFF) << 8;    /* 类型：0x40 */
    id |= (uint32_t)SPI_ReadWriteByte(0xFF);         /* 容量：0x18 */
    W25Q_CS_H();
    return id;                          /* W25Q128 应为 0xEF4018 */
}

/* ---------- 读数据（任意长度，可跨页） ---------- */
void W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25Q_CS_L();
    SPI_ReadWriteByte(0x03);
    SPI_ReadWriteByte((addr >> 16) & 0xFF);
    SPI_ReadWriteByte((addr >> 8)  & 0xFF);
    SPI_ReadWriteByte( addr        & 0xFF);
    for (uint32_t i = 0; i < len; i++) buf[i] = SPI_ReadWriteByte(0xFF);
    W25Q_CS_H();
}

/* ---------- 页编程（不超过 256 字节且不跨页） ---------- */
static void W25Q_PageProgram(uint32_t addr, uint8_t *buf, uint16_t len)
{
    W25Q_WriteEnable();                 /* 必须！ */
    W25Q_CS_L();
    SPI_ReadWriteByte(0x02);
    SPI_ReadWriteByte((addr >> 16) & 0xFF);
    SPI_ReadWriteByte((addr >> 8)  & 0xFF);
    SPI_ReadWriteByte( addr        & 0xFF);
    for (uint16_t i = 0; i < len; i++) SPI_ReadWriteByte(buf[i]);
    W25Q_CS_H();
    W25Q_WaitBusy();
}

/* ---------- 通用写（自动处理跨页） ---------- */
void W25Q_Write(uint32_t addr, uint8_t *buf, uint32_t len)
{
    while (len) {
        uint16_t page_remain = 256 - (addr % 256);       /* 本页剩余空间 */
        uint16_t write_len   = (len < page_remain) ? len : page_remain;

        W25Q_PageProgram(addr, buf, write_len);

        addr += write_len;
        buf  += write_len;
        len  -= write_len;
    }
}

/* ---------- 扇区擦除（4KB，最小擦除单位） ---------- */
void W25Q_SectorErase(uint32_t addr)
{
    W25Q_WriteEnable();
    W25Q_CS_L();
    SPI_ReadWriteByte(0x20);
    SPI_ReadWriteByte((addr >> 16) & 0xFF);
    SPI_ReadWriteByte((addr >> 8)  & 0xFF);
    SPI_ReadWriteByte( addr        & 0xFF);
    W25Q_CS_H();
    W25Q_WaitBusy();                    /* 约 45ms */
}
```

### 4. 完整测试流程

```c
void W25Q_Test(void)
{
    uint8_t wbuf[300];
    uint8_t rbuf[300];

    printf("W25Q ID = 0x%06lX\r\n", W25Q_ReadID());      /* 应输出 0xEF4018 */

    for (int i = 0; i < 300; i++) wbuf[i] = i;

    uint32_t addr = 0x000000;
    W25Q_SectorErase(addr);                              /* 先擦除 */
    W25Q_Write(addr, wbuf, 300);                         /* 跨页写入 */
    W25Q_Read(addr, rbuf, 300);

    for (int i = 0; i < 300; i++) {
        if (wbuf[i] != rbuf[i]) {
            printf("Verify FAIL at %d: w=0x%02X r=0x%02X\r\n", i, wbuf[i], rbuf[i]);
            return;
        }
    }
    printf("W25Q test PASS\r\n");
}
```

### 5. 简易键值存储（实用封装）

```c
/* 把 W25Q 当一个小型参数区使用
   扇区 0 作为索引表，从扇区 1 开始存数据 */

#define W25Q_KV_BASE      0x001000        /* 从 4KB 处开始 */
#define W25Q_KV_SLOT_SIZE 512             /* 每个键占 512 字节 */

typedef struct {
    uint32_t key;                         /* 键，0xFFFFFFFF 表示空 */
    uint32_t len;
    uint8_t  data[W25Q_KV_SLOT_SIZE - 8];
    uint32_t crc;
} KV_Slot_t;
```

---

## 五、自我检测

1. 硬件 IIC 和软件 IIC 各自的优缺点是什么？什么场景选哪个？
2. SPI 有几种模式？CPOL 和 CPHA 分别控制什么？
3. W25Q128 用哪种 SPI 模式？为什么？
4. SPI 为什么不需要上拉电阻，而 IIC 需要？
5. SPI 的 CS 引脚作用是什么？多个从机怎么接？
6. W25Q128 的页、扇区、块分别是多大？哪个是擦除单位？
7. 写 W25Q128 之前必须发哪个指令？不发会怎样？
8. 为什么每次操作后都要轮询状态寄存器的 BUSY 位？
9. 页编程为什么不能跨页？怎么规避？
10. 16MB 容量需要多少位地址？24 位地址够吗？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| 读 ID 不是 0xEF4018 | SPI 模式不对（应是 Mode 0）；CS 没拉低；接线错 |
| 写入后读回全是 0xFF | 忘记发写使能 0x06；或没轮询 BUSY 就做下一步 |
| 写入后数据只有前 256 字节对 | 页编程跨页了，超出部分回卷 |
| 擦除后立刻写入失败 | 没等 BUSY 清零（扇区擦除要 45ms） |
| 偶尔写入失败 | 没做"写使能 → 写 → 查 BUSY"的完整流程 |
| 硬件 IIC 卡死在 BUSY | F1 的 I2C 已知问题，需加超时与总线恢复 |
| SPI 读到的数据错位 | CPHA 配置错误（Mode 0 vs Mode 3） |
| 多次读写后数据错乱 | 地址计算溢出，或没处理跨页 |

---

## 七、拓展练习

1. **SPI 速度测试**：同一份 4KB 数据，用软件模拟 SPI 与硬件 SPI 各写一次，用定时器计时对比。
2. **简易文件系统**：在 W25Q128 上实现 `fs_open` / `fs_write` / `fs_read` / `fs_delete`。
3. **日志系统**：把运行日志循环写入 W25Q128，实现"设备黑匣子"。
4. **固件存储区**：把 W25Q128 规划成"参数区 + 日志区 + 升级固件区"，为 Bootloader 做准备。
5. **对比实验**：用 AT24C02 和 W25Q128 各存 1KB 数据，对比耗时与占用，写一份选型报告。
6. **SPI 多从机**：在同一 SPI 总线上挂 W25Q128 和一个 SPI 显示屏，用不同 CS 切换。

---

## 补充：存储选型速查

| 需求 | 推荐 |
|---|---|
| 存几十字节参数，写入不频繁 | AT24C02（IIC EEPROM） |
| 存几 KB ~ 几 MB 数据/日志 | W25Q128（SPI Flash） |
| 存几字节且需频繁写 | MCU 内部 Flash（配合双页轮换） |
| 需频繁擦写且量大 | 加一片 FRAM 或 SRAM + 电池 |
