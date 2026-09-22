# STM32 学习工作区（my_project）

这里是你自己的学习仓库：每个练习工程对应一个外设，独立可编译、可烧录。
模板母本在 `../project_demo`，**只用来复制，不要直接在上面做实验**。

---

## 一、目录结构

```
d:/02_Project_Source/STM32_0914/
├─ project_demo/                    模板母本（干净、未动过，不要改）
│   └─ ARM32.code-workspace         单独打开模板时用
└─ my_project/                      学习仓库（git 管理）
    ├─ stm32_learn.code-workspace   ★唯一入口：打开这一个窗口
    ├─ README.md                    本文件
    ├─ .gitignore
    │
    ├─ 00-overview.md               ★学习总览（路线图 / 方法论 / 进度表）
    ├─ 01-stm32-overview/           课程笔记：按【课程章节】编号
    ├─ 02-gpio/                     每个目录一份 README.md
    ├─  ...
    ├─ 16-17-freertos/
    │
    ├─ 01-gpio/                     实验工程：按【练习顺序】编号
    │   ├─ App/main.c               写代码的地方
    │   ├─ Driver/ Arm/ STM32_Hal/  库与启动（一般不用动）
    │   ├─ .eide/eide.yml           工程配置（宏、包含路径、文件列表）
    │   └─ 01-gpio.code-workspace   单独打开该工程时用（平时用不到）
    └─ 02-exti/ ...                 后续实验工程
```

### 笔记目录（课程章节）

除了实验工程，本仓库里还有一套**跟课程走的学习笔记**：

| 路径 | 说明 |
|---|---|
| `00-overview.md` | **从这份开始看**：完整路线、阶段划分、通用方法论、进度勾选表 |
| `01-stm32-overview/` ~ `16-17-freertos/` | 16 个课程章节笔记，每个目录一份 `README.md` |

每份笔记固定包含 7 个板块：学习内容 · 学习目标 · 学习方法 · 标准示例 · 自我检测 · 常见坑 · 拓展练习。
内容对齐本机环境（**HAL 库 + EIDE + STM32F103C8T6**），示例代码可直接用。

### ⚠️ 两套编号不要混淆

| 类型 | 编号依据 | 举例 |
|---|---|---|
| **笔记目录** | 课程章节 | `02-gpio/`、`03-key-interrupt-buzzer/` |
| **实验工程** | 练习顺序 | `01-gpio/`、`02-exti/`、`03-tim-base/` |

已有的 `01-gpio` 工程对应的是第 **02** 章笔记，**编号对不上是正常的**，看名字而不是看数字。

### 推荐用法：笔记与工程同目录

学某一章时，直接在该章笔记目录里复制模板建工程，这样只有一套编号：

```
02-gpio/
├── README.md              ← 本章笔记
├── App/ Driver/ Arm/ ...  ← 从 ../project_demo 复制来的工程
└── 02-gpio.code-workspace
```

复制后按第三节的 3 步改名（`name` / `targets` 键名 / `uid` / `env.ini` 节名）。

> 已有的 `01-gpio` / `02-exti` 保留作为"环境可用"的参照即可，不必迁移。

---

## 二、打开方式（唯一入口）

**永远只打开这一个：**

```
my_project/stm32_learn.code-workspace
```

- 同一个窗口一直用下去，聊天记录 / 上下文才连续
- ❌ 不要双击子工程里的 `xxx.code-workspace`
- ❌ 不要用「文件 → 打开文件夹」打开子工程目录
- ❌ 不要关闭窗口重开

这个 workspace **只挂两个 folder**，整个 `my_project` 都在里面，笔记和工程都能在侧边栏看到：

```
stm32_learn (工作区)
├─ template     → ../project_demo     模板母本（只复制，不改）
└─ my_project   → ./                 全部笔记 + 全部实验工程
    ├─ 00-overview.md                学习总览
    ├─ 01-stm32-overview/ ...        课程笔记
    └─ 01-gpio/ 02-exti/ ...         实验工程
```

> ❌ 不要再往 workspace 里添加子目录（`Add Folder to Workspace`）。
> `my_project` 已经整体挂载，再加子目录会让同一个工程被识别两次，导致 EIDE 工程列表出现重复项。

---

## 三、新建一个实验工程（3 步）

**① 复制模板**（在 VSCode 终端里执行，`Ctrl+`` 打开终端）

```powershell
$r = "d:/02_Project_Source/STM32_0914"
Copy-Item -Recurse "$r/project_demo" "$r/my_project/02-exti"
Remove-Item -Recurse -Force "$r/my_project/02-exti/build" -ErrorAction SilentlyContinue
```

**② 改名字**（4 处必须完全一致，否则 EIDE 里会重名 / 环境变量不生效）

以 `02-exti` 为例：

| 文件 | 改什么 |
|---|---|
| `.eide/eide.yml` 第 2 行 | `name: ARM32` → `name: 02-exti` |
| `.eide/eide.yml` 第 36 行 | `  ARM32:` → `  02-exti:`（注意保留两个空格缩进） |
| `.eide/eide.yml` 第 34 行 | `uid: ""` → 随便填一个唯一的 32 位十六进制 |
| `.eide/env.ini` 第 1 行 | `[ARM32]` → `[02-exti]` |

**③ 在 EIDE 里启用**

EIDE 侧边栏 → 项目上右键 →「刷新」→「设为活动」

> 不需要「加进工作区」这一步：`my_project` 整体已在 workspace 中，EIDE 刷新后会自动发现新工程。

---

## 四、构建 / 烧录（运行方法）

1. 左侧活动栏点「**EIDE 项目**」图标
2. **确认活动项目**是你这次要烧的工程（多工程时最容易搞错的一步）
   - 工程列表里每个含 `.eide/` 的目录都会出现一条，**认名字，不要认顺序**
3. 右键该项目：

| 操作 | 什么时候用 |
|---|---|
| **构建** | 平时改代码后 |
| **重新构建** | 改了 `eide.yml`（宏、包含路径、文件列表）之后 |
| **清理** | 想清空 `build/` 重新来过 |
| **构建，烧录** | 改完直接下载到板子 |

⚠️ 不要点「构建所有项目」——那会把所有工程都编一遍。

产物位置：`<工程目录>/build/<target名>/`（如 `01-gpio/build/01-gpio/01-gpio.hex`）。

---

## 五、EIDE 侧操作速查

| 想做什么 | 操作 |
|---|---|
| 加 HAL 源文件（学新外设时） | 项目树 → 分组右键 →「添加文件」（从 `STM32_Hal/Src/` 选） |
| 改宏 / 包含路径 | 项目右键 →「修改编译配置」 |
| 看实际构建参数（排错） | 项目右键 →「显示构建参数」 |
| 改芯片 | 项目右键 →「设置芯片」 |
| 生成调试配置 | 项目右键 →「生成调试配置」→ JLink |
| 导出 Keil 工程 | 项目右键 →「导出」→「Keil 项目」 |

**改了 `eide.yml` 后必须：刷新 → 重新构建**，否则 EIDE 还在用旧配置。

---

## 六、命名规范

- 目录名前缀两位数字：`01-gpio`、`02-exti`、`03-tim-base`、`04-tim-pwm`、`05-usart`、`06-adc`、`07-dma`
- **一个工程只学一个外设**，跑通就 `git commit`
- 四处名字必须一致：目录名 = `name` = `targets` 键名 = `env.ini` 节名

---

## 七、学习进度

| 序号 | 工程 | 内容 | 状态 |
|---|---|---|---|
| 01 | `01-stm32-overview` | 第 01 章：工程搭建、时钟体系（8MHz→72MHz）、启动流程单步验证、手写 SysTick 延时 | **已完成** |
| — | `01-gpio` / `02-exti` | 模板工作流验证（非学习产出） | 已冻结，仅作参照 |
| 02 | `02-gpio` | 第 02 章：GPIO 8 种模式、HAL↔寄存器对照、BSRR、按键消抖 | **已完成** |
| 03 | `03-tim-base` | 定时器时基、定时中断、PSC/ARR 计算 | 待开始 |
| 04 | `04-tim-pwm` | PWM 输出（呼吸灯）、占空比/频率计算 | 待开始 |
| 05 | `05-usart` | 串口收发、环形缓冲、printf 重定向 | 待开始 |
| 06 | `06-adc` | ADC 单次/连续采样、内部温度传感器 | 待开始 |
| 07 | `07-dma` | DMA + USART/ADC，解放 CPU | 待开始 |

### 实板硬件（唯一事实来源：[`硬件说明.md`](硬件说明.md)）

| 板子 | 定位 | 关键引脚 |
|---|---|---|
| **中控板（网关）** | **学习主用板** | LED1=PA8、LED2=PB15、SW2=PB12、SW3=PB13；板载 CH340K(PA9/PA10)、OLED(PB6/PB7)、LoRa(PA4~PA7 + PB0/PB1/PB10)、ESP8266/NB-IoT(PA2/PA3) |
| **节点板（终端）** | **模拟量 / 执行器实验** | 光敏=PA0、土壤=PA1、DHT22=PC15、继电器=PC14、蜂鸣器=PB9、电机=PB8；LED2=PA8、LED3=PB15、SW1=PB12、SW2=PB13 |

> 第 03~16 章的 `README.md` 开头都加了「**本章实板引脚**」小节，笔记示例与实板冲突的地方已给出替代方案。
> **涉及模拟量的第 11 章起，建议换用节点板**（PA0/PA1 上有现成的光敏和土壤湿度）。

---

## 八、踩坑记录

**工程配置三要素（缺一个就编译不过）**

| 配置 | 值 | 出错现象 |
|---|---|---|
| 宏定义 | `STM32F103xB`、`USE_HAL_DRIVER` | `Please select first the target STM32F1xx device` |
| 包含路径 | 必须含 `STM32_Hal/Inc` | `cannot open source input file "stm32f1xx_hal.h"` |
| 启动文件 | `Arm/startup_stm32f103xb.s` 必须在工程文件列表里 | 无中断向量表，程序跑不起来 |

**其他**

- `HAL_Delay` 不准一倍 → `main()` 里少了 `SystemCoreClockUpdate()`（默认变量是 16MHz，实际复位是 8MHz）
- 链接报 `undefined symbol HAL_xxx_Init` → 该外设的 HAL `.c` 没加进工程
- 改了 `eide.yml` 没生效 → 没在 EIDE 里「刷新」+「重新构建」
- **手工改 `eide.yml` 白改（会被覆盖回去）** → EIDE 以内存中的项目配置为准，一旦保存就把文件整个重写。
  必须按这个顺序：**EIDE 里「关闭项目」→ 手工改 `eide.yml` → EIDE 里「打开项目」**。
  平时改配置尽量都在 EIDE 界面里改。
- 目录里出现 `RTE_Components.h` / `build/` → 构建产物，可删；`.gitignore` 已忽略

**第 01 章实战踩坑（完整版见 [`排错手册.md`](排错手册.md)）**

| 现象 | 原因 / 解决 |
|---|---|
| 按 `F5` 弹「选择调试器」，`MEMORY` 显示 `No debugger attached` | 工作区没有 `launch.json`。**别按 F5**，用 EIDE 项目右键 → `生成调试配置` → `JLink` |
| 点「Open Disassembly View」报 `reading 'fsPath'` | EIDE 插件 bug。**绕开**：`startup_*.s` 本身就是汇编源文件，直接在编辑器里单步 |
| 监视里写 `RCC->CR` 报 `unable to create variable object` | `RCC` 是宏，GDB 不认宏。改用 `*(unsigned int *)0x40021000` |
| `while(SysTick->CTRL & MASK == 0)` 延时为 0 | `==` 优先级**高于** `&`。必须写成 `((CTRL & MASK) == 0)` |
| `HAL_RCC_GetHCLKFreq` 报错 | 少了 `HAL_RCC_` 前缀（`GetPCLK1Freq` 也要带） |
| 切到 72MHz 后 `HAL_Delay` 时间没变 | **不是 bug**：`HAL_RCC_ClockConfig` 会调 `HAL_InitTick` 重配 SysTick。要验证速度得用忙等循环探针 |
