# 04 · 定时器 + SysTick

> 对应课程：04-定时器+SysTick
> 前置要求：03 按键 + 中断
> 建议耗时：5-7 天

---

## 一、学习内容

### 1. STM32F103 定时器分类

| 类型 | 型号 | 位宽 | 特点 |
|---|---|---|---|
| 高级定时器 | TIM1、TIM8 | 16 位 | 带死区、刹车、互补输出，用于电机控制 |
| 通用定时器 | TIM2~TIM5 | 16 位 | 4 个独立通道，输入捕获/输出比较/PWM |
| 基本定时器 | TIM6、TIM7 | 16 位 | 只有定时功能，常用于 DAC 触发 |

### 2. 时基单元（核心）

```
时钟源 CK_PSC → [预分频器 PSC] → CK_CNT → [计数器 CNT] → 溢出 → 更新事件
                                              ↑
                                       [自动重装 ARR]
```

| 寄存器 | 作用 |
|---|---|
| `PSC` | 预分频，实际分频 = PSC + 1（**写 0 表示不分频**） |
| `CNT` | 当前计数值 |
| `ARR` | 自动重装值，实际周期 = ARR + 1 |
| `RCR` | 重复计数（高级定时器用） |
| `EGR` | 事件产生，写 `UG` 位可软件产生更新事件（用于立即加载 PSC） |

**定时时间公式**

```
Tout = (ARR + 1) × (PSC + 1) / Tclk
```

例：72MHz 下要 1ms 溢出 → `(PSC+1) × (ARR+1) = 72000`
取 `PSC = 7199`、`ARR = 9`（或 `PSC = 71`、`ARR = 999`）。

**计数模式**：向上 / 向下 / 中央对齐（向上向下交替）。

### 3. 时钟来源
- 内部时钟 CK_INT：APB1 定时器时钟（72MHz 时因 APB1 分频为 2，定时器时钟被 ×2 为 72MHz）
- 外部时钟模式、内部触发（ITR）

### 4. 定时器中断
- 更新中断 `UIE`：计数器溢出
- 捕获/比较中断 `CCxIE`
- `SR` 寄存器标志位：`UIF`、`CCxIF`，**必须清除**
- 中断函数：`TIM2_IRQHandler` 等

### 5. SysTick（内核滴答定时器）
- Cortex-M3 **内核**外设，不属于 STM32 外设（不在 RCC 时钟树里）
- 24 位**递减**计数器，寄存器：`CTRL`、`LOAD`、`VAL`、`CALIB`
- `LOAD` 重装值，`VAL` 当前值，计数到 0 时：
  - 置 `COUNTFLAG` 标志
  - 若 `TICKINT=1` 则产生 SysTick 异常
- HAL 库默认配置为 1ms 中断，`HAL_Delay()` 依赖它

### 6. 延时的三种实现

| 方式 | 精度 | 是否阻塞 | 适用 |
|---|---|---|---|
| `HAL_Delay()` | 1ms | 阻塞 | 主循环 |
| 定时器中断计数 | 高 | 非阻塞 | 精准周期任务 |
| SysTick 轮询 `VAL` | 微秒级 | 阻塞 | **ISR 内延时**（不能被打断） |

---

## 二、学习目标

- [ ] 能独立计算 PSC/ARR，实现任意定时周期
- [ ] 能说清 PSC、CNT、ARR 的关系与计数流程
- [ ] 能配置定时器更新中断，并在 ISR 中正确清标志
- [ ] 理解 SysTick 与 TIM 的区别（内核 vs 外设）
- [ ] 能写一个不依赖中断的微秒级延时函数
- [ ] 能用定时器替代 `HAL_Delay` 实现非阻塞任务调度

---

## 三、学习方法

1. **先算后写**：在纸上算出 PSC/ARR，再用示波器或 LED 验证周期是否准确。算错一次比看十遍公式有用。
2. **单步看 CNT**：在定时器中断打断点，打开 `Peripherals → TIM2`，观察 CNT 从 0 涨到 ARR 再归零。
3. **对比 8MHz 与 72MHz**：同一组 PSC/ARR，在两个主频下定时时间差 9 倍——理解公式中 Tclk 的含义。
4. **验证 SysTick 独立于 RCC**：故意关闭所有外设时钟，`HAL_Delay` 依然工作。
5. **非阻塞改造**：把"延时 500ms 翻转 LED"改成"定时器中断里计数到 500 次翻转"，感受 CPU 释放。

---

## 四、标准示例

### 1. TIM2 定时 1ms 中断

```c
/* 72MHz：PSC=7199 → 10kHz；ARR=9 → 1kHz → 1ms */
static void TIM2_Init(void)
{
    TIM_HandleTypeDef htim2 = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();                 /* TIM2 挂在 APB1 */

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 7199;         /* 72000000 / 7200 = 10kHz */
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 9;            /* 10kHz / 10 = 1kHz = 1ms */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim2);

    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    HAL_TIM_Base_Start_IT(&htim2);               /* 使能更新中断 */
}

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);                  /* HAL 内部清标志 + 回调 */
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        /* 1ms 执行一次的业务 */
    }
}
```

### 2. 寄存器版（理解底层）

```c
RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

TIM2->PSC = 7199;
TIM2->ARR = 9;
TIM2->CR1 |= TIM_CR1_ARPE;      /* ARR 预装载 */
TIM2->EGR  = TIM_EGR_UG;        /* 立即把 PSC/ARR 加载进影子寄存器 */
TIM2->SR   = 0;                 /* 清掉 UG 产生的更新标志 */

TIM2->DIER |= TIM_DIER_UIE;     /* 使能更新中断 */
TIM2->CR1  |= TIM_CR1_CEN;      /* 启动计数 */

NVIC_SetPriority(TIM2_IRQn, 1);
NVIC_EnableIRQ(TIM2_IRQn);

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR = ~TIM_SR_UIF;   /* 清标志（写 0 清除） */
        /* 业务代码 */
    }
}
```

### 3. SysTick 微秒延时（可在 ISR 内使用）

```c
/* 不依赖中断，纯轮询 COUNTFLAG，适合中断里调用 */
void Delay_us(uint32_t us)
{
    uint32_t load  = SystemCoreClock / 1000000UL;   /* 1us 的计数值 */
    uint32_t start, cur, elapsed;

    SysTick->LOAD = load - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    /* 注意：不设 TICKINT，避免影响 HAL 的 1ms 时基 */

    start = SysTick->VAL;
    do {
        cur = SysTick->VAL;
        elapsed = (cur <= start) ? (start - cur) : (load - cur + start);
    } while (elapsed < us);

    SysTick->CTRL = 0;
}
```

> 更稳妥的做法是用一个空闲的 TIM 做微秒延时，避免与 HAL 时基冲突。

### 4. 非阻塞定时任务框架

```c
volatile uint32_t g_tick_ms = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) g_tick_ms++;
}

#define TASK_INTERVAL 500
static uint32_t last = 0;

while (1) {
    if (g_tick_ms - last >= TASK_INTERVAL) {   /* 用减法，防止溢出问题 */
        last = g_tick_ms;
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8);
    }
    /* 其他任务可以并行执行，不被 Delay 阻塞 */
}
```

---

## 五、自我检测

1. 写出定时时间的计算公式，并算出 72MHz 下 500ms 的 PSC/ARR（给一组值）。
2. 为什么 `PSC = 0` 表示不分频而不是停止？
3. `EGR` 寄存器的 `UG` 位有什么用？不用会有什么问题？
4. TIM2 挂在哪条总线？为什么 72MHz 系统下它的时钟仍是 72MHz？
5. 定时器 `SR` 的更新标志怎么写清零？和 EXTI 的 PR 一样吗？
6. SysTick 属于内核还是外设？它受 RCC 时钟树控制吗？
7. 为什么 ISR 里不能用 `HAL_Delay()`？该怎么延时？
8. 用"减法比较"`g_tick_ms - last` 而不用 `g_tick_ms == last + 500`，为什么？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| 定时器完全不工作 | 忘记使能 `RCC_APB1ENR` 的 TIMxEN |
| 第一次中断时间明显不对 | `EGR` 的 `UG` 没写，PSC/ARR 要等下一个周期才生效 |
| 中断进一次就卡死 | `SR` 标志没清 |
| 定时时间偏 9 倍 | 时钟源理解错误（APB1 分频 ×2 规则），或主频不是 72MHz |
| `HAL_Delay()` 变慢十倍 | `SystemCoreClockUpdate()` 未调用 |
| 微秒延时不准 | 使用 `HAL_Delay` 想当然，或用 `SysTick->VAL` 时没处理回绕 |
| ISR 里 `HAL_Delay()` 导致死机 | 中断优先级高于 SysTick，`HAL_IncTick` 无法执行 |

---

## 七、拓展练习

1. **精准周期任务**：用定时器中断实现 1s 精确计时，与 `HAL_Delay` 版本对比 1 分钟的累计误差。
2. **软件 PWM**：在 1ms 定时器中断里做 100 级计数，输出可调占空比的波形（为第 05 章做铺垫）。
3. **电子时钟雏形**：定时器 1ms 中断 + 计数，实现"时:分:秒"并在串口打印。
4. **定时器级联**：TIM2 输出触发 TIM3，实现超长定时（1 小时以上）。
5. **多任务调度器**：用 `g_tick_ms` 写一个支持 3 个不同周期任务的极简调度器。
6. **寄存器重写**：把 TIM2 例程用寄存器完全重写，并对比 HAL 版代码体积。
