# 05 · 启动模式 + PWM + 启动文件分析 + 串口下载

> 对应课程：05-启动模式+PWM+启动文件分析+串口下载
> 前置要求：04 定时器 + SysTick
> 建议耗时：5-7 天

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

- [ ] 能说清三种启动模式的区别与切换方法
- [ ] 能画出中断向量表的排布，解释为什么改 ISR 函数名会导致中断不触发
- [ ] 能解释 `.data` / `.bss` 的作用与启动时的处理
- [ ] 能独立计算 PWM 频率与占空比，实现呼吸灯
- [ ] 掌握串口下载的完整流程，能在没有 JLink 时救活板子
- [ ] 理解"栈溢出"会导致什么现象，会看 `.map` 文件

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
