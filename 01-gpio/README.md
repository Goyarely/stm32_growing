# STM32F103C8 工程模板（HAL 库 / EIDE + VSCode）

基于 STM32F103C8（Cortex-M3，64KB Flash / 20KB SRAM）的裸机工程模板。
已包含完整的 HAL 库、CMSIS、启动文件与构建配置，**拷贝即可编译、烧录**，直接在上面写业务代码。

- 默认例程：PA8 输出低电平点亮 LED，500ms 亮 / 500ms 灭
- 芯片：STM32F103C8（`STM32F103xB`）
- 库：ST HAL（F1 系列）
- 工具链：ARM Compiler 5（armcc）+ Embedded IDE（EIDE）
- 下载器：JLink

---

## 1. 环境要求

| 项目 | 说明 |
|---|---|
| VSCode | 安装 `Embedded IDE (EIDE, cl.eide)` 扩展 |
| 工具链 | ARM Compiler 5（armcc），需在 EIDE 设置中配置其安装路径 |
| 器件 Pack | `STM32F1xx_DFP`；本工程默认离线使用 `.pack/` 目录下的副本，缺失时会尝试联网下载 |
| 下载器 | JLink（EIDE 上传配置里已选 JLink） |
| 调试 | cortex-debug 扩展（可选） |

> 工具链路径、Pack 路径等属于**本机环境**，不写进本文件；换机器后请在 EIDE 设置里重新指定。

---

## 2. 目录结构

```
.
├─ App/                    业务代码
│   └─ main.c              主程序（点灯例程）
├─ Driver/                 板级/中断相关
│   ├─ stm32f1xx_it.c      中断服务函数（SysTick_Handler 调 HAL_IncTick）
│   ├─ stm32f1xx_it.h
│   └─ stm32f1xx_hal_conf.h  HAL 模块裁剪配置（按需开关外设模块）
├─ Arm/                    CMSIS 内核 + 启动
│   ├─ startup_stm32f103xb.s   启动文件（中断向量表，必须参与编译）
│   ├─ system_stm32f1xx.c/.h   系统时钟初始化
│   ├─ core_cm3.h / cmsis_*.h  Cortex-M3 内核头
│   └─ stm32f103xb.h / stm32f1xx.h  器件寄存器定义
├─ STM32_Hal/              ST HAL 库（全量，自包含）
│   ├─ Inc/                头文件
│   └─ Src/                源文件（按需加入工程）
├─ .eide/                  EIDE 工程配置（唯一配置来源）
│   ├─ eide.yml            宏定义、包含路径、工程文件列表、器件型号
│   └─ env.ini             Keil 输出目录等环境变量
├─ .vscode/tasks.json      构建/烧录任务（build、flash、rebuild、clean）
├─ .cmsis/                 CMSIS 头文件（供包含路径使用）
├─ .pack/                  器件 Pack 副本（可删除，删除后首次构建需联网）
└─ ARM32.code-workspace    VSCode 工作区入口（用这个打开工程）
```

---

## 3. 构建与烧录

### 方式一：EIDE 侧边栏（推荐）

1. 用 VSCode 打开 `ARM32.code-workspace`
2. 在 EIDE 侧边栏点「构建」/「烧录」/「构建并烧录」

### 方式二：VSCode 任务

`Ctrl+Shift+P` → `Tasks: Run Task`，可选：

| 任务名 | 作用 |
|---|---|
| `build` | 编译 |
| `rebuild` | 全量重编译 |
| `clean` | 清理 |
| `flash` | 下载到芯片 |
| `build and flash` | 编译并下载 |

**产物位置**：`build/ARM32/`（生成 `.elf` 与 `.hex`；`.bin`、`.s19` 在当前配置中被排除），该目录已被 `.gitignore` 忽略。

### 关于 Keil

本模板**不包含** `uvprojx`（避免两份配置不一致）。如需用 Keil uVision，在 EIDE 里使用「导出 Keil 项目」功能生成即可（器件会自动使用 `STM32F103C8`）。

---

## 4. 关键配置说明（重要，改配置前务必阅读）

这三项是 HAL 工程能编译的最小条件，缺一个就会报错：

| 配置 | 值 | 出错现象 |
|---|---|---|
| 宏定义 | `STM32F103xB`、`USE_HAL_DRIVER` | 少了会报 `Please select first the target STM32F1xx device used in your application` |
| 包含路径 | 必须含 `STM32_Hal/Inc` | 少了会报 `cannot open source input file "stm32f1xx_hal.h"` |
| 启动文件 | `Arm/startup_stm32f103xb.s` 必须在工程文件列表中 | 少了没有中断向量表，程序无法运行 / 链接异常 |

以上均已配置在 `.eide/eide.yml` 中，一般不需要改动。
注意：Keil 工程里曾误用 `STM32F10X_MD`（标准库宏）与 `STM32F101C8`（器件型号）——**本模板已修正为 `STM32F103xB` / `STM32F103C8`**，请勿改回去。

---

## 5. 新增一个外设的步骤

HAL 库的每个外设都是独立的 `.c` 文件，**用到哪个就要把对应的源文件加进工程**，否则链接时会报 `undefined symbol HAL_xxx_Init`。

| 外设 | 需要加入工程的 HAL 源文件 |
|---|---|
| GPIO（已默认包含） | `stm32f1xx_hal_gpio.c` |
| 中断/EXTI | `stm32f1xx_hal_exti.c` |
| 定时器 TIM | `stm32f1xx_hal_tim.c`（+ `stm32f1xx_hal_tim_ex.c` 用高级特性） |
| 串口 USART | `stm32f1xx_hal_uart.c` |
| ADC | `stm32f1xx_hal_adc.c`（+ `stm32f1xx_hal_adc_ex.c`） |
| DMA | `stm32f1xx_hal_dma.c` |
| I2C | `stm32f1xx_hal_i2c.c` |
| SPI | `stm32f1xx_hal_spi.c` |
| RTC | `stm32f1xx_hal_rtc.c`（+ `_rtc_ex.c`） |
| 看门狗 | `stm32f1xx_hal_iwdg.c` / `stm32f1xx_hal_wwdg.c` |
| Flash 读写 | `stm32f1xx_hal_flash.c`（+ `_flash_ex.c`） |
| 电源/低功耗 | `stm32f1xx_hal_pwr.c` |
| 时钟扩展 | `stm32f1xx_hal_rcc_ex.c` |

操作方式（二选一，保持一致即可）：

- **EIDE**：项目树里右键 → 添加文件 → 选择 `STM32_Hal/Src/xxx.c`
- **Keil**：`Add Existing Files to Group`

另外，若使用的外设模块在 `stm32f1xx_hal_conf.h` 中未开启，需要先把对应的 `HAL_xxx_MODULE_ENABLED` 打开（本模板目前是**全模块开启**，一般无需改动）。

---

## 6. 常见问题排查

| 现象 | 原因 |
|---|---|
| `Please select first the target STM32F1xx device` | 宏定义缺 `STM32F103xB` |
| `cannot open source input file "stm32f1xx_hal.h"` | 包含路径缺 `STM32_Hal/Inc` |
| `undefined symbol HAL_xxx_Init` | 该外设的 HAL `.c` 没加进工程 |
| 程序下载后不运行、LED 不闪 | 启动文件没加入工程；或 `HAL_Delay` 失效（`SysTick_Handler` 里没有 `HAL_IncTick()`） |
| 配置改了但构建仍用旧参数 | EIDE 插件缓存了旧配置：重载 VSCode 窗口 / 在 EIDE 界面里改配置（以界面为准） |

---

## 7. 使用本模板新建项目

**方式 A：复制目录**

1. 复制整个工程目录为新项目名
2. **删除 `build/` 目录**（保证是从零构建）
3. 用 VSCode 打开新目录的 `ARM32.code-workspace`
4. 构建 + 烧录，确认灯闪 → 开始写自己的代码

**方式 B：导出为 EIDE 模板（`.ept`）**

1. EIDE 项目树 → 右键项目 → 导出 → 「导出 EIDE 项目模板」
2. 得到 `.ept` 文件，新建项目时选择「本地模板」即可

**方式 C：Git**

将本工程作为 Git 仓库（产物已被 `.gitignore` 忽略），新项目 `git clone` 即可。

---

## 8. 维护约定

- 配置**以 EIDE（`.eide/eide.yml`）为唯一来源**，Keil 工程按需用「导出 Keil 项目」重新生成，不手工维护两份
- `App/main.c` 保持最小可跑（作为环境是否正常的验证基准），具体功能另建文件
- 新增外设的文件、宏、路径变更，请同步更新本文件第 5、6 节
- 复制模板前删除 `build/`、`Objects/` 等产物目录

---

## 9. 默认例程说明（main.c）

`main.c` 刻意保持最小：只做 GPIO 初始化和 LED 翻转，便于作为模板起点（约 40 行）。

| 项目 | 内容 |
|---|---|
| LED 引脚 | PA8，推挽输出、无上下拉、低速；**低电平点亮**（LED 经限流电阻接 3.3V） |
| 闪烁节奏 | 每 500ms 翻转一次（等效亮 500ms / 灭 500ms） |
| 系统时钟 | 复位默认的 **HSI 8MHz**（未开 PLL） |
| 时基 | SysTick 1ms 中断，`HAL_Delay()` 依赖它 |

> 注意：`SystemCoreClock` 变量的默认值是 16MHz，而复位后实际时钟是 8MHz，因此 `main()` 里在 `HAL_Init()` **之前**调用了一次 `SystemCoreClockUpdate()` 把它刷新为真实值；否则 `HAL_Delay()` 会慢一倍。若你切换时钟源（如改用 PLL 到 72MHz），`HAL_RCC_ClockConfig()` 会自动更新该变量。

**换引脚**只需修改 `main.c` 顶部的三个宏：

```c
#define LED_GPIO_PORT          GPIOA
#define LED_GPIO_PIN           GPIO_PIN_8
#define LED_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()
```

**需要明确控制亮/灭**（而不是翻转）时，用：

```c
HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);  /* 低电平 -> 点亮 */
HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);    /* 高电平 -> 熄灭 */
```

**需要 72MHz 主频**时，在 `MX_GPIO_Init` 之前插入时钟配置：HSE 8MHz × PLL9 = 72MHz（`RCC_OscInitStruct` + `HAL_RCC_OscConfig()`，再 `RCC_ClkInitStruct` + `HAL_RCC_ClockConfig(&..., FLASH_LATENCY_2)`），并把 AHB/PCLK2 分频设为 1、PCLK1 设为 2。

