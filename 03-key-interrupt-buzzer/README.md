# 03 · 按键开发 + 蜂鸣器 + 中断

> 对应课程：03-按键开发+蜂鸣器+中断
> 前置要求：02 GPIO 外设 + LED
> 建议耗时：5-7 天
> 已有验证工程：`../02-exti`

---

## 一、学习内容

### 1. 按键的电气特性
- 机械触点抖动：按下/松开瞬间 5-10ms 内电平不稳定
- 消抖方案：**软件延时消抖**（最简单） / 硬件 RC 消抖 / 定时器扫描
- 上拉输入 + 按下接地（低电平有效）是最常见接法

### 2. 轮询 vs 中断

| 方式 | 优点 | 缺点 | 适用 |
|---|---|---|---|
| 轮询 | 简单、逻辑直观 | 浪费 CPU，响应慢，实时性差 | 简单实验、非实时任务 |
| 中断 | 响应及时、CPU 利用率高 | 逻辑复杂，需考虑并发与优先级 | 按键、事件触发类 |

### 3. 中断体系三层结构

```
外设（EXTI 检测边沿）→ NVIC（优先级仲裁）→ CPU（跳转执行 ISR）
```

#### EXTI（外部中断/事件控制器）
- 20 条 EXTI 线：EXTI0~15 对应引脚，EXTI16~19 对应 PVD/RTC/USB/以太网
- **重要**：EXTI0 只能接 PA0/PB0/PC0 中的**一个**——同一编号的引脚互斥
- 触发方式：上升沿 / 下降沿 / 双边沿
- 关键寄存器：`IMR`（中断屏蔽）、`EMR`（事件屏蔽）、`RTSR`（上升沿）、`FTSR`（下降沿）、`PR`（挂起标志，**必须软件清除**）

#### AFIO（复用功能 IO）
- EXTI 引脚映射通过 `AFIO_EXTICR1~4` 配置
- 使用 EXTI 时必须**使能 AFIO 时钟**（`RCC_APB2ENR` 的 `AFIOEN`，F1 特有，容易漏）

#### NVIC（嵌套向量中断控制器）
- 优先级 = 抢占优先级 + 响应优先级（子优先级）
- 优先级分组：`NVIC_PRIORITYGROUP_0` ~ `4`，决定两者各占几位
- 数值**越小优先级越高**
- 抢占优先级高的可以打断正在执行的低优先级中断（嵌套）；子优先级不能打断，只决定同时挂起时谁先执行

### 4. 中断服务函数要点
- 名称固定，来自启动文件向量表：`EXTI0_IRQHandler`、`EXTI15_10_IRQHandler` 等
- 必须**清除中断标志**（`EXTI->PR = ...`），否则反复进中断
- ISR 要尽量短：置标志位交给主循环处理，不要在 ISR 里 `HAL_Delay()`
- 共享变量加 `volatile`

### 5. 蜂鸣器
- 有源蜂鸣器：给电就响，频率固定
- 无源蜂鸣器：需输入方波，频率决定音调（为第 05 章 PWM 铺垫）
- 驱动方式：GPIO 直驱电流不足，一般用三极管/MOS 驱动
- 续流：感性负载建议并联二极管

---

## 二、学习目标

- [ ] 能手写按键软件消抖逻辑（延时确认法）
- [ ] 能说清 EXTI、AFIO、NVIC 三者的分工
- [ ] 能配置任意引脚的外部中断，并写出对应的 ISR
- [ ] 能解释优先级分组和抢占/子优先级的区别
- [ ] 理解为什么 ISR 里必须清标志位
- [ ] 用中断方式重写按键控制 LED，与轮询版对比

---

## 三、学习方法

1. **先轮询后中断**：先用轮询实现"按键控制 LED"，感受占用 CPU 的问题，再改成中断——对比出中断的价值。
2. **单步看寄存器**：在 EXTI 中断里打断点，打开 `Peripherals → EXTI`，观察 `PR` 从 1 变 0 的过程（说明清标志生效）。
3. **双按键互斥实验**：同时用 PA0 和 PB0 配 EXTI0，观察只有后配置的那个生效——理解"同编号互斥"。
4. **优先级实验**：配两个不同优先级的按键中断，在低优先级 ISR 里延时，验证高优先级能否嵌套。
5. **故意不清标志**：注释掉清标志代码，观察程序卡死在中断里——这个教训很深刻。

---

## 四、标准示例

### 1. 按键软件消抖（轮询版）

```c
#define KEY_PORT  GPIOA
#define KEY_PIN   GPIO_PIN_0

/* 按下返回 1，带消抖 */
static uint8_t Key_Scan(void)
{
    if (HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN) == GPIO_PIN_RESET) {
        HAL_Delay(20);                                        /* 延时消抖 */
        if (HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN) == GPIO_PIN_RESET) {
            while (HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN) == GPIO_PIN_RESET);  /* 等待松手 */
            return 1;
        }
    }
    return 0;
}
```

### 2. EXTI 按键中断完整配置

```c
/* ---------- 初始化 ---------- */
static void KEY_EXTI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1. 时钟：GPIOA + AFIO（F1 用 EXTI 必须开 AFIO） */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* 2. GPIO：上拉输入 */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;   /* 下降沿触发，HAL 会自行配置 EXTI */
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 3. NVIC：使能 EXTI0 中断，设优先级 */
    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/* ---------- 中断服务函数（名字来自启动文件向量表，不可改） ---------- */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);   /* HAL 内部会清标志 + 回调 */
}

/* ---------- 回调（HAL 提供的弱函数，重写即可） ---------- */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8);
    }
}
```

### 3. 寄存器版 EXTI（理解底层）

```c
/* 时钟 */
RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;

/* PA0 上拉输入 */
GPIOA->CRL &= ~(0xFU << 0);
GPIOA->CRL |=  (0x8U << 0);
GPIOA->ODR |=  GPIO_PIN_0;

/* 把 EXTI0 映射到 PA0（AFIO_EXTICR1 的 bit[3:0] = 0000） */
AFIO->EXTICR[0] &= ~0xFU;

/* 下降沿触发 + 解除屏蔽 */
EXTI->FTSR |= EXTI_FTSR_TR0;
EXTI->IMR  |= EXTI_IMR_MR0;

/* NVIC */
NVIC_SetPriority(EXTI0_IRQn, 2);
NVIC_EnableIRQ(EXTI0_IRQn);

/* ISR 里必须清标志 */
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR0) {
        EXTI->PR = EXTI_PR_PR0;   /* 写 1 清除 */
        /* 业务代码 */
    }
}
```

### 4. 中断与主循环解耦（推荐写法）

```c
volatile uint8_t g_key_flag = 0;    /* 必须 volatile */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0) {
        g_key_flag = 1;             /* ISR 只置标志，不做耗时操作 */
    }
}

int main(void)
{
    /* ... 初始化 ... */
    while (1) {
        if (g_key_flag) {
            g_key_flag = 0;
            HAL_Delay(20);                          /* 主循环里消抖 */
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8);
            }
        }
    }
}
```

### 5. 蜂鸣器（有源，GPIO 直控）

```c
#define BEEP_PORT GPIOB
#define BEEP_PIN  GPIO_PIN_12

__HAL_RCC_GPIOB_CLK_ENABLE();
GPIO_InitTypeDef g = {0};
g.Pin = BEEP_PIN; g.Mode = GPIO_MODE_OUTPUT_PP;
g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(BEEP_PORT, &g);

HAL_GPIO_WritePin(BEEP_PORT, BEEP_PIN, GPIO_PIN_SET);   /* 鸣叫 */
```

---

## 五、自我检测

1. EXTI、AFIO、NVIC 各自负责什么？少配一个会怎样？
2. 为什么 PA0 和 PB0 不能同时用作 EXTI0？
3. 中断服务函数名能不能自己改？为什么？
4. 不清 `EXTI->PR` 标志会发生什么？
5. 抢占优先级和子优先级有什么区别？
6. `NVIC_PRIORITYGROUP_2` 时，抢占和子优先级各占几位？
7. 为什么 ISR 里不能调用 `HAL_Delay()`？
8. ISR 与主循环共享的变量为什么要加 `volatile`？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| 中断完全不触发 | 忘记使能 AFIO 时钟（F1 特有）；或 NVIC 没使能 |
| 中断反复触发、程序卡死 | ISR 里没清 `EXTI->PR` 标志 |
| 按键一次触发多次 | 机械抖动，需要消抖 |
| 中断触发但 LED 不变 | ISR 函数名拼错，实际执行的是启动文件里的弱定义空函数 |
| 高优先级中断无法打断低优先级 | 优先级分组配置不对，或两者抢占优先级相同 |
| 主循环读不到 ISR 改的变量 | 变量没加 `volatile`，被编译器优化 |
| 用了 PB0 但没反应 | 另一个引脚（如 PA0）已占用 EXTI0 线 |

---

## 七、拓展练习

1. **双击检测**：500ms 内连续两次按下识别为双击，实现双击切换 LED。
2. **长按检测**：按住超过 1 秒触发不同行为（提示：结合 SysTick 计时）。
3. **多按键中断**：PA1、PA2、PA3 分别控制不同 LED，用 `EXTI1_IRQHandler` 和 `EXTI3_IRQHandler`。
4. **优先级验证**：给两个按键配不同抢占优先级，在低优先级 ISR 里跑 500ms 循环，验证嵌套。
5. **无源蜂鸣器演奏**：用 `HAL_Delay` 循环翻转 GPIO 输出不同频率，演奏简单音阶（为 PWM 铺垫）。
6. **寄存器重写**：把本节的 EXTI 例程完全用寄存器实现一遍，与原 HAL 版对比代码量。
