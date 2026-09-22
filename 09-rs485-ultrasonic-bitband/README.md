# 09 · RS485 + 超声波模块 + 位段操作

> 对应课程：09-RS485-超声波模块+位段操作
> 前置要求：08 蓝牙 + 调试
> 建议耗时：5-7 天

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)；学习用板 = **中控板（网关）**）
>
> | 用途 | 笔记示例 | **实板用这个** | 说明 |
> |---|---|---|---|
> | RS485 方向 DE/RE | PA1 | **PA1** | 空闲，且引到板上排针，可用 |
> | RS485 串口 | USART1 | 若同时要 printf，建议 RS485 走 **USART2（PA2/PA3）**，DE 仍用 PA1 | |
> | 超声波 Trig | **PB0** | **冲突**：PB0 = LORA_BUSY | |
> | 超声波 Echo | **PB1** | **冲突**：PB1 = LORA_RESET | |
> | | | **改用 `PA11 = Trig` / `PA12 = Echo`** | 两脚都空闲 |
> | 位带 LED | `PAout(8)` | **PA8 = LED1** | 可用 |
> | 位带按键 | `PAin(0)` | 实板按键是 **PB12 → `PBin(12)`** | |
> | 位带练习 | `PBout(12)` 的别名地址 | **PB12 是按键 SW2**，做位带**输出**练习请换 **PA11** 或 **PB8** | |
> | 蜂鸣器报警（拓展练习 1） | — | 中控板无蜂鸣器 | 用 LED 代替 |
>
> **一个必须做的硬件保护**
> HC-SR04 的 **Echo 输出是 5V 电平**，直接接 3.3V 的 STM32 引脚长期会损伤 IO。
> 用一个 **1kΩ + 2kΩ 分压**（Echo → 1kΩ → 引脚 → 2kΩ → GND），把 5V 降到约 3.3V。

---

## 一、学习内容

### 1. RS485 通信

**电气特性**
- **差分信号**：A、B 两根线，靠两者电压差表示逻辑（差 > +200mV 为 1，< -200mV 为 0）
- 传输距离可达 **1200m**，抗共模干扰能力远强于 TTL/RS232
- **半双工**：同一时刻只能收或发，不能同时

**MAX485 芯片引脚**

| 引脚 | 作用 |
|---|---|
| `RO` | 接收输出 → 接 MCU 的 RX |
| `DI` | 发送输入 ← 接 MCU 的 TX |
| `DE` | 发送使能（高有效） |
| `RE` | 接收使能（**低有效**） |
| `A` / `B` | 差分总线 |

**方向控制**
- `DE` 和 `RE` 通常**短接在一起**由一个 GPIO 控制
- 控制脚 = **1 → 发送**，控制脚 = **0 → 接收**
- **顺序很重要**：置为发送 → 延时 → 发数据 → 等 `TC`（发送完成）→ **延时** → 置回接收
- 如果过早切回接收，最后一个字节会发不出去

**终端电阻**：长距离、高速率时在总线两端各接 120Ω 匹配电阻。

### 2. 超声波模块（HC-SR04）

| 引脚 | 作用 |
|---|---|
| VCC | 5V 供电（**注意：多数模块要求 5V**） |
| Trig | 触发输入，给 **至少 10μs 高电平** |
| Echo | 回响输出，高电平持续时间 = 往返时间 |
| GND | 地 |

**测距原理**

```
距离(cm) = 高电平时间(us) / 58
距离(cm) = 高电平时间(us) × 340(m/s) / 2 / 10000
```

**测量流程**
1. Trig 拉低 2μs → 拉高 10μs → 拉低
2. 等待 Echo 变高
3. 计时 Echo 高电平持续时间
4. 换算距离

**Echo 电平问题**：HC-SR04 的 Echo 输出是 **5V**，STM32 引脚不耐 5V（除 FT 引脚）。建议：
- 用分压电阻（如 1k + 2k）
- 或选 3.3V 供电的模块
- 或加电平转换

**三种测量方案**

| 方案 | 精度 | 难度 | 说明 |
|---|---|---|---|
| 轮询 + 微秒延时 | 低 | 低 | 简单但不准 |
| 轮询 + SysTick/TIM 计数 | 中 | 中 | 推荐入门 |
| 定时器输入捕获 | 高 | 高 | 硬件测量，最准 |

### 3. 位段操作（Bit-Band）

**原理**：Cortex-M3 把"位带区"的每一个 bit 映射到"别名区"的一个 32 位字，对该字读写就等于对该 bit 读写，且是**原子操作**。

| 区域 | 位带区地址 | 别名区地址 |
|---|---|---|
| 外设 | `0x4000_0000` ~ `0x400F_FFFF` | `0x4200_0000` ~ `0x43FF_FFFF` |
| SRAM | `0x2000_0000` ~ `0x200F_FFFF` | `0x2200_0000` ~ `0x23FF_FFFF` |

**地址换算公式**

```
别名地址 = 别名区基址 + (位带地址 - 位带区基址) × 32 + 位序号 × 4
```

**通用宏**

```c
#define BITBAND(addr, bitnum)   ((addr & 0xF0000000U) + 0x02000000U \
                                 + ((addr & 0x000FFFFFU) << 5) \
                                 + ((bitnum) << 2))

#define MEM_ADDR(addr)          (*(volatile uint32_t *)(addr))
#define BIT_ADDR(addr, bitnum)  MEM_ADDR(BITBAND(addr, bitnum))
```

**典型应用（经典的正点原子风格）**

```c
/* 输出 */
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr, n)
#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr, n)
/* 输入 */
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr, n)
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr, n)

PAout(8) = 1;      /* 等价于 GPIOA->BSRR = GPIO_PIN_8，且是原子操作 */
if (PAin(0)) { }   /* 等价于读 GPIOA->IDR 的 bit0 */
```

**STM32F1 的位带限制**：只有 **SRAM 和外设区**支持位带，**Flash 区不支持**。

---

## 二、学习目标

- [ ] 能独立搭建 RS485 通信，正确处理 DE/RE 方向切换时序
- [ ] 能说清 RS485 与 TTL/RS232 的区别及适用场景
- [ ] 能用 HC-SR04 实现测距，误差控制在 ±1cm 内
- [ ] 能解释超声波"除以 58"是怎么来的
- [ ] 能手写位带宏，并用位带方式操作 GPIO
- [ ] 能说出位带操作相比 BSRR 的优势（原子性、可读性）

---

## 三、学习方法

1. **先算再写**：手算 `58` 这个系数的来源（`2 / 340m/s → 1cm 往返约 58μs`），理解公式比记住它重要。
2. **示波器看时序**：用示波器同时看 Trig 和 Echo，直观理解测量过程。
3. **RS485 方向切换实验**：故意把切换延时去掉，观察最后一个字节丢失——理解时序的重要性。
4. **位带地址手算**：手算 `PAout(8)` 的别名地址，与调试器里实际值对照。
5. **对比三种测距方案**：同一目标距离分别用两种方案测量，对比误差，理解输入捕获的价值。

---

## 四、标准示例

### 1. RS485 发送（半双工方向控制）

```c
#define RS485_DE_PORT   GPIOA
#define RS485_DE_PIN    GPIO_PIN_1

#define RS485_TX_MODE()  HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET)
#define RS485_RX_MODE()  HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET)

static void RS485_Send(uint8_t *buf, uint16_t len)
{
    RS485_TX_MODE();                     /* 1. 切到发送 */
    HAL_Delay(1);                        /* 2. 等收发器稳定（视器件而定） */

    HAL_UART_Transmit(&huart1, buf, len, 1000);

    /* 3. 等到最后一个字节真正发完（TC），不能只等 TXE */
    while ((USART1->SR & USART_SR_TC) == 0);

    HAL_Delay(1);                        /* 4. 等总线释放 */
    RS485_RX_MODE();                     /* 5. 切回接收 */
}
```

> **核心坑**：如果只用 `TXE` 就切回接收，最后一个字节会丢失。必须等 `TC`。

### 2. 超声波测距（轮询 + 定时计数）

```c
#define TRIG_PORT  GPIOB
#define TRIG_PIN   GPIO_PIN_0
#define ECHO_PORT  GPIOB
#define ECHO_PIN   GPIO_PIN_1

static void Ultrasonic_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = TRIG_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TRIG_PORT, &gpio);

    gpio.Pin = ECHO_PIN; gpio.Mode = GPIO_MODE_INPUT; gpio.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(ECHO_PORT, &gpio);

    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
}

/* 返回距离(cm)，0 表示超时 */
static float Ultrasonic_GetDistance(void)
{
    uint32_t timeout;

    /* 1. Trig 至少 10us 高电平 */
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);
    Delay_us(12);
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

    /* 2. 等 Echo 变高 */
    timeout = 0;
    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_RESET) {
        if (++timeout > 1000000) return 0;      /* 超时 */
    }

    /* 3. 计时高电平持续时间 */
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    __HAL_TIM_ENABLE(&htim2);                    /* 1us 计数 */

    timeout = 0;
    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_SET) {
        if (++timeout > 1000000) { __HAL_TIM_DISABLE(&htim2); return 0; }
    }

    uint32_t us = __HAL_TIM_GET_COUNTER(&htim2);
    __HAL_TIM_DISABLE(&htim2);

    return (float)us / 58.0f;                    /* 换算成厘米 */
}
```

### 3. 位带宏定义（可直接放进工程）

```c
/* bit_band.h */
#ifndef __BIT_BAND_H
#define __BIT_BAND_H

#include "stm32f1xx.h"

#define BITBAND(addr, bitnum)   ((addr & 0xF0000000U) + 0x02000000U \
                                 + ((addr & 0x000FFFFFU) << 5) \
                                 + ((bitnum) << 2))
#define MEM_ADDR(addr)          (*(volatile uint32_t *)(addr))
#define BIT_ADDR(addr, bitnum)  MEM_ADDR(BITBAND(addr, bitnum))

/* GPIO 输出/输入寄存器地址 */
#define GPIOA_ODR_Addr  (GPIOA_BASE + 0x0C)
#define GPIOB_ODR_Addr  (GPIOB_BASE + 0x0C)
#define GPIOC_ODR_Addr  (GPIOC_BASE + 0x0C)
#define GPIOA_IDR_Addr  (GPIOA_BASE + 0x08)
#define GPIOB_IDR_Addr  (GPIOB_BASE + 0x08)
#define GPIOC_IDR_Addr  (GPIOC_BASE + 0x08)

/* 输出：PAout(8) = 1; */
#define PAout(n)  BIT_ADDR(GPIOA_ODR_Addr, n)
#define PBout(n)  BIT_ADDR(GPIOB_ODR_Addr, n)
#define PCout(n)  BIT_ADDR(GPIOC_ODR_Addr, n)

/* 输入：if (PAin(0)) */
#define PAin(n)   BIT_ADDR(GPIOA_IDR_Addr, n)
#define PBin(n)   BIT_ADDR(GPIOB_IDR_Addr, n)
#define PCin(n)   BIT_ADDR(GPIOC_IDR_Addr, n)

#endif
```

### 4. 位带使用示例

```c
/* 点亮 PA8（低电平点亮） */
PAout(8) = 0;

/* 翻转 */
PAout(8) = !PAout(8);

/* 读按键 */
if (PAin(0) == 0) { /* 按下 */ }
```

### 5. 位带地址手算验证

```
求 PAout(8) 的别名地址：
  GPIOA_ODR_Addr = 0x40010800 + 0x0C = 0x4001080C
  别名地址 = 0x42000000 + (0x4001080C - 0x40000000) × 32 + 8 × 4
           = 0x42000000 + 0x1080C × 32 + 32
           = 0x42000000 + 0x210180 + 0x20
           = 0x422101A0
在调试器的 Memory 窗口输入 0x422101A0，即可观察该位。
```

---

## 五、自我检测

1. RS485 的 DE 和 RE 引脚为什么常常短接？控制逻辑分别是什么有效？
2. RS485 发送后为什么要等 `TC` 而不是 `TXE` 再切回接收？
3. 120Ω 终端电阻的作用是什么？什么时候需要加？
4. 超声波的"除以 58"是怎么推导出来的？
5. HC-SR04 的 Echo 为什么不能直接接 STM32 引脚？怎么解决？
6. 写出 `PBout(12)` 的别名地址计算过程。
7. 位带操作相比 `GPIOA->ODR ^=` 有什么优势？
8. STM32F1 的位带区支持 Flash 吗？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| RS485 收不到数据 | DE/RE 方向没切回接收；A/B 接反 |
| RS485 最后一个字节丢失 | 只等 `TXE` 没等 `TC` 就切方向 |
| RS485 通信距离短就出错 | 没加 120Ω 终端电阻；A/B 没双绞 |
| 超声波始终返回 0 | Trig 高电平时间不足 10μs；模块供电不是 5V |
| 超声波读数乱跳 | 没做多次测量取中值；Echo 干扰 |
| Echo 引脚损坏 | 5V 电平直接接了非 FT 引脚 |
| 位带操作无效果 | 地址算错；操作的是 Flash 区（不支持位带） |
| 位带宏编译报错 | 忘记包含 `stm32f1xx.h`，`GPIOA_BASE` 未定义 |

---

## 七、拓展练习

1. **超声波避障**：距离小于 20cm 时蜂鸣器报警、LED 闪烁。
2. **超声波 + 定时器输入捕获**：用输入捕获重写测距，对比精度与 CPU 占用。
3. **RS485 多机通信**：两块板子通过 RS485 通信，一块主一块从，实现远程控制 LED。
4. **位带驱动库**：用位带宏封装一套 `LED_ON()` / `LED_OFF()` / `LED_TOGGLE()` 接口，与 HAL 版对比速度。
5. **测距滤波**：连续测 5 次，去掉最大最小值后取平均，观察稳定性提升。
6. **Modbus 协议初探**：在 RS485 上实现最简单的 Modbus RTU 读寄存器功能（工业标准，很值得提前接触）。
