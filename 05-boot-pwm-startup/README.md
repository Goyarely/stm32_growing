# 05 · 启动模式 + PWM + 启动文件分析 + 串口下载

> 对应课程：05-启动模式+PWM+启动文件分析+串口下载
> 前置要求：04 定时器 + SysTick
> 建议耗时：5-7 天

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)；学习用板 = **中控板（网关）**）
>
> | 用途 | 笔记示例 | **实板用这个** | 说明 |
> |---|---|---|---|
> | 串口下载 | PA9/PA10（USART1） | **PA9/PA10** | 与笔记一致；**板载 CH340K，插 USB 就能下载** |
> | PWM 呼吸灯 | TIM3_CH1 → **PA6** | **冲突**：PA6 已被板载 LoRa 占用（LORA_MISO） | |
> | | | **改用 `PA8 → TIM1_CH1`** | 直接驱动板上 LED1，**免接线**（推荐） |
> | | | 备选：`PB8 → TIM4_CH3` / `PA0 → TIM2_CH1` | PB8、PA0 都空闲，但需外接 LED 到 3.3V |
> | 双路混色 | PA6 / PA7 | 两脚都被 LoRa 占用 | 改用 **PA8 + PB8**（PB8 需外接 LED） |
>
> **两个坑**
> 1. **PA6/PA7 已被 LoRa 占死**，笔记里的 PWM 示例照抄会没波形。用 PA8。
> 2. **TIM1 是高级定时器**，PWM 输出需要额外使能**主输出（MOE）**。如果 `HAL_TIM_PWM_Start()` 之后完全没有波形，先怀疑这里（通用定时器 TIM2~TIM4 无此问题）。
> 3. 改启动模式（BOOT0）前**先拍照记下跳线帽原位**，否则改错会"程序跑飞"。

---

## 一、学习内容

### 1. 启动模式（BOOT 引脚）

| BOOT1 | BOOT0 | 启动区域 | 说明 |
|---|---|---|---|
| x | 0 | 主 Flash | **正常运行时用这个** |
| 0 | 1 | 系统存储器 | 内置 Bootloader（ST 出厂固化），用于串口下载 |
| 1 | 1 | 内置 SRAM | 调试用，掉电丢失 |

- 上电/复位时采样 BOOT 引脚，决定**中断向量表的映射位置**（`SYSCFG_MEMRMP` 可软件修改）
- 主 Flash 映射到 `0x0000_0000`，CPU 从这里取第一条指令
- 系统存储器 Bootloader 支持 USART1（PA9/PA10）下载，不受用户程序影响

### 2. 启动文件分析（`startup_stm32f103xb.s`）

**中断向量表结构**

```
__Vectors:
    DCD  __initial_sp          ; 第 0 项：栈顶地址（SRAM 最高地址）
    DCD  Reset_Handler         ; 第 1 项：复位异常
    DCD  NMI_Handler
    DCD  HardFault_Handler
    ...
    DCD  EXTI0_IRQHandler      ; 外部中断
    ...
```

**启动流程**

```
上电复位
  → 从 0x00000000 读栈顶 → 从 0x00000004 读 Reset_Handler 地址
  → Reset_Handler: SystemInit() → __main
  → __main（C 库）：搬运 .data 到 SRAM、.bss 清零、初始化堆栈
  → main()
```

**关键段**

| 段 | 存放内容 | 处理方式 |
|---|---|---|
| `.text` | 代码 + 常量 | 直接在 Flash 执行 |
| `.data` | 有初值的全局/静态变量 | 初值存 Flash，启动时搬到 SRAM |
| `.bss` | 无初值的全局/静态变量 | 启动时清零 |
| 堆（Heap） | `malloc` 用 | `_Min_Heap_Size` |
| 栈（Stack） | 局部变量、函数调用 | `_Min_Stack_Size` |

**WEAK 弱定义**：启动文件里所有中断函数都是 `WEAK` 的空实现，你在 C 文件里定义同名函数即可覆盖。

### 3. PWM 输出

**原理**：定时器计数 CNT 与比较寄存器 CCR 比较，在特定模式下翻转输出电平。

| 寄存器 | 作用 |
|---|---|
| `ARR` | 决定 **PWM 频率**（周期） |
| `CCRx` | 决定 **占空比** |
| `CCMRx` | 输出比较模式（PWM 模式 1/2）、预装载使能 |
| `CCER` | 通道使能、输出极性 |

**PWM 模式 1**：`CNT < CCR` 时输出有效电平，`CNT >= CCR` 时输出无效电平。
**频率公式**：`Fpwm = 72MHz / ((ARR+1) × (PSC+1))`
**占空比**：`Duty = CCR / (ARR + 1) × 100%`

**关键配置点**
- 引脚必须配成 `GPIO_MODE_AF_PP`（复用推挽）
- 必须使能 `TIMx->CCER` 的 `CCxE` 位
- 建议使能 `ARPE` 和 `OCxPE` 预装载，避免运行中改参数产生毛刺
- `TIM3_CH1` → PA6、`TIM3_CH2` → PA7、`TIM2_CH1` → PA0（注意与 EXTI0 复用冲突）

### 4. 串口下载（ISP）
- 接法：USB-TTL 的 TX → PA10(RX)、RX → PA9(TX)、GND 共地
- 步骤：BOOT0 置 1 → 复位 → 打开下载软件（FlyMcu / mcuisp / STM32CubeProgrammer）→ 选择 hex → 下载 → BOOT0 置 0 → 复位
- 原理：利用系统存储器里的 Bootloader 通过 USART1 接收固件写入 Flash

---

## 二、学习目标

- [x] 能说清三种启动模式的区别与切换方法
- [x] 能画出中断向量表的排布，解释为什么改 ISR 函数名会导致中断不触发
- [ ] 能解释 `.data` / `.bss` 的作用与启动时的处理 （待做「收尾实验 A」）
- [x] 能独立计算 PWM 频率与占空比，实现呼吸灯
- [ ] 掌握串口下载的完整流程，能在没有 JLink 时救活板子 ← **本板跳过**（ST-Link 已够用；BOOT0=1 → PC=0x1FFFF35E 已实测，原理已通）
- [ ] 理解"栈溢出"会导致什么现象，会看 `.map` 文件 （待做「收尾实验 B」）

---

## 三、学习方法

1. **反汇编看向量表**：在调试器里打开 Disassembly 窗口，跳到 `0x08000000`，看前两个字是不是栈顶地址和 `Reset_Handler`。
2. **删掉启动文件试一次**：编译能过但链接异常或运行不了——理解它为什么必需。
3. **故意改错 ISR 名**：把 `EXTI0_IRQHandler` 拼错，观察中断再也不触发（实际跳到了 WEAK 空函数），这个坑必须踩一次。
4. **单步看 .bss 清零**：在 `__main` 打断点，观察 `0x20000000` 附近内存从随机值变为 0。
5. **PWM 用示波器验证**：改 CCR 时观察波形占空比变化，改 ARR 观察频率变化。

---

## 四、标准示例

### 1. PWM 呼吸灯（TIM3_CH1 → PA6）

```c
static TIM_HandleTypeDef htim3;

static void PWM_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};
    GPIO_InitTypeDef   GPIO_InitStruct = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();      /* TIM3 在 APB1 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();      /* 使用复用功能建议开启 */

    /* PA6 → 复用推挽输出 */
    GPIO_InitStruct.Pin   = GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 定时器时基：72MHz / 72 / 1000 = 1kHz */
    htim3.Instance           = TIM3;
    htim3.Init.Prescaler     = 71;        /* 72MHz / 72 = 1MHz */
    htim3.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim3.Init.Period        = 999;       /* 1MHz / 1000 = 1kHz */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim3);

    /* PWM 通道配置 */
    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 500;                 /* 初始占空比 50% */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

/* 动态调占空比：0~1000 映射到 0~100% */
static void PWM_SetDuty(uint16_t duty)   /* duty: 0 ~ 1000 */
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);
    /* 等效寄存器写法：TIM3->CCR1 = duty; */
}
```

### 2. 呼吸灯效果（主循环渐变）

```c
while (1) {
    for (uint16_t i = 0; i <= 1000; i += 10) { PWM_SetDuty(i); HAL_Delay(5); }
    for (uint16_t i = 1000; i > 0;  i -= 10) { PWM_SetDuty(i); HAL_Delay(5); }
}
```

### 3. 寄存器版 PWM 配置要点

```c
RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
TIM3->PSC = 71;
TIM3->ARR = 999;
TIM3->CCR1 = 500;

/* CH1 配置为 PWM 模式 1 + 预装载 */
TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
TIM3->CCMR1 |= (0x6U << 4) | TIM_CCMR1_OC1PE;   /* OC1M=110 PWM1, OC1PE 预装载 */
TIM3->CCER  |= TIM_CCER_CC1E;                    /* 通道 1 输出使能 */
TIM3->CR1   |= TIM_CR1_ARPE | TIM_CR1_CEN;       /* ARR 预装载 + 启动 */
```

---

## 五、自我检测

1. 三种启动模式分别映射到哪个存储区域？Bootloader 在哪？
2. 中断向量表的前两个字分别是什么？
3. `.data` 段和 `.bss` 段的区别是什么？启动时各做什么处理？
4. 把 `EXTI0_IRQHandler` 拼写错误会发生什么？为什么？
5. 72MHz 下要产生 20kHz 的 PWM，ARR 和 PSC 怎么取？
6. 为什么 PWM 输出引脚必须配成复用推挽而不是普通推挽？
7. 改 `CCR` 时为什么要使能预装载（OCxPE）？
8. 串口下载时 BOOT0 应该是什么状态？下载完必须改回来吗？

---

### 自我检测 · 参考答案

**1. 三种启动模式分别映射到哪个存储区域？Bootloader 在哪？**

| BOOT1 | BOOT0 | `0x00000000` 映射到 | 实际物理地址 |
|---|---|---|---|
| x | **0** | 主 Flash | `0x08000000`（128KB） |
| 0 | **1** | 系统存储器 | **`0x1FFFF000`（2KB）** ← **Bootloader 在这里** |
| 1 | 1 | 内嵌 SRAM | `0x20000000`（20KB） |

- Bootloader 是 **ST 出厂烧在系统存储器里的**，用户擦不掉
- 通过 **USART1（PA9/PA10）** 接收固件，这就是 ISP 串口下载的原理
- **实测证据**：`BOOT0 = 1` 时调试器读到的 PC = **`0x1FFFF35E`**（正在跑 bootloader）

**2. 中断向量表的前两个字分别是什么？**

| 偏移 | 内容 | 地址类型 |
|---|---|---|
| `0x00` | **`__initial_sp`** | **栈顶地址**（**RAM** 里，`0x2000xxxx`） |
| `0x04` | **`Reset_Handler` 的地址** | **Flash** 里，`0x0800xxxx` |

**复位时硬件自动做两件事**：
① 读 `0x00000000` → 装进 **MSP**（主栈指针）
② 读 `0x00000004` → 装进 **PC** → 跳到 `Reset_Handler`

> **为什么第一件事是设栈**：C 函数离不开栈。**没有栈，一行 C 代码都跑不了。**

**3. `.data` 段和 `.bss` 段的区别？启动时各做什么处理？**

| 段 | 内容 | 占 Flash 吗 | 启动时做什么 |
|---|---|---|---|
| **`.data`** | **有初值**的全局/静态变量 | ✅ **占**（初值存在 Flash 里） | **从 Flash 复制到 RAM** |
| **`.bss`** | **无初值**（或初值为 0）的全局/静态变量 | ❌ **不占** | **整块清零** |
| `.rodata` / `.text` | `const` 常量、代码 | ✅ 占 | 不动（常驻 Flash） |

**执行者**：`Reset_Handler` → **`__main`** → `__scatterload`（ARM C 库的分散加载代码）

**为什么这么设计**：`.bss` 的初值全是 0，**没必要在 Flash 里存一堆 0** → 省 Flash 空间。

**4. 把 `EXTI0_IRQHandler` 拼写错误会发生什么？为什么？**

- **编译、链接全部通过**（**不报任何错**）
- **中断触发时不会执行你的代码**，而是跳到启动文件里那个 `[WEAK]` 空实现
- **现象：中断"完全没反应"**

**原因**：向量表那一格填的是**链接器找到的那个符号的地址**。

```
函数名一致   →  你的函数覆盖了弱符号  →  表里填【你的地址】
函数名不一致 →  表里还是弱符号        →  表里填【默认死循环的地址】
```

**实测证据**：

| 表项 | 地址 | 说明 |
|---|---|---|
| 默认死循环（40+ 个 handler 共用，被链接器折叠） | **`0x0800011B`** | 启动文件里那句 `B .` |
| `EXTI15_10_IRQHandler`（你写的） | **`0x08000173`** | 接上了 |
| `TIM2_IRQHandler`（你写的） | **`0x08000FF5`** | 接上了 |

> **所有函数地址都是奇数** —— 最低位 `bit0 = 1` 是 **Thumb 状态标志**（Cortex-M 只跑 Thumb）。

**5. 72MHz 下要产生 20kHz 的 PWM，ARR 和 PSC 怎么取？**

```
(PSC+1) × (ARR+1) = 72MHz / 20kHz = 3600
```

一个方程两个未知数 → **先定时基**：

| 方案 | 计数频率 | PSC | ARR | 分辨率 |
|---|---|---|---|---|
| **A（推荐）** | 72MHz（不分频） | **0** | **3599** | **3600 级最高** |
| B | 1MHz | 71 | 49 | 只有 50 级（很粗） |

**推荐 A**：`PSC = 0`（不分频）、`ARR = 3599`
校验：`(0+1) × (3599+1) = 3600` → `72MHz / 3600 = 20kHz` ✅，且 `ARR ≤ 65535` ✅

> **占空比** = `CCR / (ARR+1)` = `CCR / 3600`。要 25% → `CCR = 900`。

**6. 为什么 PWM 输出引脚必须配成复用推挽而不是普通推挽？**

| 模式 | 谁控制引脚电平 |
|---|---|
| 普通推挽 `OUT_PP` | **`ODR`**（软件写） |
| **复用推挽 `AF_PP`** | **外设的输出信号**（这里是 TIM1 的比较输出） |

**配错的后果**：定时器内部**一切正常**（`CNT` 在数、比较器在工作、`CCR1` 在变），
但**引脚输出由 `ODR` 决定**（你没写它）→ **引脚是一个固定电平**。

**现象**：**"寄存器全对，但引脚上就是没有波形"** ← 极难查的一类问题。

> **同一类问题的另一种表现**：PA8 交给 TIM1 后，再调 `Led_Toggle(LED_1)`（写 `ODR`）**完全无效**。
> **一个引脚只能有一个"主人"。**

**7. 改 `CCR` 时为什么要使能预装载（`OCxPE`）？**

因为**你的写操作和 `CNT` 数是"异步"的**：

```
假设 CNT 已经数到 400，你这时把 CCR 从 500 改成 200：
    CNT 0~400  ：按旧值 500 比较（已经过去了）
    CNT 400~999：按新值 200 比较 → 立刻变低
        ↓
   得到的是一个【被截断的、不完整】的周期  ← 这就是"毛刺"
```

**`OCxPE = 1`（预装载）**：
- 新写入的 `CCR` 先进入**影子寄存器**
- **等本周期结束（下一个更新事件 UEV）才真正生效**
- → **每个周期永远是完整的方波**，占空比不会出现"半个"

**同理 `ARPE`**：预装载 `ARR`，保证运行中改频率时周期不被截断。

> **一句话**：**"换参数要等这趟车跑完再换"**，不能跑到一半换。

**8. 串口下载时 BOOT0 应该是什么状态？下载完必须改回来吗？**

| 阶段 | BOOT0 | BOOT1 |
|---|---|---|
| **下载时** | **1**（进系统存储器 bootloader） | 0 |
| **下载完** | **必须改回 0**（主 Flash） | 0 |

**不改回来的后果**：重启后 CPU 又去跑 bootloader → **你的程序不执行**。
**现象**：**"下载明明显示成功，但程序不跑"** ← 最常见的"白忙一场"。

> **`BOOT0` 是复位瞬间采样一次的** —— 改完必须**断电重上电**（或按复位），运行中改无效。

---

### 拓展练习 · 参考答案

| # | 练习 | 本板可行性 | 答案要点 |
|---|---|---|---|
| **1** | 双通道 PWM 混色 | ❌ **做不了** | 笔记里的 `TIM3_CH1/CH2 → PA6/PA7` **被板载 LoRa 占用**（LORA_MISO/MOSI）；而且板上只有单色 LED，没有混色可看 |
| **2** | 舵机预热（50Hz, 1.5ms） | ⚠️ 有舵机才做 | **`PSC = 71`、`ARR = 19999`、`CCR1 = 1500`**<br>校验：`72×20000/72MHz = 20ms = 50Hz` ✅；`1.5ms → CCR = 1500` ✅<br>**引脚**：PA6 被占 → 第 06 章用 **`PA0 → TIM2_CH1`** |
| **3** | 可调频率 PWM（串口） | ⏳ 留到第 07 章 | 需要串口接收。骨架：收到数字 → `__HAL_TIM_SET_AUTORELOAD()` + `__HAL_TIM_SET_PRESCALER()`<br>⚠️ 改完要写 **`TIM1->EGR = TIM_EGR_UG`** 让新值立即生效 |
| **4** | 看 `.map` 文件 | ⭐ **建议做** | 见下方「收尾实验 B」 |
| **5** | 栈溢出实验 | ⭐ **建议做** | 见下方「收尾实验 C」 |
| **6** | 脱离 JLink（串口下载） | ⏭️ **本板跳过** | 原理已实测（`BOOT0=1 → PC=0x1FFFF35E`）；用 ST-Link 已够 |

#### 练习 2 的完整答案（舵机）

```
目标：50Hz 方波，脉宽 1.5ms（舵机中位）

第 1 步：周期 20ms → (PSC+1)(ARR+1) = 72MHz × 20ms = 1,440,000
第 2 步：定时基 1MHz（PSC = 71）→ ARR + 1 = 1,440,000 / 72 = 20000 → ARR = 19999
第 3 步：脉宽 1.5ms = 1500 个 1µs 计数 → CCR1 = 1500
```

| 参数 | 值 |
|---|---|
| PSC | **71** |
| ARR | **19999** |
| CCR1 | **1500**（中位） |

**舵机脉宽对照**（记住这个表，第 06 章直接用）：

| 脉宽 | 角度 | CCR1 |
|---|---|---|
| 0.5 ms | 0° | 500 |
| 1.5 ms | 90°（中位） | **1500** |
| 2.5 ms | 180° | 2500 |

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| PWM 完全没有输出 | 引脚没配成 `GPIO_MODE_AF_PP`；或 `HAL_TIM_PWM_Start` 没调用 |
| PWM 有波形但占空比不变 | 修改的是 `CCR` 但没生效，检查用的通道号是否对应 |
| 运行中改 CCR 出现毛刺 | `OCxPE` 预装载未使能 |
| 中断不触发 | ISR 函数名与向量表不一致（被 WEAK 空函数覆盖了） |
| 程序偶尔跑飞、变量乱变 | 栈溢出，检查 `_Min_Stack_Size` 和局部大数组 |
| 串口下载一直"等待应答" | BOOT0 没置 1、TX/RX 接反、没共地、波特率不匹配 |
| 下载后程序不运行 | BOOT0 没改回 0 |

---

## 七、拓展练习

1. **双通道 PWM**：TIM3 的 CH1、CH2 分别输出不同占空比，控制双色 LED 混色。
2. **舵机预热**：用 PWM 输出 50Hz、1.5ms 脉宽（为第 06 章铺垫）。
3. **可调频率 PWM**：串口接收参数动态修改 ARR 与 PSC。
4. **看 `.map` 文件**：在 `build/` 下找到 `.map`，找到 `main`、全局变量的地址，对照 `.data`/`.bss` 的理解。
5. **栈溢出实验**：在函数里定义 `uint8_t buf[4096]` 局部数组，观察程序崩溃现象。
6. **脱离 JLink**：用串口下载方式烧录一次程序，完整走一遍 BOOT 切换流程。

---

## 八、收尾实验（3 个，约 40 分钟）

> 做完这三个，学习目标里的「`.data` / `.bss`」和「`.map` + 栈」两项就打勾了。

### 实验 A · 亲手证明 `.data` / `.bss` 是 `__main` 处理的

```c
/* App/main.c 顶部 */
uint32_t g_data = 0x12345678;        /* .data   ：有初值 → 占 Flash，启动时搬到 RAM */
uint32_t g_bss;                      /* .bss    ：无初值 → 不占 Flash，启动时清零   */
const uint32_t g_rom = 0xDEADBEEF;   /* .rodata ：只在 Flash，不占 RAM             */

/* while(1) 里加两行 */
g_data++;
g_bss++;
HAL_Delay(100);
```

**验证步骤**：

| 步 | 动作 | 期望 |
|---|---|---|
| 1 | 「变量」窗口看三个变量的**地址** | `&g_rom` = **`0x0800xxxx`**（Flash）<br>`&g_data` / `&g_bss` = **`0x2000xxxx`**（RAM） |
| 2 | 让它跑 30 秒 | `g_data` / `g_bss` 涨到几百 |
| 3 | **按复位键** | `g_data` → **`0x12345678`**；`g_bss` → **`0`** |

**结论**：**`.data` 被从 Flash 搬回 RAM、`.bss` 被清零** —— 都是 `__main` 的 `__scatterload` 干的。
**`const` 变量在 Flash 里，不占 RAM** —— 这是省 RAM 的重要手段。

### 实验 B · 看 `.map` 文件 + 栈分析

1. 在 `build/` 目录下找 **`.map`** 文件（EIDE 构建后生成）
2. 搜 **`g_data`** → 看它的**地址**（应该和实验 A 里变量窗口看到的**一致**）
3. 搜 **`main`**、**`Reset_Handler`**
4. 搜 **`STACK`** / **`HEAP`** → 看栈的大小和位置
5. 在 `Arm/startup_stm32f103xb.s` 里搜 **`Stack_Size`**（默认 `0x00000400` = **1KB**）

**要能回答**：
- `__initial_sp` 是多少？（应该在 `0x2000xxxx`，**RAM 顶端附近**）
- 栈有多大？够不够？（**局部大数组是栈的头号杀手**）

### 实验 C · 栈溢出实验（**破坏性，做完立刻删**）

```c
static void Boom(void)
{
    uint8_t buf[4096];      /* 4KB 局部数组 —— 而栈只有 1KB！ */
    buf[0] = 1U;            /* 防止被编译器优化掉 */
    (void)buf;
}

/* 在 while(1) 里调一次 Boom();  */
```

**期望现象**：

| 观察 | 结果 |
|---|---|
| 程序 | **卡死 / 跑飞** |
| 断点 | 停在 `Boom()` 里后**回不去** `while(1)` |
| PC | 很可能落在 **`HardFault_Handler` 的 `B .`** |
| 其它变量的值 | 被**莫名其妙改写**（因为栈把别人的内存踩了） |

**⚠️ 实验完立刻把 `Boom()` 调用和函数删掉。**

> **技术债那条"栈溢出"的坑，从此你会看一眼就认出来**：
> **"程序偶尔跑飞 / 全局变量无故变化" → 先查栈。**
