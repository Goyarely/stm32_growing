# 07 · 串口 + 重定向 printf

> 对应课程：07-串口+重定向printf
> 前置要求：06 串口概念
> 建议耗时：4-6 天

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)；学习用板 = **中控板（网关）**）
>
> | 用途 | 笔记示例 | **实板用这个** | 说明 |
> |---|---|---|---|
> | USART1 TX | PA9 | **PA9 → CH340K 的 RXD** | 与笔记一致 |
> | USART1 RX | PA10 | **PA10 ← CH340K 的 TXD** | 与笔记一致 |
> | USART2 | PA2 / PA3 | **PA2 = TXD2 / PA3 = RXD2** | 板上经「NB-IOT/WIFI 通道选择」跳线接 ESP8266 或 NB-IoT，**用之前先看跳线位置** |
> | USART3 | PB10 / PB11 | **PB10 被 LoRa_DIO1 占用、PB11 空闲** | 想用 USART3 只能改用重映射引脚 |
>
> **本板最大优势：串口不用外接任何东西**
> 板上已经焊好 **CH340K**，PA9/PA10 直连。插上 USB 线，电脑就会多一个 COM 口，**`printf` 立刻能看见**——不需要 USB-TTL 模块，也不用杜邦线。
>
> **一个必须知道的冲突**：PA9/PA10 已被 CH340K 占用，**不要把蓝牙模块或 USB-TTL 再并到这两个脚上**（两个输出对着干会打架）。要用第二个串口请走 **USART2（PA2/PA3）**。

---

## 一、学习内容

### 1. USART 硬件资源

| 外设 | 引脚（默认） | 挂载总线 |
|---|---|---|
| USART1 | PA9(TX) / PA10(RX) | APB2（72MHz） |
| USART2 | PA2(TX) / PA3(RX) | APB1（36MHz） |
| USART3 | PB10(TX) / PB11(RX) | APB1（36MHz） |

> **APB1 上的串口时钟是 36MHz**，算波特率时别用 72MHz，否则乱码。

### 2. 三种收发方式对比

| 方式 | 函数 | 特点 | 适用 |
|---|---|---|---|
| 轮询 | `HAL_UART_Transmit` / `HAL_UART_Receive` | 简单，阻塞 CPU | 少量数据发送 |
| 中断 | `HAL_UART_Receive_IT` + 回调 | 非阻塞，逐字节 | 少量不定时接收 |
| DMA | `HAL_UART_Receive_DMA` | 零 CPU 占用，大批量 | 高速/大量数据 |

### 3. 关键寄存器

| 寄存器 | 位 | 作用 |
|---|---|---|
| `SR` | `TXE` | 发送数据寄存器空（可写下一个字节） |
| `SR` | `TC` | 发送完成（最后一个字节真正发完） |
| `SR` | `RXNE` | 接收数据寄存器非空（可读） |
| `SR` | `ORE` | 溢出错误（**未及时读走数据**） |
| `DR` | — | 读写共用一个寄存器 |
| `CR1` | `UE` / `TE` / `RE` / `RXNEIE` | 使能/发送/接收/接收中断 |

### 4. printf 重定向原理
- `printf` 是 C 标准库函数，最终调用底层的字符输出函数
- armcc（ARM Compiler 5）的底层输出函数是 **`fputc(int ch, FILE *f)`**
- 重写 `fputc`，把字符从串口发出去，`printf` 就输出到串口了
- **必须勾选 Use MicroLIB**（EIDE 里的 MicroLIB 选项），否则可能不生效或卡死
- 需要 `#include <stdio.h>` 并声明 `FILE` 结构

### 5. 不定长数据接收方案

**方案一（推荐入门）**：中断逐字节接收 + 空闲中断（IDLE）判断帧结束
**方案二**：DMA + 空闲中断（大批量数据首选）
**方案三**：定时器超时判断（简单但不严谨）

**空闲中断原理**：总线空闲（一个字节时间无数据）时硬件置 `IDLE` 标志，可用于判断"一帧数据接收完毕"。

### 6. 组包替代 printf
`printf` 内部有格式化开销、浮点支持会显著增大体积。数据量大时建议：

```c
char buf[64];
int len = sprintf(buf, "ADC:%d\r\n", value);
HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100);
```

---

## 二、学习目标

- [ ] 能独立配置任意一路 USART，实现收发
- [ ] 能正确重定向 `printf` / `scanf`，并用它做调试输出
- [ ] 能说出轮询、中断、DMA 三种收发方式的适用场景
- [ ] 能实现"不定长数据接收 + 解析"的完整流程
- [ ] 理解 `TXE` 与 `TC` 的区别，知道什么时候该等哪个
- [ ] 能用 RingBuffer（环形缓冲）解耦接收与处理

---

## 三、学习方法

1. **先回环再通信**：把 TX 和 RX 短接，自己发自己收，验证链路。
2. **串口助手辅助**：PC 端用串口助手发送数据，观察 MCU 响应，比打印调试快得多。
3. **单步看 SR**：在接收中断里打断点，观察 `SR` 的 `RXNE` 位从 1 到读 DR 后变 0。
4. **故意制造溢出**：接收中断里加长延时，往快速发数据，观察 `ORE` 溢出错误并学会清除。
5. **对比 printf 与 sprintf**：看编译后的体积变化，理解 `printf` 的代价。

---

## 四、标准示例

### 1. USART1 初始化（115200-8-N-1）

```c
UART_HandleTypeDef huart1;

void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();      /* USART1 在 APB2 */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9 = TX：复用推挽 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_9;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PA10 = RX：浮空输入 或 上拉输入 */
    gpio.Pin  = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}
```

### 2. printf 重定向（armcc / MicroLIB）

```c
#include <stdio.h>

/* ARM Compiler 5：重写 fputc 即可让 printf 走串口 */
int fputc(int ch, FILE *f)
{
    while ((USART1->SR & USART_SR_TXE) == 0);
    USART1->DR = (uint8_t)ch;
    while ((USART1->SR & USART_SR_TC) == 0);
    return ch;
}

/* 如需 scanf 输入，重写 fgetc */
int fgetc(FILE *f)
{
    while ((USART1->SR & USART_SR_RXNE) == 0);
    return (int)(USART1->DR & 0xFF);
}
```

**EIDE 配置**：工程选项里勾选 `Use MicroLIB`（否则可能出现 HardFault 或输出异常）。

```c
printf("SystemCoreClock = %lu\r\n", (uint32_t)SystemCoreClock);
printf("ADC = %d, Volt = %.2fV\r\n", adc_val, adc_val * 3.3f / 4095);
```

### 3. 中断接收单字节 + 回调

```c
uint8_t g_rx_byte;

void UART_StartReceive(void)
{
    HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1);   /* 每次只收 1 字节 */
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        /* 处理 g_rx_byte */
        HAL_UART_Transmit(&huart1, &g_rx_byte, 1, 10);   /* 回显 */

        HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1);     /* 关键：重新挂载接收 */
    }
}
```

> 最常见的 bug：回调里忘记重新调用 `HAL_UART_Receive_IT`，导致**只能收到一个字节**。

### 4. 空闲中断 + 不定长接收（实用方案）

```c
#define RX_BUF_SIZE  128

uint8_t  g_rx_buf[RX_BUF_SIZE];
uint8_t  g_rx_byte;
uint16_t g_rx_len    = 0;
volatile uint8_t g_rx_done = 0;

/* 初始化：开接收中断 + 开空闲中断 */
void UART_Init_Idle(void)
{
    HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        if (g_rx_len < RX_BUF_SIZE - 1) {
            g_rx_buf[g_rx_len++] = g_rx_byte;
        }
        HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1);
    }
}

/* 在 USART1_IRQHandler 里检测空闲中断 */
void USART1_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) &&
        __HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_IDLE))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);   /* 清 IDLE：先读 SR 再读 DR */
        g_rx_buf[g_rx_len] = '\0';
        g_rx_done = 1;                        /* 通知主循环处理 */
    }
    HAL_UART_IRQHandler(&huart1);
}

/* 主循环 */
while (1) {
    if (g_rx_done) {
        g_rx_done = 0;
        printf("RX: %s\r\n", g_rx_buf);       /* 处理整帧 */
        g_rx_len = 0;
    }
}
```

### 5. 清除溢出错误（重要）

```c
/* 如果出现 ORE，串口会卡住不再接收，需要清除 */
if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE)) {
    __HAL_UART_CLEAR_OREFLAG(&huart1);   /* 先读 SR 再读 DR */
}
```

---

## 五、自我检测

1. USART1、USART2 分别挂在哪条总线？时钟频率各是多少？
2. 为什么 USART2 的 BRR 计算要用 36MHz 而不是 72MHz？
3. `TXE` 和 `TC` 有什么区别？发完最后一个字节后应该等哪个？
4. printf 重定向为什么要勾选 MicroLIB？底层调用的是哪个函数？
5. 中断接收为什么只能收一个字节就不动了？怎么解决？
6. 空闲中断是怎么判断一帧结束的？
7. `ORE` 溢出错误是怎么产生的？怎么清除？
8. 轮询、中断、DMA 三种方式各适合什么场景？

---

## 六、常见坑

| 现象 | 原因 |
|---|---|
| 输出全是乱码 | 波特率不匹配；USART2/3 用了 72MHz 算 BRR |
| 只收到第一个字节 | 回调里没重新 `HAL_UART_Receive_IT` |
| 收一阵后卡死不再接收 | `ORE` 溢出错误没清除 |
| 用了 printf 就 HardFault | 没勾选 MicroLIB；或 `fputc` 里死等 |
| printf 无输出 | 重定向函数签名不对；`#include <stdio.h>` 缺失 |
| TX 引脚无波形 | GPIO 没配成 `GPIO_MODE_AF_PP` |
| 单字节接收时丢数据 | 中断优先级太低，或被其他长 ISR 阻塞 |
| 重定向后程序体积暴涨 | printf 浮点格式（`%f`）引入大量代码，改用整型或 sprintf |

---

## 七、拓展练习

1. **命令行终端**：串口输入命令（如 `led on` / `led off` / `pwm 50`），MCU 解析执行并返回结果。
2. **环形缓冲区**：实现 RingBuffer 解耦接收与处理，解决"处理太慢导致丢数据"。
3. **串口 DMA 接收**：用 DMA + 空闲中断重写不定长接收，对比 CPU 占用。
4. **协议帧解析**：定义 `0xAA + LEN + DATA + CRC + 0x55` 格式，实现带校验的通信协议。
5. **调试输出分级**：实现 `LOG_D/I/W/E` 宏，通过宏开关控制输出级别。
6. **对比实验**：统计 10KB 数据分别用轮询、中断、DMA 传输的耗时，用定时器计数。
