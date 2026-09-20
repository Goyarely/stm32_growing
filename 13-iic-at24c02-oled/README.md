# 13 · IIC 实现 + AT24C02 + OLED

> 对应课程：13-IIC实现+AT24C02+OLED
> 前置要求：12 存储器 + FLASH + IIC 协议
> 建议耗时：6-8 天

---

## 一、学习内容

### 1. 软件模拟 IIC（本课重点）

**为什么先学软件模拟？**
- 完全掌握时序细节，理解每一个延时为什么存在
- 不受硬件 IIC 的 bug 影响（STM32F1 的硬件 I2C 有已知缺陷）
- 引脚可以任意选择，不受复用表限制
- 便于调试和移植

**核心函数清单**

| 函数 | 作用 |
|---|---|
| `IIC_Init()` | 初始化 SCL/SDA 为开漏输出 |
| `IIC_Start()` | 起始信号 |
| `IIC_Stop()` | 停止信号 |
| `IIC_SendByte()` | 发送一个字节 |
| `IIC_RecvByte()` | 接收一个字节 |
| `IIC_WaitAck()` | 等待应答 |
| `IIC_SendAck()` / `IIC_SendNack()` | 发送应答/非应答 |
| `IIC_Delay()` | 微秒级延时 |

**关键理解**
- SDA 是**双向**线：发送时是输出，接收时应变输入（或使用开漏读 IDR）
- 读 SDA 时要先拉高 SCL，再读
- 每个位都要保证 SCL 高电平期间 SDA 稳定

### 2. AT24C02 EEPROM

| 参数 | 值 |
|---|---|
| 容量 | 2Kbit = **256 字节** |
| 组织 | 32 页 × 8 字节/页 |
| 接口 | IIC，最高 400kHz |
| 设备地址 | `1010 A2 A1 A0 R/W`，A 引脚接地时 → **写 0xA0 / 读 0xA1** |
| 写周期 | 约 5ms（写完后需等待，期间不响应） |
| 擦写寿命 | 100 万次 |
| 页写限制 | **一页最多 8 字节**，跨页会回卷覆盖（重要坑） |

**读写方式**

| 操作 | 流程 |
|---|---|
| 字节写 | START → 0xA0 → ACK → 字地址 → ACK → 数据 → ACK → STOP |
| 页写 | 同上，但连续写多个字节（不超过页边界） |
| 当前地址读 | START → 0xA1 → ACK → 读数据 → NACK → STOP |
| 随机读 | START → 0xA0 → ACK → 字地址 → ACK → **重起始** → 0xA1 → 读数据 → NACK → STOP |
| 连续读 | 随机读后继续给 ACK，可连续读直到给 NACK |

### 3. OLED 显示屏（SSD1306）

| 参数 | 值 |
|---|---|
| 尺寸 | 0.96 英寸 |
| 分辨率 | **128 × 64** |
| 接口 | IIC（4 针）或 SPI（7 针） |
| IIC 地址 | **0x78（写）/ 0x79（读）** |
| 显存 | 128 × 64 / 8 = **1024 字节** |

**显存组织方式**

```
分 8 页（Page0 ~ Page7），每页高 8 像素、宽 128 列
每列 1 字节 = 8 个竖直像素（bit0 在上，bit7 在下）

    列0  列1  ...  列127
页0  ■    ■          ← 8 行像素
页1  ■    ■
...
```

**控制字节**
- `0x00`：后续是**命令**
- `0x40`：后续是**数据**
- 一次可发多个字节

**常见问题**
- 显示乱码：字库取模方式不对（要选"列行式"或"列行列行"）
- 只显示一半：初始化命令不对或分辨率设置错
- 完全不亮：IIC 地址错（有的模块是 0x7A）、上拉缺失

### 4. 常用 OLED 初始化命令

| 命令 | 作用 |
|---|---|
| `0xAE` / `0xAF` | 关闭 / 打开显示 |
| `0x20` + `0x00` | 设置寻址模式为水平/页寻址 |
| `0x81` + `0x7F` | 设置对比度 |
| `0xA1` | 段重映射（左右方向） |
| `0xC8` | 扫描方向（上下方向） |
| `0xB0~0xB7` | 设置页地址（0~7） |
| `0x00~0x0F` / `0x10~0x1F` | 设置列地址低/高 4 位 |

---

## 二、学习目标

- [ ] 能手写完整的软件 IIC 驱动（不抄代码，凭时序图写出来）
- [ ] 能用 AT24C02 实现字节读写、页写、连续读
- [ ] 能说清"页跨越"问题及规避方法
- [ ] 能驱动 OLED 显示字符、数字、汉字
- [ ] 理解 OLED 显存与页的关系，能实现局部刷新
- [ ] 能封装一个通用 IIC 驱动层，同一套代码支持多个从机

---

## 三、学习方法

1. **手写 IIC（最重要）**：先看着时序图自己写一遍，写不出来再看参考代码。这一关过了，IIC 就永久掌握了。
2. **用 IIC 扫描器验证**：先写一个扫描 `0x00~0x7F` 的小程序，确认能扫到 AT24C02（0xA0）和 OLED（0x78）。
3. **逻辑分析仪抓波形**：用逻辑分析仪抓 IIC 时序，和时序图逐段对照，是最高效的调试方式。
4. **故意跨页写**：往 AT24C02 的地址 7 开始写 8 字节，观察数据回卷覆盖，理解页写限制。
5. **进阶调试**：AT24C02 写完立刻读，发现读到旧值——理解 5ms 写周期需要延时。

---

## 四、标准示例

### 1. 软件 IIC 核心实现

```c
/* ---------------- iic.h ---------------- */
#define IIC_SCL_PORT   GPIOB
#define IIC_SCL_PIN    GPIO_PIN_6
#define IIC_SDA_PORT   GPIOB
#define IIC_SDA_PIN    GPIO_PIN_7

/* 开漏输出：既能输出也能读回 */
#define IIC_SCL_H()    HAL_GPIO_WritePin(IIC_SCL_PORT, IIC_SCL_PIN, GPIO_PIN_SET)
#define IIC_SCL_L()    HAL_GPIO_WritePin(IIC_SCL_PORT, IIC_SCL_PIN, GPIO_PIN_RESET)
#define IIC_SDA_H()    HAL_GPIO_WritePin(IIC_SDA_PORT, IIC_SDA_PIN, GPIO_PIN_SET)
#define IIC_SDA_L()    HAL_GPIO_WritePin(IIC_SDA_PORT, IIC_SDA_PIN, GPIO_PIN_RESET)
#define IIC_SDA_READ() HAL_GPIO_ReadPin(IIC_SDA_PORT, IIC_SDA_PIN)

/* ---------------- iic.c ---------------- */
static void IIC_Delay(void)
{
    Delay_us(4);        /* 100kHz 左右，太快会通信失败 */
}

void IIC_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin   = IIC_SCL_PIN | IIC_SDA_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;      /* 开漏 */
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    IIC_SCL_H();
    IIC_SDA_H();                            /* 总线空闲：都为高 */
}

/* 起始信号：SCL 高时 SDA 下降 */
void IIC_Start(void)
{
    IIC_SDA_H();
    IIC_SCL_H();
    IIC_Delay();
    IIC_SDA_L();
    IIC_Delay();
    IIC_SCL_L();
    IIC_Delay();
}

/* 停止信号：SCL 高时 SDA 上升 */
void IIC_Stop(void)
{
    IIC_SDA_L();
    IIC_SCL_H();
    IIC_Delay();
    IIC_SDA_H();
    IIC_Delay();
}

/* 发送一个字节，返回从机应答（0 = ACK 成功） */
uint8_t IIC_SendByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) IIC_SDA_H();
        else             IIC_SDA_L();
        byte <<= 1;

        IIC_Delay();
        IIC_SCL_H();        /* SCL 高电平期间数据必须稳定 */
        IIC_Delay();
        IIC_SCL_L();
        IIC_Delay();
    }
    return IIC_WaitAck();
}

/* 等待应答：第 9 个时钟，从机拉低 SDA 表示 ACK */
uint8_t IIC_WaitAck(void)
{
    uint8_t ack;
    IIC_SDA_H();            /* 释放 SDA，让从机控制 */
    IIC_Delay();
    IIC_SCL_H();
    IIC_Delay();
    ack = IIC_SDA_READ();   /* 0 = ACK */
    IIC_SCL_L();
    IIC_Delay();
    return ack;
}

/* 接收一个字节，ack=1 发送应答，ack=0 发送非应答 */
uint8_t IIC_RecvByte(uint8_t ack)
{
    uint8_t byte = 0;

    IIC_SDA_H();            /* 释放 SDA 为输入 */
    for (uint8_t i = 0; i < 8; i++) {
        IIC_SCL_H();
        IIC_Delay();
        byte <<= 1;
        if (IIC_SDA_READ()) byte |= 1;
        IIC_SCL_L();
        IIC_Delay();
    }

    /* 发送应答位 */
    if (ack) IIC_SDA_L();
    else     IIC_SDA_H();
    IIC_Delay();
    IIC_SCL_H();
    IIC_Delay();
    IIC_SCL_L();
    IIC_Delay();
    IIC_SDA_H();

    return byte;
}
```

### 2. AT24C02 字节写 / 随机读

```c
#define AT24C02_ADDR_W   0xA0
#define AT24C02_ADDR_R   0xA1

/* 写一个字节（写完需等 5ms） */
uint8_t AT24C02_WriteByte(uint8_t addr, uint8_t data)
{
    IIC_Start();
    if (IIC_SendByte(AT24C02_ADDR_W)) { IIC_Stop(); return 0; }
    if (IIC_SendByte(addr))           { IIC_Stop(); return 0; }
    if (IIC_SendByte(data))           { IIC_Stop(); return 0; }
    IIC_Stop();

    HAL_Delay(5);          /* 关键：等待写周期完成，否则下次通信失败 */
    return 1;
}

/* 随机读一个字节 */
uint8_t AT24C02_ReadByte(uint8_t addr)
{
    uint8_t data;

    IIC_Start();
    IIC_SendByte(AT24C02_ADDR_W);
    IIC_SendByte(addr);
    IIC_Start();                            /* 重起始 */
    IIC_SendByte(AT24C02_ADDR_R);
    data = IIC_RecvByte(0);                 /* 最后一个字节发 NACK */
    IIC_Stop();

    return data;
}

/* 页写（注意不能跨页，同一页内最多 8 字节） */
uint8_t AT24C02_WritePage(uint8_t addr, uint8_t *buf, uint8_t len)
{
    if (len > 8) return 0;
    /* 检查是否跨页：addr % 8 + len 不能超过 8 */
    if ((addr % 8) + len > 8) return 0;

    IIC_Start();
    IIC_SendByte(AT24C02_ADDR_W);
    IIC_SendByte(addr);
    for (uint8_t i = 0; i < len; i++) IIC_SendByte(buf[i]);
    IIC_Stop();

    HAL_Delay(5);
    return 1;
}

/* 通用写：自动处理跨页 */
void AT24C02_WriteBuf(uint8_t addr, uint8_t *buf, uint16_t len)
{
    while (len--) {
        AT24C02_WriteByte(addr++, *buf++);
    }
}
```

### 3. OLED 基础驱动

```c
#define OLED_ADDR    0x78

static void OLED_WriteCmd(uint8_t cmd)
{
    IIC_Start();
    IIC_SendByte(OLED_ADDR);
    IIC_SendByte(0x00);        /* 控制字节：命令 */
    IIC_SendByte(cmd);
    IIC_Stop();
}

static void OLED_WriteData(uint8_t data)
{
    IIC_Start();
    IIC_SendByte(OLED_ADDR);
    IIC_SendByte(0x40);        /* 控制字节：数据 */
    IIC_SendByte(data);
    IIC_Stop();
}

/* 批量写数据（推荐，效率高很多） */
static void OLED_WriteDataBuf(uint8_t *buf, uint16_t len)
{
    IIC_Start();
    IIC_SendByte(OLED_ADDR);
    IIC_SendByte(0x40);
    for (uint16_t i = 0; i < len; i++) IIC_SendByte(buf[i]);
    IIC_Stop();
}

void OLED_Init(void)
{
    HAL_Delay(100);
    OLED_WriteCmd(0xAE);   /* 关显示 */
    OLED_WriteCmd(0x20); OLED_WriteCmd(0x00);   /* 页寻址模式 */
    OLED_WriteCmd(0xB0);   /* 页 0 */
    OLED_WriteCmd(0xC8);   /* 上下方向 */
    OLED_WriteCmd(0xA1);   /* 左右方向 */
    OLED_WriteCmd(0x81); OLED_WriteCmd(0x7F);   /* 对比度 */
    OLED_WriteCmd(0xA6);   /* 正常显示 */
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14);   /* 电荷泵开 */
    OLED_WriteCmd(0xAF);   /* 开显示 */
    OLED_Clear();
}

/* 设置坐标：x 0~127，page 0~7 */
static void OLED_SetPos(uint8_t x, uint8_t page)
{
    OLED_WriteCmd(0xB0 + page);
    OLED_WriteCmd(0x00 + (x & 0x0F));          /* 列低 4 位 */
    OLED_WriteCmd(0x10 + ((x >> 4) & 0x0F));   /* 列高 4 位 */
}

/* 全屏清空 */
void OLED_Clear(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        OLED_SetPos(0, page);
        for (uint8_t i = 0; i < 128; i++) OLED_WriteData(0x00);
    }
}

/* 显示一个字符（8x16 字库） */
void OLED_ShowChar(uint8_t x, uint8_t page, char ch)
{
    uint8_t idx = ch - ' ';
    OLED_SetPos(x, page);
    OLED_WriteDataBuf((uint8_t *)&Font8x16[idx * 16], 8);   /* 上半部分 */

    OLED_SetPos(x, page + 1);
    OLED_WriteDataBuf((uint8_t *)&Font8x16[idx * 16 + 8], 8); /* 下半部分 */
}
```

### 4. 完整使用示例

```c
int main(void)
{
    /* ... 时钟、串口等初始化 ... */
    IIC_Init();
    OLED_Init();

    /* EEPROM 读写测试 */
    AT24C02_WriteByte(0x00, 0x5A);
    HAL_Delay(10);
    uint8_t v = AT24C02_ReadByte(0x00);
    printf("EEPROM[0] = 0x%02X\r\n", v);

    /* OLED 显示 */
    OLED_Clear();
    OLED_ShowString(0, 0, "STM32F103C8");
    OLED_ShowString(0, 2, "EEPROM:");
    OLED_ShowNum(64, 2, v, 3);

    while (1) { }
}
```

---

## 五、自我检测

1. 软件 IIC 为什么用开漏输出而不是推挽？
2. 起始信号和停止信号的区别是什么？
3. 为什么接收数据前要把 SDA 拉高？
4. AT24C02 的设备地址怎么算出来的？A2/A1/A0 全接地时是多少？
5. AT24C02 页写为什么不能超过 8 字节？跨页会发生什么？
6. 写完 AT24C02 为什么要延时 5ms？不延时会出现什么现象？
7. OLED 的 1024 字节显存是怎么算出来的？页和列分别代表什么？
8. OLED 的控制字节 0x00 和 0x40 分别表示什么？
9. 随机读为什么要发两次 START（重起始）？
10. 逻辑分析仪抓到的波形中，怎么识别 ACK 位？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| IIC 完全无响应 | 没接上拉电阻；SDA/SCL 接反；从机地址错 |
| AT24C02 读完立刻读是旧值 | 没等 5ms 写周期 |
| AT24C02 写多字节后数据错乱 | 跨页了，超出部分回卷覆盖 |
| OLED 显示乱码 | 取模方式与驱动不匹配（要"列行式"） |
| OLED 只亮一半或花屏 | 初始化命令缺失（尤其电荷泵 0x8D/0x14） |
| OLED 完全不亮 | IIC 地址错（0x78 vs 0x7A）；上拉缺失 |
| IIC 通信偶尔失败 | 延时太短；上拉电阻过大；线太长 |
| 多个 IIC 设备冲突 | 地址相同；或某个设备把 SDA 一直拉低（总线锁死） |
| 总线锁死无法恢复 | 从机在传输中被复位，需发 9 个时钟脉冲解锁 |

---

## 七、拓展练习

1. **IIC 总线扫描器**：扫描 0x00~0x7F，打印所有响应的设备地址（最实用的调试工具）。
2. **总线解锁函数**：实现"检测 SDA 被拉低时发 9 个时钟"的解锁逻辑。
3. **EEPROM 参数存储**：实现 `SaveConfig()` / `LoadConfig()`，保存 LED 闪烁频率等参数，断电不丢。
4. **OLED 显示温湿度**：把第 10 章的 DHT11 数据实时显示到 OLED 上。
5. **OLED 波形显示**：把第 11 章的 ADC 数据画成实时波形。
6. **汉字显示**：用取模软件生成几个汉字的字库，在 OLED 上显示。
7. **软件 IIC 封装成通用层**：把 IIC 底层与设备驱动解耦，同一套底层支持 AT24C02 + OLED + 其他 IIC 设备。

---

## 补充：本章的产出价值

学完这一章，你应该拥有三个可以**直接复用到任何项目**的模块：

| 模块 | 文件 | 复用价值 |
|---|---|---|
| 软件 IIC 驱动 | `iic.c/h` | 换任何 MCU 改 4 个宏即可用 |
| EEPROM 驱动 | `at24c02.c/h` | 参数存储标配 |
| OLED 驱动 | `oled.c/h` | 调试显示标配 |

把它们整理进工程 `Driver/` 目录，这是你的第一份"私有驱动库"。
