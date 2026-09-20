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
3. 打印 `HAL_RCC_GetHCLKFreq()` / `GetPCLK1Freq()` / `GetPCLK2Freq()`，验证时钟树配置正确。
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
