# 03 · 按键开发 + 蜂鸣器 + 中断

> 对应课程：03-按键开发+蜂鸣器+中断
> 前置要求：02 GPIO 外设 + LED
> 建议耗时：5-7 天
> 已有验证工程：`../02-exti`

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)；学习用板 = **中控板（网关）**）
>
> | 用途 | 笔记示例 | **实板用这个** | 说明 |
> |---|---|---|---|
> | 按键 | PA0 | **PB12 = SW2 / PB13 = SW3** | 板上已有 2 个按键，10kΩ 上拉，按下 = 低电平 |
> | 指示 LED | PA8 | **PA8 = LED1 / PB15 = LED2** | 低电平点亮 |
> | 蜂鸣器 | PB12 | **中控板无蜂鸣器**；节点板 = **PB9** | 原示例 PB12 与按键 SW2 冲突，本文已改 |
> | EXTI 编号 | EXTI0 | **EXTI12 / EXTI13** | 两条线**共用中断向量 `EXTI15_10_IRQHandler`** |
>
> **本板注意**
> 1. 板上只有 SW2/SW3 两个按键，笔记里「PA0 和 PB0 配 EXTI0 互斥实验」在本板**无法复现**（PB12/PB13 不在同一条 EXTI 线上）。
> 2. 反过来，本板会撞上一个更真实的场景：**两个按键进同一个 ISR**，必须在回调里用 `GPIO_Pin` 区分是谁触发的。
> 3. 蜂鸣器部分：中控板做不了，用 LED 代替；若手上有节点板，用 **PB9**。

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

- [x] 能手写按键软件消抖逻辑（延时确认法）
- [x] 能说清 EXTI、AFIO、NVIC 三者的分工
- [x] 能配置任意引脚的外部中断，并写出对应的 ISR（**手写寄存器版**）
- [ ] 能解释优先级分组和抢占/子优先级的区别 —— **挪到第 04 章**
      （本板两个按键共用 `EXTI15_10_IRQn` 一条向量，做不了"不同优先级"实验；等有定时器中断后可配对做）
- [x] 理解为什么 ISR 里必须清标志位
- [x] 用中断方式重写按键控制 LED，与轮询版对比

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

> ⚠️ **引脚勘误**：本节原示例用的是 `PB12`，但实板（中控板）上 **PB12 是按键 SW2**！
> 详见 `../硬件说明.md`：
> - **中控板（网关）**：**没有蜂鸣器** → 本章跳过，或用 LED 代替
> - **节点板（终端）**：蜂鸣器 = **PB9**，有源 3kHz，S8050 NPN 低边驱动，**输出高 = 响**

**节点板（终端）的实际写法：**

```c
#define BEEP_PORT GPIOB
#define BEEP_PIN  GPIO_PIN_9        /* 节点板 BEEP = PB9（中控板无此电路） */

__HAL_RCC_GPIOB_CLK_ENABLE();
GPIO_InitTypeDef g = {0};
g.Pin = BEEP_PIN; g.Mode = GPIO_MODE_OUTPUT_PP;
g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(BEEP_PORT, &g);

HAL_GPIO_WritePin(BEEP_PORT, BEEP_PIN, GPIO_PIN_SET);   /* 鸣叫（NPN 低边，高电平导通） */
HAL_Delay(200);
HAL_GPIO_WritePin(BEEP_PORT, BEEP_PIN, GPIO_PIN_RESET); /* 停止 */
```

> **有源 vs 无源**：实板是**有源**蜂鸣器 → 给高电平就响，**不需要输出方波**。
> 所以「拓展练习 5 · 演奏音阶」在这块板上做不了，要用**无源**蜂鸣器配合 PWM，留到第 05 章。

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

### 自我检测 · 参考答案

#### Q1 · EXTI / AFIO / NVIC 的分工

| 组件 | 职责 | 一句话 | 少配的后果 |
|---|---|---|---|
| **AFIO** | 把 `EXTIx` 线**路由**到具体端口 | "告诉 EXTI 去盯哪个引脚" | `AFIO->EXTICR` 写入被丢弃 → EXTI 盯着默认的 PAx |
| **EXTI** | 检测边沿并**锁存**到 `PR`，查 `IMR` 决定是否上报 | "发现有人按了" | `FTSR` 没配 → 不检测边沿；`IMR` 没配 → 只产生事件，不进中断 |
| **NVIC** | 优先级**仲裁** + 放行到 CPU | "决定让不让它打断 CPU" | 请求已发出但没人理 → 中断不触发 |

**要记住的一点**：**这几种"少配"的现象完全一样 —— 都是"什么反应都没有"，而且编译器不报错。**
所以只能靠"我知道这里**必须**有一个动作"逐条核对（`Key_Init()` 里的 `[1]~[4]` 就是这张核对表）。

#### Q2 · 为什么 PA0 和 PB0 不能同时做 EXTI0

**因为 EXTI 线按"引脚编号"分配，不按端口。**

```
一共只有 16 条线：EXTI0 ~ EXTI15
EXTI0 只能接【一个】：PA0 / PB0 / PC0 三选一

谁决定？AFIO->EXTICR[0] 的那 4 位：
    0000 = PA   0001 = PB   0010 = PC  ...
一个寄存器位段只能存一个值 → 只能选一个端口（后配的覆盖先配的）
```

**第二层原因（更本质）**：即使硬件能同时接两个端口，**软件也分不清是谁触发的** ——
ISR 只有一个 `EXTI0_IRQHandler`，`PR` 里只有一位。

**解法**：用不同编号的引脚，如 `PA0(EXTI0)` + `PB1(EXTI1)`。
**本板正好绕开**：`PB12 → EXTI12`、`PB13 → EXTI13`，编号不同，可以共存。

#### Q3 · 中断服务函数名能不能自己改

**被硬件直接调用的入口名不能改；但可以"转发"到自己命名的函数。**

```
启动文件里：
    __Vectors:  DCD  EXTI0_IRQHandler      ← 编译期就把符号地址固化在 Flash 固定位置
                EXPORT EXTI0_IRQHandler [WEAK]   ← 弱定义，实现是 B . 死循环
                        ↓
        只有【完全同名】的强定义才能覆盖
                        ↓
    拼错一个字母 → 链接器不报错（弱定义还在）→ 你写的是个"没人调用的普通函数"
                        ↓
    中断真来了 → 跳进弱定义 → B . 死循环 → 程序卡死
```

**检查方法**：在 `.map` 里搜这个符号，看它被链接到哪个地址（在启动文件那一带就说明没生效）。

**正确做法（本工程就是）**：

```c
/* stm32f1xx_it.c —— 名字必须和向量表一致 */
void EXTI15_10_IRQHandler(void)
{
    Key_IrqHandler();      /* ← 这个名字随便起 */
}
```

#### Q4 · 不清 `EXTI->PR` 会怎样

**不是"多进几次中断"，而是"整个系统瘫死"。**

```
ISR 执行完 → BX LR → 内核识别为异常返回
        ↓
内核检查：中断源【还在请求】吗？
        ↓
PR 那一位还是 1 + IMR 还开着 → 请求【仍然有效】
        ↓
NVIC 立刻重新挂起 → 立刻再进 ISR → 出来一步又进去 …… 无限循环
        ↓
CPU 100% 在 ISR 里转，主循环【一个字节都执行不到】
```

**分清两类"挂起位"**：

| | 谁置的 | 谁清 |
|---|---|---|
| `EXTI->PR`（**外设**挂起位） | EXTI 硬件 | **软件清（你）** |
| NVIC 的挂起/活动位 | NVIC | 异常进入/返回时**硬件自动**维护 |

**验证**：注释掉清标志那一行 → 按一次按键 → 程序卡死（第 8.3 节的破坏性实验）。

#### Q5 · 抢占优先级 vs 子优先级

| | **抢占优先级** | **子优先级**（响应优先级） |
|---|---|---|
| 能否**打断**正在跑的中断 | **能** | **不能** |
| 作用时机 | 任何时刻 | 只在"**同时挂起**"时决定谁先 |
| 记法 | 能不能**插队** | 同时排队时**谁站前面** |

**数值越小，优先级越高**（和直觉相反）。

#### Q6 · `NVIC_PRIORITYGROUP_2` 各占几位

| 分组 | 抢占位数 | 子优先级位数 | 抢占取值 | 子取值 |
|---|---|---|---|---|
| `GROUP_0` | 0 | 4 | 无 | 0~15 |
| `GROUP_1` | 1 | 3 | 0~1 | 0~7 |
| **`GROUP_2`** | **2** | **2** | **0~3** | **0~3** |
| `GROUP_3` | 3 | 1 | 0~7 | 0~1 |
| `GROUP_4` | 4 | 0 | 0~15 | 无 |

**本工程用的是 `GROUP_4`**（`HAL_Init()` 里写死）→ **4 位全给抢占**，
所以 `HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0)` 里的第 3 个参数**被忽略**。

#### Q7 · 为什么 ISR 里不能调用 `HAL_Delay()`

```c
/* HAL_Delay 的本质 */
while ((HAL_GetTick() - tickstart) < Delay);   /* ← 死等 uwTick 增加 */
                        ↑ uwTick 靠 SysTick 中断递增
```

```
SysTick 优先级 = TICK_INT_PRIORITY = 15（最低）
按键 ISR 优先级 = 2（高）
        ↓
按键 ISR 在跑 → SysTick 被压住进不来 → uwTick 停止增长
        ↓
HAL_Delay 的 while 永远等不到 → 死循环 → ISR 退不出来 → 【系统彻底卡死】
```

**更根本的原因**：**ISR 在跑的时候，同优先级和低优先级的中断都进不来。**

| 需求 | 正确做法 |
|---|---|
| 微秒级精确等待 | 轮询 `SysTick->VAL`（**不依赖中断**），但也要尽量短 |
| 毫秒级以上 | **别在 ISR 里等**：记时间戳，回主循环处理 |
| **最佳实践** | **ISR 里只置标志，主循环干活** |

#### Q8 · 为什么要 `volatile`

**不加会怎样**：

```c
while (g_flag == 0) { }    /* 编译器认为"这个循环里没人改它" */
                           /* → 读一次放进寄存器，以后一直用旧值 */
```

→ ISR 改了内存，**主循环永远看不到** → 死等。

**必须加的场合**：

| 场合 | 要加吗 |
|---|---|
| ISR **写**、主循环**读** | **必须** |
| ISR 与主循环**都写** | **必须**（但还要考虑原子性） |
| **硬件寄存器**（`EXTI->PR`、`GPIOA->IDR`…） | **必须**（`__IO` 宏本质就是 `volatile`） |
| DMA 缓冲区 | **必须** |
| 只在主循环里用 | 不需要（加上无害） |

**⚠️ 重要补充：`volatile` 不保证原子性**

```c
g_count++;      /* 读 → 改 → 写，三步。中断插在中间会丢更新 */
```

要让"读-改-写"安全，得用**临界区**：`__disable_irq()` / `__enable_irq()`。

**本工程为什么安全**：`s_keyFlag[i]` 是 **ISR 只写 `1`、主循环只写 `0`**，
双方都只做**单字节写入**、没有"读-改-写"，且 Cortex-M 上 `uint8_t` 单字节读写**本身就是原子的**
→ **天然免锁**。

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

---

### 拓展练习 · 思路与答案

> 实现基于本工程的骨架（`Key_HasEvent()` 取事件、`Key_IsPressed()` 读状态）。
> **先看每个练习的「实板适配」—— 笔记原文用的引脚在本板上大多不可用。**

#### 练习 1 · 双击检测

**思路**：第一次按下**不立即响应**，等 500ms 看有没有第二次：
- 有 → **双击**
- 超时没有 → **单击**

**⚠️ 与现有代码的冲突（重要）**：`App/main.c` 的 `Key_Task()` 里有 **100ms 时间窗**，
双击的第二次按下若落在 100ms 内会被它吃掉。**所以做双击必须用独立的状态机，不要叠在 `Key_Task` 上。**

```c
typedef enum { CLICK_NONE = 0, CLICK_SINGLE, CLICK_DOUBLE } Click_Event_t;

static Click_Event_t Key_Click_Task(Key_Id_t keyNo)
{
    static uint8_t  s_waitSecond[KEY_NUM] = {0};
    static uint32_t s_firstTick[KEY_NUM]  = {0};

    if (Key_HasEvent(keyNo))                    /* 收到"按下"事件 */
    {
        if (s_waitSecond[keyNo] == 0U)          /* 第一次按下 */
        {
            s_waitSecond[keyNo] = 1U;
            s_firstTick[keyNo]  = HAL_GetTick();
        }
        else                                    /* 500ms 内又按了一次 */
        {
            s_waitSecond[keyNo] = 0U;
            return CLICK_DOUBLE;
        }
    }

    /* 超时判定：等了 500ms 还没等到第二次 → 单击 */
    if (s_waitSecond[keyNo] != 0U &&
        (uint32_t)(HAL_GetTick() - s_firstTick[keyNo]) >= 500U)
    {
        s_waitSecond[keyNo] = 0U;
        return CLICK_SINGLE;
    }

    return CLICK_NONE;
}

/* 主循环用法 */
Click_Event_t ev = Key_Click_Task(KEY1);
if (ev == CLICK_SINGLE) { Led_Toggle(LED_1); }
if (ev == CLICK_DOUBLE) { /* 双击的另一个动作 */ }
```

**代价**：**单击响应延迟 500ms**（必须等"确认不是双击"）。
这是"单击/双击共存"的必然代价 —— 想避免就得用更短的窗口 + 更精细的状态机。

#### 练习 2 · 长按检测

**思路**：`Key_HasEvent()` 只给"**按下瞬间**"，**按住/松开要靠 `Key_IsPressed()` 每圈轮询**；
按住时长用 `HAL_GetTick()` 算，**不用 `HAL_Delay`**（保持非阻塞）。

```c
static uint32_t s_downTick[KEY_NUM];
static uint8_t  s_holding[KEY_NUM];
static uint8_t  s_longFired[KEY_NUM];

void Key_LongPress_Task(Key_Id_t keyNo)
{
    if (Key_HasEvent(keyNo))                    /* 按下瞬间：开始计时 */
    {
        s_downTick[keyNo]  = HAL_GetTick();
        s_holding[keyNo]   = 1U;
        s_longFired[keyNo] = 0U;                /* 本次按下还没触发过长按 */
    }

    if (s_holding[keyNo] == 0U) { return; }

    if (Key_IsPressed(keyNo))                   /* 还按着 */
    {
        if (s_longFired[keyNo] == 0U &&
            (uint32_t)(HAL_GetTick() - s_downTick[keyNo]) >= 1000U)
        {
            s_longFired[keyNo] = 1U;            /* 旗标：防止重复触发 */
            /* → 长按事件 */
        }
    }
    else                                        /* 已松开 */
    {
        s_holding[keyNo] = 0U;
        if (s_longFired[keyNo] == 0U)
        {
            /* → 短按事件 */
        }
    }
}
```

**三个状态必须分开存**：

| 变量 | 作用 |
|---|---|
| `s_holding` | 现在按着吗 |
| `s_longFired` | 长按**已经触发过**吗（防重复） |
| `s_downTick` | 按下的时刻（用来算时长） |

#### 练习 3 · 多按键中断

**实板适配先说清楚**：

| 笔记原文 | 本板情况 |
|---|---|
| `PA1 / PA2 / PA3` | **PA2/PA3 被 USART2 占用**（中控板），且这些引脚上**没有按键** |
| 用 `EXTI1_IRQHandler` / `EXTI3_IRQHandler` | 想体验**独立向量**，必须选**不同 EXTI 分组**的引脚 |

**本板可行的改法**（在排针上外接按键）：

| 引脚 | EXTI 线 | 落在哪个向量 | 价值 |
|---|---|---|---|
| `PA0` | EXTI0 | `EXTI0_IRQHandler` | **独立向量** |
| `PB5` | EXTI5 | `EXTI9_5_IRQHandler` | 5~9 共用 |
| `PA11` | EXTI11 | `EXTI15_10_IRQHandler` | 10~15 共用（**和板载按键同组**） |

**这个练习真正的收获**是理解"**独立向量 vs 分组共用**"：

```c
/* 独立向量：一个 ISR 只管一条线，代码最简 */
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR0)
    {
        EXTI->PR = EXTI_PR_PR0;
        /* 业务 */
    }
}

/* 分组向量：一个 ISR 要照顾 6 条线 —— 所以必须【循环 + 判断】 */
void EXTI15_10_IRQHandler(void)
{
    for (uint8_t i = 0U; i < KEY_HW_NUM; i++)
    {
        if (__HAL_GPIO_EXTI_GET_IT(g_keyHw[i].pin))
        {
            EXTI->PR = g_keyHw[i].pin;
            s_keyFlag[i] = 1U;
        }
    }
}
```

> **你写的 `Key_IrqHandler()` 已经是"分组向量"的标准形态** —— 那个 `for` + `if` 就是这个练习的答案。

#### 练习 4 · 优先级验证 —— **挪到第 04 章**

**为什么现在做不了**：本板两个按键是 `PB12/PB13` → **共用 `EXTI15_10_IRQn` 一条向量**，
**同一条向量不可能配两个不同优先级**。

**正确做法**：用**两个不同来源**的中断配对 —— **按键中断 + 定时器中断**（第 04 章会有 TIM2）。

**实验设计**：

| 组合 | 抢占优先级 | 预期现象 |
|---|---|---|
| EXTI=2, TIM=1 | TIM 更高 | TIM 中断**能打断**按键 ISR |
| EXTI=2, TIM=3 | EXTI 更高 | TIM 中断**打不断**，只能排队等 |
| 两者都 = 2 | 相同 | 不能嵌套，只能排队 |

**观察方法**：在按键 ISR 里跑一个 **500ms 忙等**，同时让 TIM 每 10ms 中断一次并翻转另一个 LED：

| 现象 | 结论 |
|---|---|
| 那个 LED 在 500ms 内**继续闪** | ✅ 嵌套成功（TIM 抢占优先级更高） |
| 那个 LED **卡住 500ms 不动** | ❌ 没嵌套（抢占相同或更低） |

**⚠️ 两个致命陷阱**：

1. **忙等只能用 `for (volatile uint32_t i = 0; i < N; i++);`，绝对不能用 `HAL_Delay`**
   —— 按键 ISR 会挡住优先级最低的 SysTick → `HAL_GetTick()` 停摆 → `HAL_Delay` 死循环
   （**这一条本身就是自我检测 Q7 的实证**）
2. 在 ISR 里跑 500ms 会**暂停整个系统 500ms** —— **只在教学实验里这么干**，真实项目绝不允许

#### 练习 5 · 无源蜂鸣器演奏

**实板适配（关键）**：

| 板子 | 蜂鸣器 | 能演奏吗 |
|---|---|---|
| **中控板（网关）** | **没有** | ❌ |
| **节点板（终端）** | **PB9，有源 3kHz** | ❌ **有源蜂鸣器频率固定，改不了调** |

**三个替代方案**：

| 方案 | 做法 | 收获 |
|---|---|---|
| **A** | **用 LED 代替**：把"频率"用闪烁节奏表示 | 验证时序代码对不对 |
| **B** | 驱动 PB9 的**有源**蜂鸣器输出方波 | **只会听到"嘟嘟"断续声，不能变调** —— 这个"失败"本身就是最好的对比实验 |
| **C** | 买一个**无源**蜂鸣器接在空闲引脚（`PA11` / `PB5`） | 真正听到音阶 |

**无源蜂鸣器代码**（输出方波，**频率决定音调**）：

```c
/* 中音阶频率表：Do Re Mi Fa Sol La Si Do */
static const uint16_t s_noteFreq[8] = { 262, 294, 330, 349, 392, 440, 494, 523 };

/* 需要微秒级延时（第 04 章的 SysTick 轮询版 Delay_us） */
void Beep_PlayTone(uint16_t freqHz, uint32_t durationMs)
{
    uint32_t halfUs = 1000000UL / freqHz / 2UL;              /* 半周期 */
    uint32_t cycles = (uint32_t)freqHz * durationMs / 1000UL; /* 要发多少个周期 */

    for (uint32_t i = 0U; i < cycles; i++)
    {
        BEEP_H();
        Delay_us(halfUs);
        BEEP_L();
        Delay_us(halfUs);
    }
}

void Beep_PlaySong(void)
{
    for (uint8_t i = 0U; i < 8U; i++)
    {
        Beep_PlayTone(s_noteFreq[i], 300U);   /* 每个音 300ms */
        HAL_Delay(50U);                        /* 音符之间留空 */
    }
}
```

**核心认知**：

| 蜂鸣器类型 | 驱动方式 | 能否变调 |
|---|---|---|
| **有源** | **给高电平就响** | ❌ 频率由内部振荡电路决定 |
| **无源** | **必须给方波** | ✅ **方波频率 = 音调** |

> 第 05 章的**硬件 PWM** 会用定时器自动产生方波，**CPU 完全不用管** ——
> 而上面这个 `for` 循环会 **100% 占满 CPU**。这就是"软件 PWM vs 硬件 PWM"的差别。

#### 练习 6 · 寄存器重写

**你已经完成一半** —— `Key_IrqHandler()` 就是手写寄存器版。
**还差**：把 `Key_Init()` 也改成纯寄存器。

```c
void Key_Init_REG(void)
{
    /* [1] 时钟：GPIOB + AFIO（F1 特有） */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    /* [2] PB12/PB13 配成浮空输入
     *     CRH 管 PB8~PB15，每引脚 4 位，offset = (引脚号 - 8) × 4 */
    GPIOB->CRH &= ~(0xFU << 16);   GPIOB->CRH |= (0x4U << 16);   /* PB12: bits[19:16] */
    GPIOB->CRH &= ~(0xFU << 20);   GPIOB->CRH |= (0x4U << 20);   /* PB13: bits[23:20] */

    /* [3] AFIO 映射：EXTI12/13 → GPIOB
     *     EXTICR[3] 管 EXTI12~15；PB12 在 bits[3:0]，PB13 在 bits[7:4]；1 = GPIOB */
    AFIO->EXTICR[3] &= ~(0xFFU << 0);
    AFIO->EXTICR[3] |=  (0x11U << 0);

    /* [4] EXTI：只检测下降沿 + 解除中断屏蔽 + 清残留 */
    EXTI->RTSR &= ~(GPIO_PIN_12 | GPIO_PIN_13);   /* 不要上升沿 */
    EXTI->FTSR |=  (GPIO_PIN_12 | GPIO_PIN_13);   /* 要下降沿   */
    EXTI->IMR  |=  (GPIO_PIN_12 | GPIO_PIN_13);   /* 允许中断   */
    EXTI->PR    =  (GPIO_PIN_12 | GPIO_PIN_13);   /* 清残留（**写 1 清除**，只能用 =） */

    /* [5] NVIC（用 CMSIS 接口，不是 HAL 的） */
    NVIC_SetPriority(EXTI15_10_IRQn, 2);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}
```

**代码量对比（诚实结论）**：

| 版本 | 行数 | 要掌握的知识 |
|---|---|---|
| HAL 版 | 约 12 行 | 只需知道结构体字段名 |
| 寄存器版 | 约 15 行 | `CRH` 位偏移、`EXTICR` 分组、AFIO 时钟、`FTSR`/`IMR`/`PR` |

**寄存器版并不更短。** 它的价值在于"**完全可控、看得见每一步**"：

| 场景 | 更适合 |
|---|---|
| 引脚固定、追求可控性 / 代码体积 | **寄存器版** |
| 引脚任意、可读性、团队协作 | **HAL 版** |

> **HAL 真正的价值在"任意引脚通用"**：`HAL_GPIO_Init()` 内部那个 `position` 循环，
> 把"引脚编号 → 位偏移"的换算封装掉了。
> 寄存器版写死引脚很直接，**换成任意引脚就要自己写位运算循环**——那正是在重写 HAL。

---

## 八、我的实现与实测（第 03 章产出）

### 8.1 代码结构（四层解耦）

| 文件 | 职责 | 关键设计 |
|---|---|---|
| `Driver/key.h` | 接口：**4 个函数，不暴露任何变量** | 只 `#include <stdint.h>`，与 HAL 解耦 |
| `Driver/key.c` | 实现：硬件映射表 + `static` 事件标志 + ISR 主体 | 标志 `static` → 不可能重复定义、不可能被误改 |
| `Driver/stm32f1xx_it.c` | `EXTI15_10_IRQHandler()` **一行转发** | 引脚知识不泄漏到中断文件 |
| `App/main.c` | `Key_Task()` 三道闸门 | 非阻塞 |

### 8.2 4 个函数的职责

| 函数 | 运行在 | 能阻塞吗 | 一句话 |
|---|---|---|---|
| `Key_Init()` | 主循环（开机一次） | 可以 | 时钟 → GPIO → 清挂起 → NVIC |
| `Key_IrqHandler()` | **中断** | **绝对不能** | 查 → 清 → 报（**手写寄存器版**） |
| `Key_IsPressed()` | 主循环 | — | 读 `IDR`，回答"**现在**按着吗"（状态） |
| `Key_HasEvent()` | 主循环 | — | 读走即清，回答"**刚刚**按过吗"（事件） |

> **核心设计**：中断负责"**界定事件边界**"（一次下降沿 = 一个事件），
> 所以"状态"和"事件"可以彻底分开，**两者都不需要阻塞**。

### 8.3 验收清单

| # | 测试 | 期望 | 实测 |
|---|---|---|---|
| 1 | 构建 | 0 error | |
| 2 | 按 SW2 一次 | LED1 翻转 **1 次** | |
| 3 | **按住 SW2 不放** | LED1 **只翻转 1 次，之后不动** | |
| 4 | 按 SW3 | LED2 翻转 1 次（互不干扰） | |
| 5 | 空闲时 `*(unsigned int *)0x40010414` | **`0`**（标志已清） | |
| 6 | `*(unsigned int *)0x080000E0`（向量表第 56 项） | **`0x0800014F`** | |
| 7 | **破坏性实验**：注释掉 `EXTI->PR = ...` 后按一次 | **程序卡死**（CPU 100% 在 ISR 里，主循环饿死） | |

### 8.4 中断链路实测（调试器数据）

| 观察点 | 实测值 | 说明了什么 |
|---|---|---|
| `xpsr` 的 IPSR | `0x38` = **56** | **异常号 = 16 + IRQ号**（16 + 40）；`ipsr==0` 才是主程序 |
| `lr`（在 `EXTI15_10_IRQHandler` 里） | **`0xFFFFFFF9`** | **被硬件异常机制跳进来的**，不是函数调用 |
| 调用堆栈 | `EXTI15_10_IRQHandler` → `<signal handler called>@0xfffffff9` → `Key_Task` → `main` | 硬件压栈把 PC 也压进去了，所以能回溯到主循环 |
| `pc = 0x08000150` | = `EXTI15_10_IRQHandler`(入口 `0x0800014E`) **+2** | 停在 ISR 的第 2 条指令（`BL` 之前） |
| ISR 大小 | **8 字节 = 3 条指令** | "转发"的硬件成本：`PUSH {r7,lr}` / `BL` / `POP {r7,pc}` |
| `msp` | `0x200003F8`（栈顶 `__initial_sp = 0x20000420`） | 差值 **40 = 32（硬件自动压栈）+ 8（`PUSH {r7,lr}`）** |

### 8.5 踩过的坑（本机实录）

| 现象 | 原因 | 教训 |
|---|---|---|
| 链接报 `L6200E: multiply defined` | 变量**定义**写在 `key.h` 里，`key.c` / `main.c` 各生成一份 | **`.h` 只放声明（`extern`），`.c` 放定义** |
| 编译报 `#148: already initialized` | `key.c` 里又定义一遍，而 `key.h` 里那份没删 | 改错要**成对改** |
| `EXTI15_10_IRQn = pins;` 报 lvalue 错 | **枚举常量不能当赋值目标** | 清挂起标志要写 `EXTI->PR` |
| 按住按键 LED 每 100ms 闪一次 | `Key_HasEvent()` 少了"**读走即清**" | 少这一句，设计从"中断驱动"**退化回轮询** |
| `if (s_keyFlag[i] = 1u)` | 用了**赋值**不是比较 | **单点测试抓不到**，必须测"**按住不放**" |

### 8.6 遗留（下一章要还的债）

| # | 内容 | 何时还 |
|---|---|---|
| 1 | `Key_Task()` 里的 `HAL_Delay(20)` 仍是**阻塞**的 | **第 04 章**：改用定时器扫描 + 状态机消抖 |
| 2 | **优先级分组 / 抢占 vs 子优先级**（学习目标 4） | **第 04 章**：有定时器中断后可做优先级对比实验 |
| 3 | 蜂鸣器 | 中控板**无蜂鸣器**；节点板 = PB9；"演奏音阶"需**无源**蜂鸣器 → 留到**第 05 章 PWM** |
