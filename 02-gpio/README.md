# 02 · GPIO 外设 + LED 库函数 + 寄存器开发

> 对应课程：02-GPIO外设+LED库函数+寄存器开发
> 前置要求：01 STM32 概述 + 环境搭建
> 建议耗时：4-6 天

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)；学习用板 = **中控板（网关）**）
>
> | 用途 | 笔记示例 | **实板用这个** | 说明 |
> |---|---|---|---|
> | LED | PA8 | **PA8 = LED1**（第二个：**PB15 = LED2**） | 低电平点亮，与笔记一致 |
> | 按键 | PA0 / PB12 | **PB12 = SW2 / PB13 = SW3** | 10kΩ 上拉，按下 = 低 |
> | 跑马灯（拓展练习 1） | PB0~PB7 八个 LED | **板上没有 8 个 LED** | 改用"用调试器看 `GPIOB->ODR` 的 bit0~bit7 依次变化"，或只用现有 2 个 LED 交替 |
> | 开漏验证（拓展练习附加） | 空引脚 | 用**排针引出的 PA11**（完全空闲） | 见 `拓展练习参考.md` |

---

## 一、学习内容

### 1. GPIO 的物理结构
- 端口位：每个 GPIO 端口有 16 个引脚（Px0 ~ Px15）
- 内部结构：输入保护二极管 → 上下拉电阻 → 施密特触发器 → 输出驱动器（P-MOS/N-MOS）
- 三类寄存器：配置寄存器 + 数据寄存器 + 复用功能

### 2. 8 种工作模式

| 类别 | 模式 | 说明 | 典型用途 |
|---|---|---|---|
| 输入 | 浮空输入 | 电平由外部决定 | 外部已有上下拉 |
| 输入 | 上拉输入 | 内部上拉，默认高 | 按键（按下接地） |
| 输入 | 下拉输入 | 内部下拉，默认低 | 按键（按下接高） |
| 输入 | 模拟输入 | 关闭数字通道 | **ADC 采集必须用此模式** |
| 输出 | 推挽输出 | 能输出高低电平，驱动强 | LED、蜂鸣器 |
| 输出 | 开漏输出 | 只能拉低，需外部上拉 | I2C、电平转换、共用总线 |
| 输出 | 复用推挽 | 由外设控制引脚 | USART TX、SPI、PWM |
| 输出 | 复用开漏 | 由外设控制引脚 | I2C 硬件外设 |

### 3. 寄存器映射（F1 系列）

| 寄存器 | 作用 | 说明 |
|---|---|---|
| `GPIOx_CRL` | 配置 Px0~Px7 | 每引脚 4 位（2 位 MODE + 2 位 CNF） |
| `GPIOx_CRH` | 配置 Px8~Px15 | 同上 |
| `GPIOx_IDR` | 输入数据 | 只读，读引脚电平 |
| `GPIOx_ODR` | 输出数据 | 可读可写 |
| `GPIOx_BSRR` | 置位/复位 | 低 16 位写 1 置高，高 16 位写 1 置低 |
| `GPIOx_BRR` | 复位 | 低 16 位写 1 置低 |

**配置位（CRL/CRH 每个引脚 4 位）**

| CNF | MODE | 模式 |
|---|---|---|
| 00 | 00 | 输入（模拟/浮空，见下） |
| 01 | 00 | 浮空输入 |
| 10 | 00 | 上拉/下拉输入（由 ODR 决定） |
| 00 | 01/10/11 | 通用推挽输出（10/2/50MHz） |
| 01 | 01/10/11 | 通用开漏输出 |
| 10 | 01/10/11 | 复用推挽输出 |
| 11 | 01/10/11 | 复用开漏输出 |

> 注意：F1 的 GPIO 没有独立的上拉/下拉配置寄存器，**上拉还是下拉由 ODR 位决定**，这是 F1 与 F4 的重要区别。

### 4. HAL 库 GPIO API

| 函数 | 作用 |
|---|---|
| `HAL_GPIO_Init()` | 初始化（模式、上下拉、速度） |
| `HAL_GPIO_DeInit()` | 复位为默认状态 |
| `HAL_GPIO_ReadPin()` | 读引脚电平 |
| `HAL_GPIO_WritePin()` | 写引脚电平 |
| `HAL_GPIO_TogglePin()` | 翻转电平 |
| `HAL_GPIO_LockPin()` | 锁定配置 |

时钟宏：`__HAL_RCC_GPIOA_CLK_ENABLE()` ~ `GPIOE`。

---

## 二、学习目标

- [x] 能说出 8 种模式的区别与适用场景，不查资料
- [x] 能解释 CRL/CRH 中 4 位配置的含义，手算任意引脚的配置值
- [x] 会用 BSRR 实现原子化的位操作
- [x] HAL 版与寄存器版都能写出 LED 闪烁
- [x] 理解开漏输出为什么需要外部上拉电阻

---

## 三、学习方法

1. **打通"库 ↔ 寄存器"对应关系（本课核心）**：在 `HAL_GPIO_Init()` 内部打断点，单步执行到最后，看它写了 `CRL` 还是 `CRH` 的哪几位。把"Mode = OUTPUT_PP + Speed = LOW"翻译成 `0x1` 这个位值。
2. **必须双版本手写**：HAL 版和寄存器版各写一遍 LED，并排对比。
3. **钟表法看时序**：用示波器或逻辑分析仪看翻转波形，验证 500ms 周期是否准确。
4. **破坏性实验**：故意**不开 GPIOA 时钟**就调用 `HAL_GPIO_Init()`，观察 LED 完全不动——把"时钟先使能"刻进肌肉记忆。

---

## 四、标准示例

### 1. HAL 版：PA8 推挽输出点灯

```c
#include "stm32f1xx_hal.h"

#define LED_PORT        GPIOA
#define LED_PIN         GPIO_PIN_8
#define LED_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

static void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    LED_CLK_ENABLE();                     /* 1. 使能时钟（最容易漏） */

    GPIO_InitStruct.Pin   = LED_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;      /* 推挽输出 */
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;      /* 点灯无需高速 */
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);  /* 熄灭 */
}
```

### 2. 寄存器版：完全等价的实现

```c
/* 1. 使能 GPIOA 时钟：RCC->APB2ENR 的 bit2 */
RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

/* 2. 配置 PA8：在 CRH 中占 bit[3:0]（PA8 是 CRH 的第 0 个引脚） */
GPIOA->CRH &= ~(0xFU << 0);        /* 先清零 */
GPIOA->CRH |=  (0x1U << 0);        /* MODE=01(10MHz), CNF=00(通用推挽) */

/* 3. 输出低电平点亮 */
GPIOA->BSRR = GPIO_PIN_8 << 16;    /* 高 16 位写 1 → 复位 */
```

### 3. 三种写引脚方式的对比

```c
/* 方式一：HAL 库（内部有分支判断，稍慢） */
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

/* 方式二：BSRR（推荐，单次写，原子操作，不会被中断打断） */
GPIOA->BSRR = (uint32_t)GPIO_PIN_8 << 16;   /* 置低 */
GPIOA->BSRR = (uint32_t)GPIO_PIN_8;         /* 置高 */

/* 方式三：ODR 异或（仅适合单线程翻转） */
GPIOA->ODR ^= GPIO_PIN_8;
```

> **为什么推荐 BSRR？** 它是原子操作，"读-改-写"由硬件完成，不会出现中断打断导致的竞争问题。而 `ODR ^=` 是软件三步操作，有风险。

### 4. 按键输入（上拉输入 + 轮询）

```c
/* PA0 上拉输入，按键另一端接地 */
RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
GPIOA->CRL &= ~(0xFU << 0);   /* 清空 PA0 配置 */
GPIOA->CRL |=  (0x8U << 0);   /* MODE=00(输入), CNF=10(上拉/下拉) */
GPIOA->ODR |=  GPIO_PIN_0;    /* ODR=1 → 上拉(F1 系列靠 ODR 选择上下拉) */

/* 读取：按下时接地 → 读到 0 */
if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) { /* 按下 */ }
```

---

## 五、自我检测

1. 推挽输出和开漏输出的电路结构差别在哪？为什么 I2C 必须用开漏？
2. PA13 应该在 `CRL` 还是 `CRH` 里配置？占哪几位？
3. 把 PA5 配成"上拉输入"，写出 CRL 的配置值。
4. `BSRR` 和 `BRR` 有什么区别？为什么 `BSRR` 更好？
5. ADC 采集时引脚为什么要配成"模拟输入"？
6. 忘记使能 GPIO 时钟会有什么现象？
7. F1 和 F4 在上下拉配置上有什么不同？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| 配置完引脚完全没反应 | 忘记 `__HAL_RCC_GPIOx_CLK_ENABLE()` |
| LED 亮度极暗 | 用了开漏输出，没有外部上拉 |
| 配了 `GPIO_PULLUP` 但读不到预期电平 | F1 的上拉靠 ODR 实现，HAL 已处理，但用寄存器时要自己写 ODR |
| 按键抖动导致多次触发 | 需要软件消抖（延时 10-20ms 后二次确认） |
| 复用功能引脚不工作 | Mode 没设成 `GPIO_MODE_AF_PP`，而是设成了普通输出 |
| 引脚配置被覆盖 | 同一引脚在两个初始化函数里都配置了 |

---

## 七、拓展练习

1. **跑马灯**：用 PB0~PB7 八个 LED 实现流水灯，分别用 HAL 和寄存器实现。
2. **呼吸灯（软件 PWM）**：用 `HAL_Delay` 精调占空比实现亮度渐变，为第 05 章的硬件 PWM 做铺垫。
3. **按键计数**：按下 PA0 一次，LED 翻转一次；改用 EXTI 重写（为第 03 章铺垫）。
4. **对比实验**：用 `HAL_GPIO_TogglePin` 与 `GPIOA->ODR ^=` 分别翻转，用逻辑分析仪对比波形。
5. **查手册练习**：打开 RM0008 的 GPIO 章节，找到 `GPIOx_BSRR` 的位定义截图，标注每个位的含义。
