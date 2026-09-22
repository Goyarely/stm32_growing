# 08 · 蓝牙模块 + Debug 调试

> 对应课程：08-蓝牙模块+debug调试
> 前置要求：07 串口 + printf 重定向
> 建议耗时：4-6 天

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)；学习用板 = **中控板（网关）**）
>
> | 用途 | 笔记示例 | **实板用这个** | 说明 |
> |---|---|---|---|
> | 蓝牙模块 | HC-05 接 PA9(TX) / PA10(RX) | **冲突**：PA9/PA10 已焊死给板载 CH340K | |
> | | | **改用 USART2：`PA2 = TX` / `PA3 = RX`** | 蓝牙模块的 RXD 接 PA2、TXD 接 PA3；VCC 接 5V |
> | 调试用 LED | PA8 | **PA8 = LED1** | 与笔记一致 |
> | 调试接口 | — | **SWD（SWCLK / SWDIO）** | 用 JLink 的 SWD 模式；PA13/PA14 归 SWD 用，**不要拿去当普通 GPIO** |
>
> **两个提醒**
> 1. **别把蓝牙并到 PA9/PA10 上**。CH340K 的 TXD 也驱动 PA10，两个输出对着干会打架（轻则乱码，重则通信异常）。走 **USART2** 最干净。
> 2. 板载 **ESP8266_12E** 其实也能干"无线调试"这件事（AT 指令 + 串口透传），**不用买蓝牙模块也能做本章实验**——第 15 章会正式用它。

---

## 一、学习内容

### 1. 蓝牙模块（HC-05 / HC-06）

| 型号 | 角色 | AT 指令 | 说明 |
|---|---|---|---|
| HC-05 | 主/从可切换 | 支持完整 AT | 可作主机主动连接 |
| HC-06 | 仅从机 | 指令较少 | 只能被连接 |

**关键参数**
- 默认波特率：**9600**（注意与程序里的 115200 不同，需要改）
- 默认配对密码：1234 或 0000
- 默认名称：HC-05 / HC-06
- 供电：3.6~6V（模块多带稳压，但信号线是 3.3V 电平）

**接线（与 MCU 串口交叉）**

```
HC-05 TX  →  MCU RX (PA10)
HC-05 RX  →  MCU TX (PA9)
HC-05 VCC →  5V
HC-05 GND →  GND
```

**AT 指令模式进入**：按住模块上的按键（或把 EN/KEY 接高电平）再上电，进入 AT 模式，此时波特率固定为 38400。

**常用 AT 指令**

| 指令 | 作用 |
|---|---|
| `AT` | 测试，返回 OK |
| `AT+NAME=xxx` | 修改名称 |
| `AT+PSWD=1234` | 修改配对密码 |
| `AT+UART=115200,0,0` | 修改波特率（波特率,停止位,校验位） |
| `AT+ROLE=1` | 设为主机（HC-05） |
| `AT+RESET` | 重启生效 |

**透传模式**：正常上电（不进 AT 模式）后，蓝牙模块相当于一根"无线串口线"，MCU 发什么手机收什么，反之亦然。

### 2. 调试手段全景

| 手段 | 适用场景 | 优点 |
|---|---|---|
| 断点 | 逻辑分支验证 | 直观 |
| 条件断点 | 循环中特定条件 | 避免打断几百次 |
| Watch 窗口 | 观察变量变化 | 实时 |
| Memory 窗口 | 看数组、缓冲区 | 看内存真实值 |
| 寄存器窗口 | 确认外设配置 | 验证库函数 |
| Call Stack | 定位调用链 | 查崩溃现场 |
| 串口打印 | 长时间运行监控 | 不影响实时性 |
| 逻辑分析仪 | 时序问题 | 看硬件波形 |

### 3. 断点的原理与影响
- 硬件断点：CPU 有专用寄存器，数量有限（Cortex-M3 一般 6 个），不修改代码
- 软件断点：把指令替换为 BKPT，**修改了 Flash/内存**
- **注意**：在中断里长时间停在断点会导致看门狗复位、通信超时

### 4. HardFault 定位方法

```c
/* 方法一：在 HardFault_Handler 里死循环，然后看调用栈 */
void HardFault_Handler(void)
{
    while (1) { }        /* 在调试器里暂停，看 Call Stack 窗口 */
}

/* 方法二：打印现场寄存器 */
void HardFault_Handler(void)
{
    __asm volatile (
        "tst lr, #4        \n"
        "ite eq            \n"
        "mrseq r0, msp     \n"
        "mrsne r0, psp     \n"
        "b hard_fault_report \n"
    );
}
```

**常见 HardFault 原因**
- 空指针 / 野指针访问
- 数组越界
- 栈溢出（局部大数组）
- 未对齐访问（Cortex-M3 对某些访问要求对齐）
- 除零（M3 默认不触发，需配置）
- 访问未使能时钟的外设寄存器

### 5. 其他调试技巧
- **变量加 `volatile`**：被优化掉的变量在 Watch 里显示 `<optimized out>`
- **降低优化等级**：`-O0` 便于调试，发布时再改 `-Os`
- **LED + 串口双通道**：无法打断点的场景（如中断密集）用 LED 指示
- **printf 打时间戳**：配合 SysTick 记录事件发生时刻

---

## 二、学习目标

- [ ] 能独立配置蓝牙模块（改名字、密码、波特率）并完成与手机通信
- [ ] 能用手机 APP 通过蓝牙控制 LED、读取状态
- [ ] 掌握断点、条件断点、Watch、Memory、Call Stack 的使用
- [ ] 能定位一次 HardFault 并找出原因
- [ ] 理解硬件断点与软件断点的区别，知道何时不该用断点
- [ ] 能说出至少 3 种"无调试器"场景下的调试方法

---

## 三、学习方法

1. **先用串口助手调通蓝牙**：不要一上来就写 MCU 代码，先用 USB-TTL 接蓝牙模块，用串口助手发 AT 指令，确认模块正常。
2. **手机端用现成 APP**：如"蓝牙调试助手"、"SPP 调试助手"，先建立通信，再写 MCU 逻辑。
3. **故意制造 HardFault**：写一段野指针代码，练习用 Call Stack 定位，比看教程印象深十倍。
4. **优化等级对比**：把同一段代码在 `-O0` 和 `-Os` 下调试，看 Watch 窗口的差异，理解编译器优化对调试的影响。
5. **断点 vs 打印对比**：同一个问题分别用断点和串口打印解决，体会各自适用场景。

---

## 四、标准示例

### 1. 蓝牙控制 LED（透传，复用第 07 章接收框架）

```c
/* 蓝牙模块波特率设为 115200 后，与 USART1 一致 */
/* 接收到手机发来的指令并解析 */

static void Process_Cmd(const char *cmd)
{
    if (strcmp(cmd, "LED_ON") == 0) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);   /* 低电平点亮 */
        printf("LED ON\r\n");
    }
    else if (strcmp(cmd, "LED_OFF") == 0) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        printf("LED OFF\r\n");
    }
    else if (strcmp(cmd, "STATUS") == 0) {
        uint8_t on = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_RESET);
        printf("LED:%s\r\n", on ? "ON" : "OFF");
    }
    else {
        printf("ERR: unknown cmd\r\n");
    }
}

/* 在主循环收到整帧后调用 */
while (1) {
    if (g_rx_done) {
        g_rx_done = 0;
        g_rx_buf[g_rx_len] = '\0';
        Process_Cmd((char *)g_rx_buf);
        g_rx_len = 0;
    }
}
```

> 手机串口助手发送时**不要自动加换行**，否则 `strcmp` 会失败。若确实带了 `\r\n`，解析前先去掉。

### 2. 条件断点的实用场景

```c
/* 循环 1000 次，只想在 i == 500 时停下 */
for (uint32_t i = 0; i < 1000; i++) {
    do_something(i);        /* 在此行打断点，条件设为 i == 500 */
}
```

在 EIDE / cortex-debug 的断点属性里填条件表达式即可。

### 3. 用串口打时间戳定位时序问题

```c
volatile uint32_t g_ms = 0;     /* SysTick 或 TIM 中断里自增 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    g_ms++;
}

#define LOG_TS(fmt, ...)  printf("[%lu] " fmt "\r\n", g_ms, ##__VA_ARGS__)

/* 使用 */
LOG_TS("enter ISR");
LOG_TS("adc=%d", adc_val);
```

### 4. HardFault 快速定位模板

```c
void HardFault_Handler(void)
{
    __asm volatile (
        "tst lr, #4         \n"   /* 判断用的是 MSP 还是 PSP */
        "ite eq             \n"
        "mrseq r0, msp      \n"
        "mrsne r0, psp      \n"
        "mov r1, lr         \n"
        "b   HardFault_Report \n"
    );
}

void HardFault_Report(uint32_t *stack, uint32_t lr)
{
    /* stack[0..7] 依次是 R0,R1,R2,R3,R12,LR,PC,xPSR */
    printf("\r\n!! HardFault !!\r\n");
    printf("R0 =0x%08lX R1 =0x%08lX R2 =0x%08lX R3 =0x%08lX\r\n",
           stack[0], stack[1], stack[2], stack[3]);
    printf("R12=0x%08lX LR =0x%08lX PC =0x%08lX PSR=0x%08lX\r\n",
           stack[4], stack[5], stack[6], stack[7]);
    printf("Fault LR = 0x%08lX\r\n", lr);
    while (1);
}
```

**用法**：出错后读 `PC` 值，在 `.map` 文件里搜索这个地址，就能定位到出错的函数。

### 5. Watch 窗口变量显示为 `<optimized out>` 的解决

```c
/* 反例：不加 volatile，-Os 下被优化掉 */
uint32_t g_count = 0;

/* 正例 */
volatile uint32_t g_count = 0;
```

或临时把该文件优化等级调低（EIDE 支持单文件选项）。

---

## 五、自我检测

1. HC-05 和 HC-06 有什么区别？怎么进入 AT 模式？
2. 蓝牙模块默认波特率是多少？为什么常常需要改？
3. 蓝牙模块的 TX/RX 与 MCU 应该怎么接？
4. 硬件断点和软件断点有什么区别？各有什么限制？
5. 在中断里长时间停在断点会发生什么？
6. 变量在 Watch 窗口显示 `<optimized out>` 是什么原因？怎么解决？
7. 说出 5 种常见的 HardFault 原因。
8. 拿到 HardFault 打印的 PC 值后，怎么定位到源码位置？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| 手机连不上蓝牙 | 密码错误、模块没供电、模块已配对其他设备 |
| 能连上但收发乱码 | 模块波特率与程序不一致（默认 9600） |
| 发 AT 无响应 | 没进 AT 模式，或波特率不是 38400 |
| 收到的指令 `strcmp` 总是失败 | 数据带了 `\r\n`，需要先清理 |
| 停在断点后程序复位 | 看门狗超时，或在通信时序中间停太久 |
| HardFault 定位不到 | 优化等级太高，`.map` 对不上，用 `-O0` 重编一次 |
| 程序跑飞但没进 HardFault | 看门狗复位，检查 RCC_CSR 的复位来源标志 |
| 中断里加断点后行为就正常了 | 典型时序/竞争问题，改用串口打印定位 |

---

## 七、拓展练习

1. **蓝牙遥控小车雏形**：手机发方向指令，串口打印并控制 LED 模拟动作。
2. **蓝牙参数配置器**：上电后用串口修改波特率、LED 闪烁频率等参数，参数存进 Flash（预告第 12 章）。
3. **主动制造并定位 HardFault**：写 `*(uint32_t*)0xFFFFFFFF = 1;`，用 Call Stack 找到出错行。
4. **看门狗与断点实验**：开启独立看门狗，在断点处停 3 秒，观察复位，然后读 RCC_CSR 确认复位原因。
5. **调试手段对比报告**：同一个 bug 分别用断点、串口打印、逻辑分析仪解决一次，写一段总结。
6. **优化等级实验**：同一段代码用 `-O0` / `-O1` / `-Os` 编译，对比体积与运行速度（用定时器计数）。
