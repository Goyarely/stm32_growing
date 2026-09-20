# 12 · 存储器分类 + FLASH + IIC 协议

> 对应课程：12-存储器分类+FLASH+IIC协议
> 前置要求：11 ADC + DMA
> 建议耗时：5-7 天

---

## 一、学习内容

### 1. 存储器分类体系

```
存储器
├─ RAM（随机存取，掉电丢失）
│   ├─ SRAM  静态：速度快、成本高、容量小 → MCU 内部运行内存
│   └─ DRAM  动态：需刷新、密度高、便宜 → 电脑内存条
└─ ROM（只读，掉电保存）
    ├─ PROM   一次性可编程
    ├─ EPROM  紫外线擦除
    ├─ EEPROM 电可擦除，可字节级擦写 → AT24C02
    └─ Flash  电可擦除，按块/页擦除 → MCU 内部 Flash、W25Q128
```

**关键对比**

| 特性 | EEPROM | Flash |
|---|---|---|
| 擦写粒度 | 字节 | 页 / 扇区 |
| 擦写寿命 | 100 万次 | 1 万 ~ 10 万次 |
| 写入速度 | 慢 | 快 |
| 常用器件 | AT24C02 | W25Q128、内部 Flash |

**NOR Flash vs NAND Flash**

| | NOR | NAND |
|---|---|---|
| 读取 | 随机读取快，可执行代码 | 页读取，不适合执行 |
| 容量 | 小（MB 级） | 大（GB 级） |
| 用途 | MCU 程序存储、W25Q128 | U 盘、SSD、SD 卡 |

### 2. STM32 内部 Flash

**STM32F103C8T6**：64KB，地址 `0x0800_0000` ~ `0x0800_FFFF`，**页大小 1KB**（共 64 页）。
**STM32F103 大容量（ZE 等）**：页大小 2KB。

**Flash 编程特性**
- 写入前**必须先擦除**（擦除后全为 `0xFF`）
- 擦除以**页**为单位（最小 1KB）
- 写入可以是半字（16 位）或字（32 位）
- **Flash 里不能直接改**，必须"读出来 → 改 → 擦页 → 写回"
- 擦写期间 **CPU 从 Flash 取指会暂停**（程序会卡住几十毫秒，需注意中断响应）

**FPEC 操作流程**

```
1. 解锁：FLASH->KEYR = 0x45670123; FLASH->KEYR = 0xCDEF89AB;
2. 擦除：
   - 页擦除：FLASH->CR |= PG_ER; AR = 页地址; CR |= STRT; 等 BSY 清零
   - 全擦除：CR |= MER
3. 编程：CR |= PG; 往目标地址写半字; 等 BSY 清零
4. 上锁：FLASH->CR |= LOCK
```

**读保护与写保护**
- 读保护（RDP）：防止别人读取 Flash 内容，开启后调试器读不到
- 写保护（WRP）：防止某几页被意外擦除
- 选项字节（Option Bytes）：存放 RDP、WRP、用户数据等配置

### 3. IIC（I2C）协议

**物理层**
- 两根线：`SCL`（时钟）、`SDA`（数据）
- **开漏输出 + 外部上拉电阻**（典型 4.7kΩ）
- 上拉电阻不能省——开漏只能拉低，拉高靠上拉电阻
- 总线空闲时两线都是高电平

**协议帧结构**

```
起始信号 → 7位从机地址 + 读写位 → 应答位 → 数据字节 → 应答位 → ... → 停止信号
```

**关键时序**

| 信号 | 定义 |
|---|---|
| 起始（START） | SCL 高时，SDA 由高变低 |
| 停止（STOP） | SCL 高时，SDA 由低变高 |
| 数据有效 | SCL 高电平期间 SDA 必须稳定 |
| 数据变化 | 只能在 SCL 低电平期间改变 SDA |
| 应答（ACK） | 接收方在第 9 个时钟拉低 SDA |
| 非应答（NACK） | 接收方保持 SDA 高 |

**时钟同步与仲裁**
- 时钟同步：多个主机同时发送时，SCL 线与，低电平周期由最长的那个主机决定
- 总线仲裁：SDA 线与，谁先发出与总线不一致的电平谁退出

**优缺点**
- 优点：线少（2 根）、支持多设备（地址寻址）、有应答机制
- 缺点：速率低（标准 100kHz、快速 400kHz）、开漏上拉上升沿慢、抗干扰弱

---

## 二、学习目标

- [ ] 能说清 SRAM / DRAM / EEPROM / Flash 的区别与典型应用
- [ ] 能独立实现 STM32 内部 Flash 的读写（含擦除）
- [ ] 理解"Flash 写前必擦"的原因，能设计合理的存储方案
- [ ] 能完整画出 IIC 的起始/停止/应答/数据时序图（手绘）
- [ ] 理解 IIC 为什么必须用开漏输出 + 上拉电阻
- [ ] 能说清 IIC 的地址、方向位、应答机制

---

## 三、学习方法

1. **手绘时序图（重点）**：IIC 的四种时序必须能手绘，不看资料。这是后面软件模拟 IIC 的基础。
2. **先理解再抄**：看别人的 IIC 代码前，先自己根据时序图想"起始信号该怎么写"，再对照。
3. **Flash 破坏实验**：往 Flash 某页写入数据，不擦除直接再写，观察数据出错——理解"写前必擦"。
4. **算寿命**：假设每秒写一次 Flash，1 万次寿命能用多久？——理解为什么不能高频写 Flash。
5. **故意去掉上拉电阻**：观察 IIC 通信失败，理解开漏的物理限制。

---

## 四、标准示例

### 1. 内部 Flash 写入（半字编程）

```c
#define FLASH_USER_START_ADDR   0x0800F000U   /* 最后一页起始（1KB 页） */

/* 解锁 */
static void Flash_Unlock(void)
{
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
}

/* 擦除一页 */
static uint8_t Flash_ErasePage(uint32_t page_addr)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = page_addr;
    erase.NbPages     = 1;

    return (HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK) ? 1 : 0;
}

/* 写半字（16 位），必须在擦除后的区域 */
static uint8_t Flash_WriteHalfWord(uint32_t addr, uint16_t data)
{
    return (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, data) == HAL_OK) ? 1 : 0;
}

/* 写一段数据 */
static uint8_t Flash_WriteBuf(uint32_t addr, uint16_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        if (!Flash_WriteHalfWord(addr + i * 2, buf[i])) return 0;
    }
    return 1;
}

/* 使用示例 */
void Flash_Demo(void)
{
    uint16_t wbuf[4] = {0x1234, 0x5678, 0x9ABC, 0xDEF0};
    uint16_t rbuf[4];

    Flash_Unlock();
    Flash_ErasePage(FLASH_USER_START_ADDR);            /* 必须先擦 */
    Flash_WriteBuf(FLASH_USER_START_ADDR, wbuf, 4);
    HAL_FLASH_Lock();                                   /* 用完上锁 */

    for (int i = 0; i < 4; i++) {
        rbuf[i] = *(volatile uint16_t *)(FLASH_USER_START_ADDR + i * 2);
    }
    printf("Flash[0]=0x%04X Flash[3]=0x%04X\r\n", rbuf[0], rbuf[3]);
}
```

**寄存器版（理解 FPEC）**

```c
FLASH->KEYR = 0x45670123;
FLASH->KEYR = 0xCDEF89AB;              /* 解锁 */

FLASH->CR |= FLASH_CR_PER;             /* 页擦除模式 */
FLASH->AR  = FLASH_USER_START_ADDR;
FLASH->CR |= FLASH_CR_STRT;
while (FLASH->SR & FLASH_SR_BSY);

FLASH->CR |= FLASH_CR_PG;              /* 编程模式 */
*(volatile uint16_t *)FLASH_USER_START_ADDR = 0x1234;
while (FLASH->SR & FLASH_SR_BSY);
FLASH->CR &= ~FLASH_CR_PG;

FLASH->CR |= FLASH_CR_LOCK;            /* 上锁 */
```

### 2. 简单的 Flash 参数存储方案（避免频繁擦写）

```c
/* 应用场景：保存用户参数，但怕频繁擦写损耗 Flash
   方案：双页轮换 + 魔数校验 */

#define PARAM_PAGE_A     0x0800F000U
#define PARAM_PAGE_B     0x0800F400U
#define PARAM_MAGIC      0x55AA

typedef struct {
    uint16_t magic;
    uint16_t cycle;        /* 擦写轮换计数 */
    uint16_t led_period;
    uint16_t crc;
} Param_t;

/* 写入前先读旧值比对，值没变就不写 —— 显著延长寿命 */
```

### 3. IIC 时序图（文字版，务必手绘一遍）

```
起始信号：
SCL  ‾‾‾‾‾‾‾\__________
SDA  ‾‾‾‾\_____________
            ↑ SCL 为高时 SDA 下降沿

停止信号：
SCL  __________/‾‾‾‾‾‾‾
SDA  _____________/‾‾‾‾
                  ↑ SCL 为高时 SDA 上升沿

发送 1 位数据：
     SCL  __/‾‾‾‾\______
     SDA  ──<  数据稳定  >──
          ↑ SCL 低电平时改变 SDA，高电平时数据必须稳定

应答：
主机发完 8 位后释放 SDA，
从机在第 9 个 SCL 低电平期间拉低 SDA 表示 ACK
```

### 4. IIC 引脚硬件配置（开漏 + 上拉）

```c
/* 硬件 IIC：引脚必须配成复用开漏 */
GPIO_InitTypeDef gpio = {0};
gpio.Pin   = GPIO_PIN_6 | GPIO_PIN_7;      /* PB6=SCL, PB7=SDA */
gpio.Mode  = GPIO_MODE_AF_OD;              /* 复用开漏 */
gpio.Pull  = GPIO_PULLUP;
gpio.Speed = GPIO_SPEED_FREQ_HIGH;
HAL_GPIO_Init(GPIOB, &gpio);

/* 软件模拟 IIC：普通开漏输出 */
gpio.Mode = GPIO_MODE_OUTPUT_OD;
```

> **上拉电阻必须外部接**（4.7kΩ），STM32 内部的弱上拉（约 40kΩ）太弱，高速通信会失败。

---

## 五、自我检测

1. SRAM 和 DRAM 的区别是什么？EEPROM 和 Flash 的核心区别？
2. NOR Flash 和 NAND Flash 各适合什么场景？
3. 为什么 Flash 写入前必须先擦除？擦除后是什么值？
4. STM32F103C8T6 的 Flash 页大小是多少？总共多少页？
5. Flash 擦写时程序会有什么现象？为什么？
6. 假设每秒写一次 Flash，1 万次寿命能用多久？该怎么优化？
7. IIC 为什么必须用开漏输出？如果改成推挽会怎样？
8. 手绘 IIC 的起始、停止、发送 1、发送 0、应答五种时序。
9. IIC 的从机地址一般是几位？加读写位后总共几位？
10. 读保护（RDP）和写保护（WRP）分别防什么？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| Flash 写入的数据不对 | 写入前没有擦除；写到非 `0xFF` 的位置 |
| Flash 写入一直失败 | 没解锁 FPEC；没清错误标志；地址超出范围 |
| 擦写 Flash 时串口丢数据 | 擦写期间 CPU 停住，中断被延迟，通信超时 |
| Flash 频繁擦写导致损坏 | 设计问题，应加"值变化才写"或双页轮换 |
| IIC 完全无响应 | 没接上拉电阻；SDA/SCL 接反；从机地址错 |
| IIC 读到的数据错位 | 时序延时不够（尤其高波特率）；上拉电阻太大 |
| 程序写 Flash 后跑飞 | 擦到了存放代码的页（地址计算错误） |
| 复用开漏不能用 | 忘记开 AFIO 时钟；上拉不够 |

---

## 七、拓展练习

1. **Flash 参数管理器**：实现 `Param_Save(key, value)` / `Param_Load(key)`，带魔数和 CRC 校验。
2. **Flash 日志**：在 Flash 里记录最近 100 条开机时间，满了自动覆盖最旧的。
3. **手绘 IIC 时序图**：不看资料画完整总线时序（起始 + 地址 + ACK + 数据 + ACK + 停止），拍照存进本章目录。
4. **IIC 扫描器**：软件模拟 IIC，扫描 `0x00`~`0x7F` 地址，打印所有响应的从机地址。
5. **上拉电阻实验**：用 10kΩ、4.7kΩ、1kΩ 三种上拉，观察波形上升沿与通信成功率。
6. **Flash 寿命计算报告**：为你设想的应用（比如每 10 秒存一次数据）计算 Flash 能用多久，设计方案延长到 10 年以上。
