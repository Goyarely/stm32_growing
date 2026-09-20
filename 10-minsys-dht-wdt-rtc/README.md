# 10 · 最小系统 + 温湿度 + 看门狗 + RTC

> 对应课程：10-最小系统+温湿度+看门狗+RTC
> 前置要求：09 RS485 + 超声波 + 位带
> 建议耗时：5-7 天

---

## 一、学习内容

### 1. STM32 最小系统

一块 MCU 能跑起来，最小必须包含以下五部分：

| 部分 | 内容 | 说明 |
|---|---|---|
| 电源 | 3.3V（VDD）+ 多个 100nF 去耦电容 | 每个 VDD 引脚旁一个 |
| 晶振 | 8MHz HSE（+ 两个 20pF）、32.768kHz LSE（+ 两个 10pF） | LSE 供 RTC |
| 复位 | 10kΩ 上拉 + 100nF 到地 + 复位按键 | NRST 低电平复位 |
| 启动配置 | BOOT0 下拉 10kΩ、BOOT1 灵活 | 正常从 Flash 启动 |
| 下载 | SWD（SWCLK/SWDIO ）或串口 | JLink ST-Link |

**其他注意点**
- VBAT：接 3V 纽扣电池（或接 VDD），给 RTC 和备份寄存器供电
- VDDA / VSSA：模拟电源，需加 LC 滤波，`VDDA` 与 `VDD` 之间常接磁珠
- NRST 引脚内部已有上拉，但外部再加一个更稳

### 2. DHT11 温湿度传感器

| 参数 | 值 |
|---|---|
| 接口 | **单总线**（一根数据线） |
| 供电 | 3.3V ~ 5.5V |
| 测量范围 | 湿度 20~90%RH，温度 0~50°C |
| 精度 | ±5%RH，±2°C |
| 采样周期 | **≥ 1 秒** |
| 数据 | 40 位：湿度整数+小数、温度整数+小数、校验和 |

**通信时序**

```
主机拉低 18ms → 拉高 20-40us → 释放（等 DHT 响应）
DHT 响应：拉低 80us → 拉高 80us
之后：每位数据以 50us 低电平开始
      高电平 26-28us → 数据 0
      高电平 70us    → 数据 1
```

**关键点**
- **时序要求微秒级**，必须用 `Delay_us` 或 `SysTick` 计数
- **必须关中断**读取，否则被中断打断会导致校验失败（或读取前关、读完开）
- 数据线要**开漏输出 + 外部上拉 4.7kΩ**（或输入输出切换）
- 校验和 = 湿度整数 + 湿度小数 + 温度整数 + 温度小数（取低 8 位）
- 两次读取间隔至少 1 秒，否则数据不准

### 3. 独立看门狗（IWDG）

**原理**：内部 40kHz LSI 时钟驱动 12 位递减计数器，减到 0 就复位 MCU。程序必须定期"喂狗"。

| 寄存器 | 作用 |
|---|---|
| `KR` | 键寄存器：`0x5555` 允许写 PR/RLR、`0xAAAA` 喂狗、`0xCCCC` 启动 |
| `PR` | 预分频：4/8/16/32/64/128/256 |
| `RLR` | 重装值（12 位，最大 4095） |
| `SR` | 状态位 |

**超时时间公式**

```
Tout = (4 × 2^PR) × RLR / 40000     (秒)
```

例：PR = 4（分频 64）、RLR = 625 → `64 × 625 / 40000 = 1.0` 秒

**注意**
- IWDG 一旦启动**无法关闭**，只能复位
- 调试时在断点停住太久会被狗复位——调试期可先不启动
- 硬件看门狗（外部芯片）比 IWDG 更可靠，产品常用

### 4. 窗口看门狗（WWDG）
- 挂在 APB1，有"窗口"概念：喂狗太早或太晚都会复位
- 用于检测"程序跑得太快"的异常
- 有早期唤醒中断（EWI），可在复位前保存数据

### 5. RTC 实时时钟
- 时钟源：**LSE 32.768kHz**（推荐，走时准）/ LSI（不准）/ HSE 分频
- 32 位可编程计数器，`RTC_CNT`（秒计数）、`RTC_ALR`（闹钟）
- **BKP 备份寄存器**（10 个 16 位），由 VBAT 供电，主电源掉电不丢
- 常用技巧：在 BKP 写一个魔数，判断是否首次上电，避免每次复位都重新配置 RTC
- 需要 `__HAL_RCC_PWR_CLK_ENABLE()` + `HAL_PWR_EnableBkUpAccess()`
- RTC 闹钟可产生中断（`RTC_Alarm_IRQn`）或唤醒待机模式

---

## 二、学习目标

- [ ] 能画出 STM32F103C8T6 最小系统原理图
- [ ] 能独立驱动 DHT11，正确读取温湿度并通过校验
- [ ] 理解单总线协议的时序要求，能解释为什么必须关中断
- [ ] 能配置 IWDG 并计算超时时间，实现"喂狗"机制
- [ ] 能说清 IWDG 与 WWDG 的区别与适用场景
- [ ] 能配置 RTC，实现掉电不丢时间的电子钟

---

## 三、学习方法

1. **对比时序图**：把 DHT11 手册的时序图和示波器实测波形并排看，理解每个延时参数。
2. **故意不关中断**：读 DHT11 时不关中断，观察校验失败率，理解时序敏感性。
3. **看门狗实验**：设置 1 秒超时，主循环里 `HAL_Delay(1500)`，观察复位；再读 `RCC_CSR` 确认复位来源。
4. **RTC 掉电实验**：配置好时间后拔掉主电源（保持 VBAT 电池），重新上电看时间是否继续走。
5. **BKP 魔数实验**：不加魔数判断，观察每次复位时间都被重置的 bug。

---

## 四、标准示例

### 1. DHT11 读取（精简版）

```c
#define DHT_PORT   GPIOB
#define DHT_PIN    GPIO_PIN_5

/* 切换到输出模式 */
static void DHT_SetOutput(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = DHT_PIN; g.Mode = GPIO_MODE_OUTPUT_OD;   /* 开漏 */
    g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT_PORT, &g);
}

/* 切换到输入模式 */
static void DHT_SetInput(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = DHT_PIN; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT_PORT, &g);
}

/* 等引脚变为指定电平，带超时 */
static uint8_t DHT_WaitLevel(GPIO_PinState level, uint32_t timeout_us)
{
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) != level) {
        if (timeout_us-- == 0) return 0;
        Delay_us(1);
    }
    return 1;
}

/* 读一次温湿度，成功返回 1 */
uint8_t DHT11_Read(uint8_t *temp, uint8_t *humi)
{
    uint8_t buf[5] = {0};

    __disable_irq();                    /* 关键：关中断保证时序 */

    /* 1. 起始信号：拉低 18ms 再拉高 30us */
    DHT_SetOutput();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
    HAL_Delay(18);
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    Delay_us(30);
    DHT_SetInput();

    /* 2. 等待 DHT 响应：低 80us → 高 80us */
    if (!DHT_WaitLevel(GPIO_PIN_RESET, 200)) { __enable_irq(); return 0; }
    if (!DHT_WaitLevel(GPIO_PIN_SET,   200)) { __enable_irq(); return 0; }
    if (!DHT_WaitLevel(GPIO_PIN_RESET, 200)) { __enable_irq(); return 0; }

    /* 3. 读 40 位数据 */
    for (uint8_t i = 0; i < 5; i++) {
        for (uint8_t j = 0; j < 8; j++) {
            if (!DHT_WaitLevel(GPIO_PIN_SET, 200)) { __enable_irq(); return 0; }
            Delay_us(40);               /* 40us 后仍为高 → 数据 1 */
            buf[i] <<= 1;
            if (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET) {
                buf[i] |= 1;
                if (!DHT_WaitLevel(GPIO_PIN_RESET, 200)) { __enable_irq(); return 0; }
            }
        }
    }

    __enable_irq();

    /* 4. 校验 */
    if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) return 0;

    *humi = buf[0];
    *temp = buf[2];
    return 1;
}
```

### 2. 独立看门狗配置（1 秒超时）

```c
static IWDG_HandleTypeDef hiwdg;

static void IWDG_Init(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;   /* 40kHz / 64 = 625Hz */
    hiwdg.Init.Reload    = 625;                 /* 625 / 625Hz = 1s */
    HAL_IWDG_Init(&hiwdg);                      /* 调用后立即启动，无法关闭 */
}

/* 主循环里定期喂狗 */
while (1) {
    do_work();
    HAL_IWDG_Refresh(&hiwdg);                   /* 喂狗 */
}
```

**寄存器版**

```c
IWDG->KR  = 0x5555;             /* 允许写 PR/RLR */
IWDG->PR  = 4;                  /* 分频 64 */
IWDG->RLR = 625;
IWDG->KR  = 0xAAAA;             /* 首次喂狗加载 RLR */
IWDG->KR  = 0xCCCC;             /* 启动看门狗 */
```

### 3. 读复位来源（调试利器）

```c
/* 在 main 里尽早调用 */
static void PrintResetCause(void)
{
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))  printf("Reset: IWDG\r\n");
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))  printf("Reset: WWDG\r\n");
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))   printf("Reset: Software\r\n");
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))   printf("Reset: PowerOn\r\n");
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))   printf("Reset: NRST Pin\r\n");

    __HAL_RCC_CLEAR_RESET_FLAGS();              /* 清标志 */
}
```

### 4. RTC 初始化（含 BKP 魔数判断）

```c
#define RTC_BKP_MAGIC  0xA5A5

static RTC_HandleTypeDef hrtc;

void RTC_Init_Once(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* 判断是否已配置过 RTC，避免每次复位重置时间 */
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != RTC_BKP_MAGIC) {
        hrtc.Instance            = RTC;
        hrtc.Init.AsynchPrediv   = 32767;      /* LSE 32768 / 32768 = 1Hz */
        hrtc.Init.SynchPrediv    = 0;
        hrtc.Init.OutPut         = RTC_OUTPUTSOURCE_NONE;
        HAL_RTC_Init(&hrtc);

        RTC_TimeTypeDef sTime = {0};
        sTime.Hours = 12; sTime.Minutes = 0; sTime.Seconds = 0;
        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

        RTC_DateTypeDef sDate = {0};
        sDate.Year = 26; sDate.Month = RTC_MONTH_SEPTEMBER; sDate.Date = 17;
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_BKP_MAGIC);   /* 写魔数 */
    }
}

/* 读取并打印 */
void RTC_Show(void)
{
    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);   /* 必须先读 Time 再读 Date */
    printf("20%02d-%02d-%02d %02d:%02d:%02d\r\n",
           d.Year, d.Month, d.Date, t.Hours, t.Minutes, t.Seconds);
}
```

> **注意**：`HAL_RTC_GetDate` 必须紧跟在 `HAL_RTC_GetTime` 之后调用，否则日期不更新（HAL 的机制）。

---

## 五、自我检测

1. 最小系统包含哪五部分？各起什么作用？
2. VBAT 引脚有什么特殊之处？不接电池会怎样？
3. DHT11 的 40 位数据怎么排列？校验和怎么算？
4. 读 DHT11 为什么要关中断？
5. 数据线为什么用开漏 + 外部上拉？
6. IWDG 的超时时间怎么算？给 PR=3、RLR=1000，算出超时时间。
7. IWDG 启动后能关闭吗？为什么？
8. IWDG 和 WWDG 的核心区别是什么？
9. RTC 为什么用 LSE 而不用 HSI？
10. BKP 魔数判断解决了什么问题？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| DHT11 校验总是失败 | 没关中断；`Delay_us` 不准；两次读取间隔小于 1 秒 |
| DHT11 完全无响应 | 数据线没有上拉电阻；供电不足；引脚模式切换没做 |
| 看门狗误复位 | 主循环某处耗时超过超时时间；调试时断点停太久 |
| IWDG 配置后立即复位 | `RLR` 太小，或先启动了再配置（顺序错） |
| 每次复位时间都重置 | 没有用 BKP 魔数判断 |
| RTC 走时不准 | 用了 LSI 而不是 LSE；LSE 晶振匹配电容不对 |
| RTC 日期读出来不更新 | 没按"先 GetTime 再 GetDate"的顺序调用 |
| 上电 RTC 不工作 | 忘记 `HAL_PWR_EnableBkUpAccess()` 或没使能 PWR/BKP 时钟 |

---

## 七、拓展练习

1. **温湿度记录仪**：DHT11 每 10 秒采集一次，加上 RTC 时间戳，通过串口打印。
2. **数据存 Flash**：把温湿度数据写入 Flash（预告第 12 章），实现掉电保存。
3. **看门狗保护**：在串口接收任务里故意制造死循环，验证看门狗能救活系统。
4. **RTC 闹钟**：用 RTC 闹钟中断实现每天定时触发任务，配合待机模式做低功耗。
5. **OLED 显示时间**：把 RTC 时间显示到 OLED（配合第 13 章）。
6. **画原理图**：用立创 EDA 或 KiCad 画出完整最小系统原理图，包含所有去耦电容。

---

## 补充：本章为什么重要

这一章是从"做实验"走向"做产品"的分水岭：

- **最小系统** = 能独立做板子的前提
- **看门狗** = 产品可靠性的底线
- **RTC + BKP** = 时间相关的功能基础
- **DHT11** = 第一次接触严格时序协议，为后面 IIC/SPI 打基础
