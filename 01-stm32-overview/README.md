# 01 · STM32 概述 + 开发环境安装 + 工程搭建

> 对应课程：01-STM32概述+开发环境安装+STM32工程搭建
> 前置要求：无
> 建议耗时：3-5 天

---

## 一、学习内容

### 1. STM32F103C8T6 芯片资源
- 内核：ARM Cortex-M3，最高 72MHz
- 存储：64KB Flash / 20KB SRAM
- 封装：LQFP48，37 个可用 GPIO
- 外设：3×USART、2×SPI、2×I2C、2×ADC(12bit)、4×TIM、CAN、USB
- 供电：2.0-3.6V，通常 3.3V

### 2. Cortex-M3 核心概念
- 哈佛结构、Thumb-2 指令集
- 存储器映射：Code / SRAM / Peripheral / External RAM（`0x0000_0000` 起）
- 位带区（Bit-Band）：别名区地址换算
- 寄存器组：R0-R15、xPSR、PRIMASK/FAULTMASK/BASEPRI
- 异常与中断：NVIC、优先级、向量表

### 3. 时钟树初识
- 时钟源：HSI(8MHz) / HSE(8MHz晶振) / LSI(40kHz) / LSE(32.768kHz)
- PLL：HSE×9 = 72MHz
- 总线分频：AHB=72M、APB2=72M、APB1=36M（**APB1 上限 36MHz**）
- 外设时钟默认全部关闭，用哪个开哪个（`RCC->APB2ENR` 等）

### 4. 开发环境搭建
- VSCode + `Embedded IDE (EIDE)` 扩展
- ARM Compiler 5（armcc）工具链路径配置
- 器件 Pack：`STM32F1xx_DFP`
- 下载器：JLink，驱动安装
- 调试扩展：`cortex-debug`

### 5. 工程结构解析（以 `../project_demo` 模板为例）

```
App/          业务代码（main.c）
Driver/       stm32f1xx_it.c 中断服务函数、hal_conf 配置
Arm/          CMSIS 内核 + startup_stm32f103xb.s 启动文件 + system_stm32f1xx.c
STM32_Hal/    HAL 库源码（Inc / Src）
.eide/        工程配置（eide.yml 是唯一配置来源）
build/        编译产物（已被 .gitignore 忽略）
```

编译能跑的三个最小条件：

| 配置 | 值 | 缺了会报什么错 |
|---|---|---|
| 宏定义 | `STM32F103xB`、`USE_HAL_DRIVER` | `Please select first the target STM32F1xx device` |
| 包含路径 | `STM32_Hal/Inc` | `cannot open source input file "stm32f1xx_hal.h"` |
| 启动文件 | `Arm/startup_stm32f103xb.s` | 无中断向量表，程序不运行 |

### 6. 启动文件概览
- 中断向量表：`__Vectors`，第一个是栈顶地址，第二个是 `Reset_Handler`
- `Reset_Handler` → `SystemInit()` → `__main`（C 库初始化）→ `main()`
- `.data` 段从 Flash 搬到 SRAM，`.bss` 段清零
- 弱定义（`WEAK`）：中断服务函数可在 C 里重写覆盖

---

## 二、学习目标

- [ ] 能画出 STM32F103C8T6 的资源框图（内核 + 总线 + 外设）
- [ ] 能说清复位后到进入 `main()` 之间发生了什么
- [ ] 能从零搭建一个可编译、可烧录的工程
- [ ] 能解释 `SystemCoreClock`、`HAL_Init()`、`SystemInit()` 的作用
- [ ] 能把系统时钟从默认 8MHz HSI 切到 72MHz HSE+PLL

---

## 三、学习方法

1. **手绘资源图**：不要只看图，自己画一遍总线拓扑（AHB → APB1/APB2 → 各外设挂在哪条）。
2. **追启动流程**：在 `Reset_Handler` 打断点，用汇编窗口单步，看它如何跳到 `main`。
3. **手动删一次文件**：把启动文件从工程移除，编译烧录，观察现象——理解它为什么必需。
4. **时钟切换实验**：先跑通默认 8MHz，再切 72MHz，用示波器/SysTick 验证延时是否变准。

---

## 四、标准示例

### 1. 最小可运行工程（main.c）

```c
#include "stm32f1xx_hal.h"

#define LED_PORT        GPIOA
#define LED_PIN         GPIO_PIN_8
#define LED_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    LED_CLK_ENABLE();                          /* 先开时钟，否则配置无效 */

    GPIO_InitStruct.Pin   = LED_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);  /* 默认熄灭 */
}

int main(void)
{
    SystemCoreClockUpdate();   /* 必须在 HAL_Init 前，否则 HAL_Delay 慢一倍 */
    HAL_Init();
    MX_GPIO_Init();

    while (1) {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        HAL_Delay(500);
    }
}
```

### 2. 切换到 72MHz

```c
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue      = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL9;   /* 8MHz x 9 = 72MHz */
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;     /* AHB  = 72MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;       /* APB1 = 36MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;       /* APB2 = 72MHz */
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);  /* 72MHz 需 2 个等待周期 */
}
```

### 3. 用寄存器读一次时钟状态

```c
uint32_t sysclk = HAL_RCC_GetSysClockFreq();   /* HAL 版 */
// 寄存器版：读 RCC->CFGR 的 SWS 位判断当前系统时钟源
uint32_t sws = (RCC->CFGR >> 2) & 0x3;         /* 0=HSI 1=HSE 2=PLL */
```

---

## 五、自我检测

1. 说出 STM32F103C8T6 的 Flash、SRAM、主频、GPIO 数量。
2. APB1 和 APB2 的最高频率分别是多少？为什么？
3. 程序从复位到 `main()` 依次经过哪些函数？
4. 为什么外设初始化前必须先使能 RCC 时钟？
5. 把启动文件从工程中删除，会出现什么现象？为什么？
6. 72MHz 下为什么 `FLASH_LATENCY` 需要设置 2？
7. `SystemCoreClockUpdate()` 不加会有什么后果？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| `Please select first the target STM32F1xx device` | 宏定义缺 `STM32F103xB` |
| `cannot open source input file "stm32f1xx_hal.h"` | 包含路径缺 `STM32_Hal/Inc` |
| `undefined symbol HAL_xxx_Init` | 该外设的 HAL `.c` 没加进工程 |
| 下载后不运行、灯不闪 | 启动文件没加入工程；或 `SysTick_Handler` 里没调 `HAL_IncTick()` |
| `HAL_Delay()` 时间慢一倍 | 没调 `SystemCoreClockUpdate()`，默认值 16MHz 与实际 8MHz 不符 |
| 改了配置但构建仍用旧参数 | EIDE 缓存，重载 VSCode 窗口 |
| 切 72MHz 后程序跑飞 | `FLASH_LATENCY` 没改，或 PCLK1 分频不是 2 |

---

## 七、拓展练习

1. 把 `main.c` 的 LED 换成 PB12，验证你是否真的掌握了"改引脚"。
2. 用 `SysTick` 手写一个毫秒延时（不调 `HAL_Delay`），与 `HAL_Delay` 对比精度。
3. 打印 `HAL_RCC_GetHCLKFreq()` / `HAL_RCC_GetPCLK1Freq()` / `HAL_RCC_GetPCLK2Freq()`，验证时钟树配置正确。
   （注意：三个函数都要带 `HAL_RCC_` 前缀；没有串口时可用全局变量存起来，再用调试器看）
4. 在 `Reset_Handler` 单步跟踪，记录 `.data` 段搬运的起止地址。
5. 对比"8MHz HSI"和"72MHz PLL"下的 LED 闪烁周期，用秒表实测偏差。

---

## 八、环境验证记录（已完成）

> 在正式开始学习之前，已通过 `../01-gpio` 和 `../02-exti` 两个工程完成了模板工作流验证。

### 验证清单

- [x] 复制工程模板 `project_demo` 生成独立工程
- [x] VSCode + EIDE 正确识别工程结构
- [x] 编译通过（armcc 工具链路径配置正确）
- [x] 烧录成功（JLink 连接正常）
- [x] 程序实际运行（LED 闪烁 → 说明启动文件、时钟配置、SysTick 时基都正常）
- [x] 第二个工程独立编译烧录成功（排除偶然因素）

### 结论

| 项目 | 状态 |
|---|---|
| 工具链 | 可用 |
| 工程模板 | 可用 |
| 编译链路 | 可用 |
| 烧录 / 运行 | 可用 |

**工作流已就绪，环境不是瓶颈，可以直接进入正式学习。**

### 建议的下一步

1. 把本节「标准示例」的最小工程和 72MHz 时钟配置**手动敲一遍**，确认每一行都理解（不要复制粘贴）。
2. 完成「自我检测」的 7 道题，重点回答第 3 题：**从复位到 `main()` 依次经过哪些函数**。
3. 在 `my_project/` 下新建 `03-tim-base` 之类的工程目录（按 `README.md` 的命名规范），正式开始第 02 章 GPIO 的学习。

---

## 九、我的实测记录（第 01 章学习产出）

> 工程：`01-stm32-overview`（笔记与工程同目录）
> 实机：智慧农业中控板 V1.0 —— STM32F103C8T6 核心板 `LCKFB-DKX-STM32F103C8T6`

### 1. 启动流程单步验证（JLink + cortex-debug）

| 观察点 | 实测值 | 说明 |
|---|---|---|
| `Reset_Handler` 处 `sp` | `0x20000410` | 栈顶 `__initial_sp`，**硬件自动装入** |
| `Reset_Handler` 处 `lr` | `0xFFFFFFFF` | 复位初始值 → **没有任何 BL/BLX "调用"过它** |
| `Reset_Handler` 处 `pc` | `0x08000102` | `Reset_Handler(+2)`，正执行 `BLX R0` |
| `LDR R0,=SystemInit` 后 `r0` | `0x08000E65` | SystemInit 的地址（末位 `1` = Thumb 标记） |
| `BLX R0` 后 `pc` / `lr` | `0x08000E64` / `0x08000105` | 跳进 SystemInit；`lr` = 返回地址（`0x08000104` + Thumb 位） |
| `main` 处 `pc` | `0x08000F18` | 与 `.map` 一致 |
| `main` 处 `lr` | `0x08000165` | 落在 **C 库无名区**（`__scatterload` 之后）→ **`main` 是被 C 库调用的** |

**结论链条**：`复位 → [硬件装载 SP/PC] → Reset_Handler → SystemInit → __main → main`

**`.map` 关键符号**（`build/01-stm32-overview/01-stm32-overview.map`）：

| 符号 | 地址 | 说明 |
|---|---|---|
| `__Vectors` / `__Vectors_End` | `0x08000000` / `0x080000EC` | 向量表 59 项 = `0xEC` 字节 |
| `__main` | `0x080000EC` | 紧跟向量表（scatter 里 `*(InRoot$$Sections)`） |
| `Reset_Handler` | `0x08000100` | |
| `SystemInit` | `0x08000E64` | |
| `main` | `0x08000F18` | |
| `__initial_sp` | `0x20000410` | `.data`（16 字节）之后才是 STACK |

### 2. 时钟树实测（寄存器级验证）

**方法一：`HAL_RCC_GetXXXFreq()` 返回值**

| 变量 | 8MHz（不开 PLL） | 72MHz（HSE+PLL ×9） |
|---|---|---|
| `g_hclk` | `8000000` | `72000000` |
| `g_pclk1` | `8000000` | **`36000000`** |
| `g_pclk2` | `8000000` | `72000000` |

**方法二：直接读寄存器 `RCC->CFGR`（更可信——这是硬件自己的记录）**

```
监视表达式：*(unsigned int *)0x40021004
```

| 场景 | 实测值（十进制） | 换算成十六进制 |
|---|---|---|
| 8MHz（复位默认） | `0` | `0x00000000` |
| **72MHz** | **`1901578`** | **`0x001D040A`** |

**进制转换过程（十进制 → 十六进制）**

```
1901578 ÷ 16 = 118848 ··· 余 10 → A
 118848 ÷ 16 =   7428 ··· 余  0 → 0
   7428 ÷ 16 =    464 ··· 余  4 → 4
    464 ÷ 16 =     29 ··· 余  0 → 0
     29 ÷ 16 =      1 ··· 余 13 → D
      1 ÷ 16 =      0 ··· 余  1 → 1
                                 ↑
                    从下往上读 → 0x1D040A
```

**验算**：`1×16⁵ + 13×16⁴ + 4×16² + 10×16⁰ = 1048576 + 851968 + 1024 + 10 = 1901578` ✅

**逐位验证 `0x001D040A`**

```
0x001D040A = 0000 0000 0001 1101 0000 0100 0000 1010
```

| 位域 | 位 | 掩码运算 | 结果 | 含义 | 判定 |
|---|---|---|---|---|---|
| `SW` | 1:0 | `0x1D040A & 0x3` = `0x2` | `0b10` | 时钟源 = PLL | ✅ |
| `SWS` | 3:2 | `(0x1D040A & 0xC) >> 2` = `0x2` | `0b10` | **硬件确认**：当前 = PLL | ✅ |
| `HPRE` | 7:4 | `(0x1D040A & 0xF0) >> 4` = `0` | `0b0000` | AHB ÷1 → 72MHz | ✅ |
| `PPRE1` | 10:8 | `(0x1D040A & 0x700) >> 8` = `0x4` | `0b100` | **APB1 ÷2 → 36MHz**（未超频） | ✅ |
| `PPRE2` | 13:11 | `(0x1D040A & 0x3800) >> 11` = `0` | `0b000` | APB2 ÷1 → 72MHz | ✅ |
| `ADCPRE` | 15:14 | `(0x1D040A & 0xC000) >> 14` = `0` | `0b00` | ADC ÷2 | — |
| `PLLSRC` | 16 | `(0x1D040A & 0x10000) >> 16` = `1` | `1` | PLL 输入 = HSE | ✅ |
| `PLLXTPRE` | 17 | `0` | `0` | HSE 不二分频 | ✅ |
| `PLLMUL` | 21:18 | `(0x1D040A & 0x3C0000) >> 18` = `0x7` | `0b0111` | **×9** → 8MHz × 9 = 72MHz | ✅ |
| `USBPRE` | 22 | `0` | `0` | — | — |
| `MCO` | 26:24 | `0` | `000` | 不输出时钟 | — |

**从寄存器反推频率（只用这 32 个 bit，不看代码）**

```
PLLSRC = 1    → PLL 输入 = HSE = 8MHz
PLLMUL = 0111 → 编码 7，真实倍数 = 7 + 2 = 9     → 8MHz × 9 = 72MHz
SWS    = 10   → 当前 SYSCLK 就是 PLL             → SYSCLK = 72MHz
HPRE   = 0000 → HCLK  = 72 / 1 = 72MHz
PPRE1  = 0100 → PCLK1 = 72 / 2 = 36MHz
PPRE2  = 0000 → PCLK2 = 72 / 1 = 72MHz
```

> **注意 `PLLMUL` 编码的 `+2` 偏移**：编码 `0000` 对应 ×2，所以**真实倍数 = 编码值 + 2**。
> `0111`(7) + 2 = 9 → ×9。这就是 `SystemCoreClockUpdate()` 里 `pllmull = (pllmull >> 18U) + 2U;` 的由来。

**两套独立方法互相印证**

| | 读寄存器反推 | `HAL_RCC_GetXXXFreq()` | |
|---|---|---|---|
| HCLK | 72MHz | `72000000` | ✅ |
| PCLK1 | 36MHz | `36000000` | ✅ |
| PCLK2 | 72MHz | `72000000` | ✅ |

**结论**：`SWS = 0b10` 是**硬件写**的位（不是软件设置的），它证明时钟**真的切换到了 PLL**。
返回值只是软件推算，而这一位是硬件的"自白"；两者一致 → **时钟树 100% 正确**。

### 3. 速度对比（忙等循环探针）

| 时钟 | 探针 | 现象 |
|---|---|---|
| 8MHz | `for (volatile uint32_t i = 0; i < LOOP_N; i++);` | 基准 |
| 72MHz | 同上，`LOOP_N` 不变 | **明显变快（约 9 倍）** |

> **为什么不用 `HAL_Delay` 做对比？**
> 因为 `HAL_RCC_ClockConfig()` 内部会调用 `HAL_InitTick()` 重配 SysTick 的 `LOAD`
> （`8000 → 72000`），所以 `HAL_Delay` **依然准**，看不出任何变化。
> 必须用"只吃 CPU 周期"的探针。

### 4. 引脚分配（本机实板）

| 功能 | 引脚 | 有效电平 |
|---|---|---|
| LED1 | **PA8** | 低电平点亮 |
| LED2 | **PB15** | 低电平点亮 |
| 按键 SW2 | **PB12** | 10kΩ 上拉，按下 = 低 |
| 按键 SW3 | **PB13** | 10kΩ 上拉，按下 = 低 |

---

## 十、本章遇到的问题与解决

### 1. 按 F5 弹「选择调试器」，程序不启动

**现象**：`F5` 弹出调试器选择列表；`MEMORY` 面板显示 `No debugger attached`；`监视` 面板空白。

**原因**：工作区里**没有 `launch.json`**，VS Code 不知道该启动什么。

**解决**：**不要按 F5**。走 EIDE 的调试入口：项目右键 →「生成调试配置」→ `JLink`；之后 `F5` 才可用。

**排查手法**：检查 `my_project/.vscode/`、`01-stm32-overview/.vscode/` 下有没有 `launch.json`。

---

### 2. 点「Open Disassembly View」报 `reading 'fsPath'`

**现象**：`TypeError: Cannot read properties of undefined (reading 'fsPath')`，提示是 EIDE 插件报的。

**原因**：**EIDE 插件的 bug** —— 反汇编视图没有文件 URI，EIDE 未做空值检查。

**解决**：**绕开，不用反汇编视图**。`Arm/startup_stm32f103xb.s` 本身就是汇编源文件，单步时黄色箭头直接落在汇编行上，效果一样。

**通用经验**：遇到插件报错，先问"**这个功能我是否真的需要**"。需要才去治，不需要就绕开——别让工具问题变成学习阻塞。

---

### 3. 监视面板写 `RCC->CR` 报 `unable to create variable object`

**原因**：`RCC` 是**预处理宏**（`#define RCC ((RCC_TypeDef *)RCC_BASE)`），编译后不存在这个符号。**GDB 只认符号，不认宏。**

**解决**：改用绝对地址

```
*(unsigned int *)0x40021000      /* RCC->CR    */
*(unsigned int *)0x40021004      /* RCC->CFGR  */
*(unsigned int *)0x40022000      /* FLASH->ACR */
```

**通用规则**：`GPIOA`、`TIM2`、`USART1`… 全是宏，**在监视里一律要换成地址**。

**地址算法**：基址 + 结构体成员偏移。例：`RCC_BASE = 0x40021000`，`CR` 是第一个成员 → `+0x00`。

---

### 4. `while(SysTick->CTRL & MASK == 0)` 导致延时变成 0

**现象**：LED 极快闪烁（像常亮偏暗），`my_delay(2000)` 完全没有 2 秒。

**原因**：**运算符优先级陷阱** —— `==` 的优先级**高于** `&`，表达式实际被解析成：

```c
SysTick->CTRL & (SysTick_CTRL_COUNTFLAG_Msk == 0)
/*            = CTRL & 0 = 0  →  while(0) 循环体一次都不执行 */
```

**解决**：给位运算加括号

```c
while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) { }
```

**通用规则**：只要表达式里同时出现「位运算符 `& | ^`」和「比较运算符 `== < >`」，**一律给位运算那一侧加括号**。

---

### 5. `HAL_RCC_GetHCLKFreq()` 等函数报错

**原因**：笔记「拓展练习 3」写成了简写，后两个少了 `HAL_RCC_` 前缀（**本文档已修正**）。

**正确函数名**：

```c
HAL_RCC_GetHCLKFreq();
HAL_RCC_GetPCLK1Freq();
HAL_RCC_GetPCLK2Freq();
```

---

### 6. `SystemCoreClock` 初始值为什么不是 8MHz？

**发现**：`Arm/system_stm32f1xx.c` 第 123 行是 `uint32_t SystemCoreClock = 16000000;`，而 F1 复位实际是 8MHz。

**原因**：这是 ST 跨系列共用模板留下的**历史值**（16MHz 是 F4 系列 HSI 的频率）。
**它注定要被覆盖**——编译期根本无法知道用户会把时钟配成多少。

**ST 自己在源码注释里写了三种同步方式**（第 115~122 行）：

1. `SystemCoreClockUpdate()` —— 手动
2. `HAL_RCC_GetHCLKFreq()` —— 调用即返回
3. **`HAL_RCC_ClockConfig()` —— 自动更新**（配时钟时顺手做了）

**结论**：如果 `main()` 里有 `SystemClock_Config()`，就**不必**手动调 `SystemCoreClockUpdate()`。

---

### 7. 切到 72MHz 后 `HAL_Delay` 时间完全没变

**现象**：加不加 `SystemClock_Config()`，LED 闪烁周期都是 2 秒。

**原因**：**这不是 bug**。`HAL_RCC_ClockConfig()` 内部会调用 `HAL_InitTick()`，用新的 `SystemCoreClock` 重算 `SysTick->LOAD`（`7999 → 71999`），所以 **1ms 还是 1ms**。

**教训**：**"现象没变化" ≠ "代码没生效"** —— 也可能只是观测手段不敏感。
要验证时钟真的变了，必须用"只吃 CPU 周期"的探针（忙等循环）。

---

### 8. 变量面板里 `__initial_sp` 显示 `data variable, no debug info`

**原因**：`__initial_sp`、`__Vectors` 是**链接器生成的汇编符号**，没有 DWARF 调试信息。

**解决**：改用 `.map` 文件查地址，或直接读内存：

```
MEMORY 面板输入 0x08000000        → 前 4 字节（小端序）= 初始 SP
监视里写 *(unsigned int *)0x08000000
```

**读出的值**：`00 04 00 20` → 小端序 → `0x20000410` = `__initial_sp` ✅

---

### 9. 调用堆栈只显示 `main` 一帧

**原因**：C 库那几层**没有调试信息、没有帧指针**，调试器无法回溯。

**解决**：判断"谁调用我"要看 **`lr`** 寄存器，不要依赖调用堆栈。

---

### 10. 怎么查"某个地址属于哪个函数"？

**方法**：在 `.map` 里找**不超过该地址的最大符号**，看它的「地址 + 大小」是否覆盖目标地址。

```
目标 0x08000165
→ 找 ≤ 0x08000165 的最大符号：__scatterload  0x08000149（大小 28 = 0x1C）
→ 覆盖 0x08000148 ~ 0x08000163，不含 0x08000165
→ 下一个符号 BusFault_Handler 0x0800016D
→ 所以 0x08000164 ~ 0x0800016B 是【无名区】（C 库的 `.ARM.Collect$$$$` 小节）
```

**更快的办法**：调试控制台敲

```
info symbol 0x08000165
```

---

## 十一、本章小结

| 学习目标 | 状态 |
|---|---|
| 画出 F103C8T6 资源框图（内核 + 总线 + 外设） | ✅ 已画（**允许查资料，不必背**） |
| 说清复位 → `main()` 之间发生了什么 | ✅ 单步 + 寄存器验证 |
| 从零搭建可编译、可烧录的工程 | ✅ |
| 解释 `SystemInit` / `SystemCoreClock` / `HAL_Init` | ✅ |
| 把系统时钟从 8MHz HSI 切到 72MHz HSE+PLL | ✅ 实测 |

**拓展练习完成情况**

| # | 练习 | 状态 |
|---|---|---|
| 1 | 换引脚（PA8 → PB15，点亮 LED2） | ✅ |
| 2 | 手写 SysTick 毫秒延时（不调 `HAL_Delay`） | ✅ |
| 3 | 打印 HCLK / PCLK1 / PCLK2 | ✅ 用调试器完成（`printf` 版留到第 07 章） |
| 4 | `Reset_Handler` 单步跟踪 | ✅ |
| 5 | 8MHz vs 72MHz 闪烁对比 | ✅ |

**遗留待办（不阻塞第 02 章）**

- [ ] 给 `HAL_RCC_OscConfig()` 加返回值检查（`HAL_TIMEOUT` = HSE 起振失败）
- [ ] 第 07 章有串口后，用 `printf` 重做拓展练习 3
