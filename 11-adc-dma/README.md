# 11 · ADC + 光敏电阻实验 + DMA

> 对应课程：11-ADC+光敏电阻试验+DMA
> 前置要求：10 最小系统 + 看门狗 + RTC
> 建议耗时：5-7 天

---

## 一、学习内容

### 1. STM32F103 的 ADC 资源

| 项目 | 参数 |
|---|---|
| 分辨率 | 12 位（0 ~ 4095） |
| 通道数 | ADC1/ADC2 各 16 个外部通道 + 内部通道（温度、Vref） |
| 转换时间 | 最快 1μs（14MHz ADCCLK、1.5 周期采样） |
| 参考电压 | `VREF+` = VDDA（通常 3.3V） |
| 供电 | VDDA（需 LC 滤波） |

**引脚分配（部分）**

| 通道 | 引脚 | 通道 | 引脚 |
|---|---|---|---|
| ADC_IN0 | PA0 | ADC_IN4 | PA4 |
| ADC_IN1 | PA1 | ADC_IN5 | PA5 |
| ADC_IN2 | PA2 | ADC_IN6 | PA6 |
| ADC_IN3 | PA3 | ADC_IN7 | PA7 |

> **注意**：ADC 引脚必须配成 `GPIO_MODE_ANALOG`（模拟输入），**关闭数字通道**，否则读数不准。

### 2. 关键概念

| 概念 | 说明 |
|---|---|
| 规则通道 | 常规转换，最多 16 个，按 `SQR` 寄存器顺序转换 |
| 注入通道 | 可打断规则通道，最多 4 个，用于紧急采样 |
| 扫描模式 | 多个通道依次自动转换（多通道必须开） |
| 连续转换 | 一次触发后不断转换 |
| 单次转换 | 触发一次转换一次 |
| 外部触发 | 可由定时器触发（配合 DMA 做定时采样） |
| 采样时间 | 越长越准（高阻抗信号源需更长） |

**转换时间公式**

```
Tconv = (采样周期数 + 12.5) / ADCCLK
例：采样 55.5 周期、ADCCLK = 14MHz → (55.5 + 12.5) / 14M = 4.86us
```

**电压换算**

```
Voltage = ADC_Value × 3.3 / 4095
```

### 3. 光敏电阻模块
- 分压电路：光敏电阻 + 固定电阻串联，中点接 ADC 引脚
- 光照越强 → 光敏电阻阻值越小 → 分压点电压变化
- 多数模块输出为模拟电压，直接接 ADC 引脚
- 也可用 LM393 比较器输出数字量（只能判断亮暗，不能量化）

### 4. DMA（直接存储器访问）

**核心价值**：数据搬运不经过 CPU，ADC 转换完直接写入内存数组，CPU 能做别的事。

**STM32F103 DMA 资源**

| DMA | 通道 | 常见外设 |
|---|---|---|
| DMA1 | 通道 1 | ADC1、TIM2_CH3、TIM4_CH1 |
| DMA1 | 通道 2 | USART3_TX、SPI1_RX |
| DMA1 | 通道 4 | USART1_TX、SPI1_TX、I2C1_TX |
| DMA1 | 通道 5 | USART1_RX、SPI1_RX、TIM2_CH1 |
| DMA1 | 通道 6/7 | USART2、I2C1 |
| DMA2 | 通道 1 | ADC3、TIM8、SPI1_RX |
| DMA2 | 通道 3 | USART1_RX |
| DMA2 | 通道 5 | ADC1、USART1_RX |

**关键概念**

| 概念 | 说明 |
|---|---|
| 传输方向 | 外设→内存 / 内存→外设 / 内存→内存 |
| 循环模式 | Circular：传完自动重头开始，**ADC 连续采样必须用这个** |
| 外设/内存地址自增 | ADC 用外设地址不自增、内存地址自增 |
| 数据宽度 | 外设与内存可分别设（ADC 是 16 位） |
| 传输完成中断 | `TC`，半传输 `HT` 中断 |

**为什么 ADC 必须配 DMA？** 多通道扫描时若不用 DMA，`DR` 寄存器会被后一个通道覆盖，导致读数全错。

---

## 二、学习目标

- [ ] 能独立配置单通道 ADC 并读取电压值，误差 < 5%
- [ ] 能配置多通道扫描 + DMA，实现 4 通道同时采样
- [ ] 能解释规则通道与注入通道的区别
- [ ] 能说清为什么 ADC 引脚必须配成模拟输入
- [ ] 能独立配置 DMA，理解循环模式与非循环模式的区别
- [ ] 能用定时器触发 + DMA 实现固定采样率的采集

---

## 三、学习方法

1. **先单通道后多通道**：单通道轮询跑通，再加 DMA 做多通道，否则问题混在一起很难查。
2. **用已知电压验证**：先用 3.3V 和 GND 测，读数应该接近 4095 和 0，确认量程对。
3. **单步看 DR**：在转换完成处打断点，观察 `ADC1->DR` 的值。
4. **对比 DMA 开与关**：多通道时关掉 DMA，观察读到的值是否都一样（被覆盖），理解 DMA 的必要性。
5. **示波器验证采样率**：定时器触发配置后用示波器看转换频率是否与设定一致。

---

## 四、标准示例

### 1. 单通道 ADC（PA0，轮询）

```c
static ADC_HandleTypeDef hadc1;

static void ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA0 → 模拟输入（必须） */
    gpio.Pin  = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    hadc1.Instance                   = ADC1;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;      /* 单通道 */
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    HAL_ADC_Init(&hadc1);

    sConfig.Channel      = ADC_CHANNEL_0;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;   /* 光敏电阻阻抗较高，用长采样 */
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/* 读取一次电压 */
static float ADC_GetVoltage(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint32_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return (float)val * 3.3f / 4095.0f;
}
```

### 2. 多通道 ADC + DMA（4 通道循环采集）

```c
#define ADC_CH_NUM   4
uint16_t g_adc_buf[ADC_CH_NUM];      /* DMA 目标缓冲区 */

static void ADC1_DMA_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    GPIO_InitTypeDef gpio = {0};
    DMA_HandleTypeDef hdma_adc1 = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA0~PA3 → 模拟输入 */
    gpio.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* ---------- DMA 配置 ---------- */
    hdma_adc1.Instance                 = DMA1_Channel1;         /* ADC1 固定用 DMA1_CH1 */
    hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;      /* 外设地址不自增 */
    hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;       /* 内存地址自增 */
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode                = DMA_CIRCULAR;          /* 循环模式 */
    hdma_adc1.Init.Priority            = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&hdma_adc1);
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    /* DMA 中断（可选） */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    /* ---------- ADC 配置 ---------- */
    hadc1.Instance                   = ADC1;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;         /* 多通道必须开扫描 */
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = ADC_CH_NUM;
    HAL_ADC_Init(&hadc1);

    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;

    sConfig.Channel = ADC_CHANNEL_0; sConfig.Rank = ADC_REGULAR_RANK_1;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_1; sConfig.Rank = ADC_REGULAR_RANK_2;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_2; sConfig.Rank = ADC_REGULAR_RANK_3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_3; sConfig.Rank = ADC_REGULAR_RANK_4;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    /* 校准（重要，能显著提升精度） */
    HAL_ADCEx_Calibration_Start(&hadc1);

    /* 启动：ADC + DMA 一起启动，之后硬件自动搬运 */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc_buf, ADC_CH_NUM);
}

/* DMA 传输完成回调（循环模式下会周期触发） */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    /* g_adc_buf[0..3] 已更新为最新一轮采样值 */
}

/* 主循环使用 */
while (1) {
    for (int i = 0; i < ADC_CH_NUM; i++) {
        float v = g_adc_buf[i] * 3.3f / 4095.0f;
        printf("CH%d: %4d (%.2fV)\r\n", i, g_adc_buf[i], v);
    }
    HAL_Delay(500);
}
```

### 3. 定时器触发 ADC + DMA（固定采样率）

```c
/* 让 TIM3 的 TRGO 事件触发 ADC，实现精准采样率 */
static void ADC_TrigConfig(void)
{
    /* TIM3 配置：1kHz 更新事件 */
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;   /* 更新事件作为触发 */
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);

    /* ADC 外部触发源改为 TIM3_TRGO */
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
    HAL_ADC_Init(&hadc1);
    /* 重新配置通道 + 校准 + 启动 DMA */
}
```

### 4. 寄存器版：启动一次转换并读值

```c
ADC1->CR2 |= ADC_CR2_ADON;         /* 第一次写：从掉电唤醒 */
for (volatile int i = 0; i < 1000; i++);

ADC1->CR2 |= ADC_CR2_ADON;         /* 再次写：启动转换 */
while (!(ADC1->SR & ADC_SR_EOC));  /* 等转换完成 */
uint16_t val = ADC1->DR;           /* 读数据寄存器（读 DR 会自动清 EOC） */
```

---

## 五、自我检测

1. 12 位 ADC 的最大值是多少？3.3V 参考下 1 个 LSB 对应多少电压？
2. ADC 引脚为什么必须配成模拟输入？配成浮空输入会怎样？
3. 规则通道和注入通道有什么区别？
4. 多通道扫描为什么必须配 DMA？不用会怎样？
5. 采样时间设长有什么好处？有什么代价？
6. DMA 的循环模式与非循环模式有什么区别？
7. DMA 配置里为什么外设地址不自增、内存地址要自增？
8. ADC1 为什么必须用 DMA1 通道 1？
9. 校准（Calibration）的作用是什么？不做会怎样？
10. 如何用定时器实现精准的采样率？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| 读到的值一直是 4095 或 0 | 引脚没配成模拟输入；引脚接到了错误的通道 |
| 多通道读到的值全一样 | 没开扫描模式；没用 DMA（DR 被覆盖） |
| 读数抖动大 | 采样时间太短；VDDA 没滤波；信号源阻抗高 |
| DMA 只搬一次就停 | 用了 `DMA_NORMAL` 而不是 `DMA_CIRCULAR` |
| ADC 完全没有值 | 忘记 `HAL_ADCEx_Calibration_Start()`；没使能 ADC 时钟 |
| 定时器触发不生效 | TRGO 配置不对；ADC 触发源没改 |
| 读数比实际值偏小 | VREF+ 与 VDDA 未接好；忽略了输入阻抗分压 |
| 采样速率上不去 | ADCCLK 分频过大（ADC 时钟上限 14MHz） |

---

## 七、拓展练习

1. **电压表**：4 通道分别接不同电压，串口输出实时值，精度对比万用表。
2. **光控灯**：光敏电阻 ADC 值低于阈值时自动点亮 LED，实现自动夜灯。
3. **滑动平均滤波**：对 ADC 值做 10 点滑动平均，对比滤波前后的抖动。
4. **ADC + OLED 波形**：把 ADC 采集的数据用 OLED 画成波形（预告第 13 章）。
5. **音频采样**：用定时器触发 + ADC + DMA 采集麦克风信号，存放在数组里。
6. **内温读取**：读取 ADC 内部温度传感器通道，计算芯片温度（需查手册的公式与校准值）。
7. **过采样提精度**：连续采样 16 次求平均，实现等效 14 位分辨率。
