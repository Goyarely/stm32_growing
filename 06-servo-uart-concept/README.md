# 06 · 舵机控制 + 串口概念

> 对应课程：06-舵机控制+串口概念
> 前置要求：05 启动模式 + PWM
> 建议耗时：3-5 天

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)；学习用板 = **中控板（网关）**）
>
> | 用途 | 笔记示例 | **实板用这个** | 说明 |
> |---|---|---|---|
> | 舵机 PWM（50Hz） | TIM3_CH1 → **PA6** | **冲突**：PA6 已被 LoRa 占用 | |
> | | | **改用 `PA0 → TIM2_CH1`** | PA0 有引到板上排针（HDR），接线方便（推荐） |
> | | | 备选：`PB8 → TIM4_CH3` | PB8 空闲 |
> | 双舵机 | PA6 / PA7 | 都用 **PA0(TIM2_CH1) + PA1(TIM2_CH2)** | PA0/PA1 都在排针上 |
> | USART1 | PA9 / PA10 | **PA9 / PA10** | 与笔记一致，板载 CH340K |
>
> **舵机供电必须注意**
> 舵机堵转电流可达 1A 以上，**绝对不要从 3.3V 取电**。
> 从板上 **VCC5V** 取，并在舵机电源脚附近**并一个 470uF 以上的电解电容**；否则舵机一动，MCU 就可能复位（这是新手最常见的"莫名重启"）。

---

## 一、学习内容

### 1. 舵机原理（以 SG90 为例）

| 参数 | 值 |
|---|---|
| 供电 | 4.8V ~ 6V（**不要用 3.3V 直接驱动，扭矩不足**） |
| 控制信号 | PWM，周期 20ms（50Hz） |
| 脉宽范围 | 0.5ms ~ 2.5ms |
| 对应角度 | 0° ~ 180° |
| 线序 | 棕=GND、红=VCC、橙=信号 |

**脉宽与角度关系**

```
角度 = (脉宽 - 0.5ms) / (2.5ms - 0.5ms) × 180°
```

| 脉宽 | 角度 |
|---|---|
| 0.5ms | 0° |
| 1.0ms | 45° |
| 1.5ms | 90°（中位） |
| 2.0ms | 135° |
| 2.5ms | 180° |

**PWM 参数计算（72MHz）**：要 50Hz → `(PSC+1) × (ARR+1) = 72000000 / 50 = 1440000`
取 `PSC = 71` → 1MHz 计数（1 个计数 = 1μs），`ARR = 19999` → 20ms。
于是 **CCR 的值直接等于脉宽微秒数**：`CCR = 500` → 0.5ms。

**注意事项**
- 舵机是感性负载，启动电流可达 1A 以上，**必须独立供电**，与 MCU 共地
- 上电瞬间可能抖动，建议初始化先给 1.5ms 中位
- 不要频繁大角度来回摆动，会烧舵机

### 2. 串口通信概念

**基本概念**
- 异步串口（UART）：无时钟线，靠约定的波特率同步
- 波特率：每秒传输的位数，如 9600、115200
- 帧格式：`起始位(1) + 数据位(8) + 校验位(0/1) + 停止位(1)`
- 常用配置简写：**115200-8-N-1**（8 数据位、无校验、1 停止位）
- 全双工（UART）vs 半双工（RS485）

**常见接口对比**

| 接口 | 电平 | 距离 | 特点 |
|---|---|---|---|
| TTL 串口 | 0 ~ 3.3V | 板内 | 直连 MCU 引脚 |
| RS232 | ±12V | 15m | 负逻辑，需 MAX232 转换 |
| RS485 | 差分 | 1200m | 抗干扰强，半双工，需 MAX485 |

**交叉连接规则**：A 的 TX 必须接 B 的 RX，且必须共地。

### 3. 串口相关寄存器（预告）

| 寄存器 | 作用 |
|---|---|
| `SR` | 状态：`TXE`(发送空)、`RXNE`(接收非空)、`TC`(发送完成) |
| `DR` | 数据寄存器，读写都是它 |
| `BRR` | 波特率分频值 |
| `CR1` | 使能位 `UE`、`TE`、`RE`、接收中断 `RXNEIE` |

**波特率公式**：`Baud = fCK / (16 × USARTDIV)`，即 `USARTDIV = fCK / (16 × Baud)`
例：72MHz、115200 → `USARTDIV = 39.0625` → `BRR = 0x271`（HAL 自动计算）

---

## 二、学习目标

- [ ] 能解释舵机脉宽与角度的换算关系并手算 CCR 值
- [ ] 能独立实现"任意角度控制"的舵机驱动函数
- [ ] 理解为什么舵机要用 50Hz 而不是更高频率
- [ ] 能说清 UART 帧结构、波特率含义、TTL/RS232/RS485 的区别
- [ ] 能解释串口为什么必须 TX-RX 交叉连接
- [ ] 知道 115200 波特率下每秒最多传输多少字节（理论值 11520 B/s）

---

## 三、学习方法

1. **先算后写**：在纸上把 50Hz、0.5~2.5ms 到 CCR 值的映射算清楚，再写代码。理解"1 计数 = 1μs"这个设计技巧。
2. **示波器验证**：用示波器量舵机信号线，确认周期 20ms、脉宽随角度变化。
3. **独立供电实验**：先用 MCU 的 3.3V 供电，观察舵机抖动/不动；再接外部 5V，对比效果——理解驱动能力问题。
4. **串口回环测试**：用 USB-TTL 把 PC 与 MCU 相连，先不写代码，用串口助手手动发数据，建立对帧格式的直觉。
5. **波特率不匹配实验**：故意把两边波特率设成不一样，观察乱码——理解波特率同步的重要性。

---

## 四、标准示例

### 1. 舵机驱动（TIM3_CH1 → PA6，50Hz）

```c
static TIM_HandleTypeDef htim3;

#define SERVO_MIN_US   500      /* 0°   */
#define SERVO_MAX_US   2500     /* 180° */
#define SERVO_MID_US   1500     /* 90°  */

static void Servo_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};
    GPIO_InitTypeDef   gpio = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6; gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 1MHz 计数频率 → 1 个计数 = 1us */
    htim3.Instance         = TIM3;
    htim3.Init.Prescaler   = 72 - 1;      /* 72MHz / 72 = 1MHz */
    htim3.Init.Period      = 20000 - 1;   /* 1MHz / 20000 = 50Hz = 20ms */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim3);

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = SERVO_MID_US;   /* 上电先回中位，避免抖动 */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

/* 角度 0~180 → 脉宽 500~2500us */
static void Servo_SetAngle(uint8_t angle)
{
    if (angle > 180) angle = 180;
    uint16_t pulse = SERVO_MIN_US + (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle / 180;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
}
```

### 2. 舵机扫动

```c
while (1) {
    for (uint8_t a = 0; a <= 180; a += 5) { Servo_SetAngle(a); HAL_Delay(20); }
    for (uint8_t a = 180; a > 0;   a -= 5) { Servo_SetAngle(a); HAL_Delay(20); }
}
```

> `HAL_Delay(20)` 是控制转动速度的：舵机每 20ms 移动 5°，180° 约需 0.7s。改这个值就能调快慢。

### 3. 串口发送一个字节（轮询版，理解帧）

```c
/* 最原始的发送：等 TXE 置位后写 DR */
void Uart_SendByte(USART_TypeDef *USARTx, uint8_t byte)
{
    while (!(USARTx->SR & USART_SR_TXE));   /* 等发送数据寄存器空 */
    USARTx->DR = byte;
}

/* 等待发送完成（TC），确保最后一个字节真正发出 */
void Uart_WaitTxDone(USART_TypeDef *USARTx)
{
    while (!(USARTx->SR & USART_SR_TC));
}
```

### 4. 计算练习：波特率寄存器

```c
/* 72MHz / (16 × 115200) = 39.0625 */
/* 整数部分 39 = 0x27，小数 0.0625 × 16 = 1 → BRR = 0x271 */
/* HAL 里由 HAL_UART_Init 自动计算，但要知道原理 */
USART1->BRR = 0x271;
```

---

## 五、自我检测

1. 舵机控制信号为什么要 50Hz？改成 100Hz 会怎样？
2. 写出 90° 对应的 CCR 值（PSC=71、ARR=19999）。
3. 为什么示例里把 PSC 设成 71 而不是 7199？这样设计有什么好处？
4. 舵机为什么要独立供电？共地是否必要？
5. UART 一帧包含哪些位？"115200-8-N-1"是什么意思？
6. 波特率 115200 时，传 10 个字节大约需要多久？
7. 为什么 A 的 TX 要接 B 的 RX？如果 TX 接 TX 会怎样？
8. TTL、RS232、RS485 三者的电平与适用距离分别是多少？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| 舵机抖动、无力 | 供电不足，用 3.3V 直驱或电流不够 |
| 舵机完全不动 | 信号线没接对、PWM 引脚不是复用输出、周期不是 20ms |
| 舵机一直转到极限位置 | 脉宽超出 0.5~2.5ms 范围，被当作无效信号 |
| 上电瞬间抖动 | 没有先输出中位信号 |
| 串口全是乱码 | 波特率不匹配、晶振频率与代码设定不符 |
| 串口收不到数据 | TX/RX 没交叉、没有共地、电平不兼容 |
| 收发都能但偶尔丢字节 | 轮询方式没等 `TC` 就关闭串口/进入低功耗 |

---

## 七、拓展练习

1. **串口控制舵机**：通过串口发送角度值（如 "090"），舵机转过去。
2. **多舵机控制**：用 TIM3 的 CH1/CH2 同时控制两个舵机，做机械臂雏形。
3. **舵机速度可调**：封装 `Servo_SlowMove(target, speed)` 实现平滑转动。
4. **串口回环**：MCU 收到什么就原样发回（echo），验证收发链路。
5. **波特率实验**：同一段数据用 9600 和 115200 分别发送，用秒表测耗时差异。
6. **查手册**：打开 RM0008 的 USART 章节，找到 `SR` 寄存器位定义，标注 `TXE`、`RXNE`、`TC` 三个位的含义与清除方式。
