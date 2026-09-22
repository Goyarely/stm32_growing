# 16-17 · FreeRTOS 实时操作系统

> 对应课程：16-17-FreeRTOS
> 前置要求：01-15 全部
> 建议耗时：4-6 周

> **本章实板引脚**（完整引脚表见 [`../硬件说明.md`](../硬件说明.md)；学习用板 = **中控板（网关）**）
>
> | 用途 | 笔记示例 | **实板用这个** | 说明 |
> |---|---|---|---|
> | LED 任务 | PA8 | **PA8 = LED1**（PB15 = LED2） | 低电平点亮 |
> | 按键 ISR（FromISR 示例） | EXTI0 | **EXTI12 / EXTI13 → `EXTI15_10_IRQHandler`** | 两个按键共用一个中断向量 |
> | 串口打印任务 | USART1 | **PA9 / PA10** | 板载 CH340K，插 USB 即用 |
> | DHT11 | PB5 | 中控板外接 **PB5**；节点板板载 **DHT22 = PC15** | |
> | OLED | PB6 / PB7 | **板载** | |
> | AT24C02 | PB6 / PB7 同总线 | 外接并联 | |
> | 综合项目（第 13 章项目 RTOS 化） | 涉及光敏 / 土壤 | **需节点板**（PA0 光敏、PA1 土壤板载） | 或在中控板排针上外接 |
>
> **FreeRTOS 特有的三个坑（与引脚无关，但一定会撞上）**
> 1. **`HAL_Delay()` 别在任务里用** → 换成 `vTaskDelay(pdMS_TO_TICKS(ms))`，否则整个调度器被阻塞。
> 2. **中断里必须用 `...FromISR()` 版本 API**（如 `xQueueSendFromISR`），普通版本会破坏调度器状态。
> 3. **SysTick 被 FreeRTOS 抢走了**：它用自己的时基。所以 **HAL 的时基要重定向到别的定时器**（重写 `HAL_InitTick()` 用 TIMx），否则 `HAL_Delay`/超时判断全乱。

---

## 一、为什么要用 RTOS

### 裸机开发的痛点

```c
/* 典型的裸机主循环 */
while (1) {
    Key_Scan();          /* 必须快速返回 */
    HAL_Delay(500);      /* 这里阻塞了 500ms，其他任务全被拖住 */
    Read_Sensor();       /* 传感器读取需要 200ms */
    Send_Data();         /* 发送可能超时 1s */
}
```

**问题**
- 任何一处阻塞都会拖慢所有功能
- 功能多了以后时序纠缠，改一处崩三处
- 无法处理"紧急事件优先"的实时要求
- 代码耦合严重，难以维护

### RTOS 的解决方式

```c
void Task_LED(void *p)   { for(;;) { LED_Toggle(); vTaskDelay(500); } }
void Task_Key(void *p)   { for(;;) { Key_Scan();  vTaskDelay(20);  } }
void Task_Sensor(void *p){ for(;;) { Read_Sensor(); vTaskDelay(1000); } }
void Task_Send(void *p)  { for(;;) { Send_Data();  vTaskDelay(5000); } }
```

**优势**
- 每个任务独立编写，互不干扰
- 阻塞一个任务不影响其他任务
- 用优先级保证实时性
- 用队列/信号量解耦模块

### 裸机 vs RTOS 选择

| 场景 | 推荐 |
|---|---|
| 功能 < 3 个，逻辑简单 | 裸机足够 |
| 需要多任务并行、有实时要求 | RTOS |
| 用状态机就能理清 | 裸机 + 状态机 |
| 功能会持续扩展 | 早点上 RTOS |
| 资源极度受限（如 8 位机） | 裸机 |

---

## 二、FreeRTOS 核心概念

### 1. 任务（Task）

- 任务是一个**独立的函数**，永不返回（`for(;;)` 无限循环）
- 每个任务有**独立的栈空间**
- 任务有**优先级**（数值越大优先级越高，0 最低）
- 同一时刻只有一个任务在运行（单核）

### 2. 任务状态

```
        创建
         ↓
    ┌─────────┐  阻塞超时/事件到达   ┌─────────┐
    │  就绪   │ ←──────────────────  │  阻塞   │
    │ Ready   │ ──────────────────→  │ Blocked │
    └─────────┘   等事件/延时         └─────────┘
         ↕ 调度器切换
    ┌─────────┐   vTaskSuspend      ┌─────────┐
    │  运行   │ ──────────────────→  │  挂起   │
    │ Running │ ←──────────────────  │Suspended│
    └─────────┘   vTaskResume        └─────────┘
```

| 状态 | 说明 |
|---|---|
| 运行（Running） | 正在占用 CPU |
| 就绪（Ready） | 可运行，但被更高优先级任务占用 |
| 阻塞（Blocked） | 等待延时或事件（**不占用 CPU**） |
| 挂起（Suspended） | 被显式挂起，不会被调度，等 `vTaskResume` |

### 3. 调度机制

- **抢占式调度**：高优先级任务就绪时立即抢占低优先级任务
- **时间片轮转**：同优先级任务轮流执行（需 `configUSE_TIME_SLICING = 1`）
- **空闲任务（Idle Task）**：系统自动创建，优先级 0，负责释放被删除任务的内存

### 4. 优先级设计原则

- 优先级 **0**：空闲任务（系统占用）
- 优先级 **1-2**：非实时任务（显示、日志）
- 优先级 **3-4**：常规业务任务（采集、通信）
- 优先级 **5+**：实时任务（电机控制、紧急响应）

**注意**：优先级不能随意堆叠，否则低优先级任务可能永远得不到执行（饿死）。

---

## 三、FreeRTOS 常用 API

### 1. 任务管理

| API | 作用 |
|---|---|
| `xTaskCreate()` | 动态创建任务（从堆分配栈） |
| `xTaskCreateStatic()` | 静态创建任务（栈由用户提供） |
| `vTaskDelete()` | 删除任务 |
| `vTaskDelay()` | 相对延时（从调用时刻算起） |
| `vTaskDelayUntil()` | 绝对延时（固定周期，推荐） |
| `vTaskSuspend()` / `vTaskResume()` | 挂起 / 恢复 |
| `uxTaskPriorityGet()` / `vTaskPrioritySet()` | 查询 / 设置优先级 |
| `taskYIELD()` | 主动让出 CPU |

**`vTaskDelay` vs `vTaskDelayUntil`（重要）**

```c
/* vTaskDelay：执行时间波动会导致周期变长 */
void Task_A(void *p) {
    for (;;) {
        Do_Work();            /* 耗时 30ms */
        vTaskDelay(100);      /* 实际周期 = 130ms，误差累积！ */
    }
}

/* vTaskDelayUntil：固定周期，不受执行时间影响 */
void Task_B(void *p) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        Do_Work();                        /* 耗时 30ms */
        vTaskDelayUntil(&last, 100);      /* 实际周期 = 100ms */
    }
}
```

### 2. 队列（Queue）—— 任务间传递数据

| API | 作用 |
|---|---|
| `xQueueCreate(len, size)` | 创建队列 |
| `xQueueSend()` / `xQueueSendToBack()` | 入队（尾部） |
| `xQueueSendToFront()` | 入队（头部，插队） |
| `xQueueReceive()` | 出队 |
| `xQueuePeek()` | 查看队首但不移除 |
| `uxQueueMessagesWaiting()` | 查询队列中元素个数 |

**典型用途**：中断 → 任务传数据、生产者-消费者解耦。

### 3. 信号量（Semaphore）

| 类型 | 创建 API | 用途 |
|---|---|---|
| 二值信号量 | `xSemaphoreCreateBinary()` | 任务同步、事件通知 |
| 计数信号量 | `xSemaphoreCreateCounting()` | 资源计数、事件累计 |
| 互斥锁（Mutex） | `xSemaphoreCreateMutex()` | 保护共享资源 |
| 递归互斥锁 | `xSemaphoreCreateRecursiveMutex()` | 同一任务可重复获取 |

**共同操作**：`xSemaphoreGive()` / `xSemaphoreTake()`

**互斥锁 vs 二值信号量的区别**

| | 二值信号量 | 互斥锁 |
|---|---|---|
| 用途 | 任务同步 | 保护共享资源 |
| 优先级继承 | 无 | **有**（防优先级反转） |
| 所有权 | 无 | 有（谁获取谁释放） |
| 中断中使用 | 可以 Give | **不可以** |

### 4. 事件组（Event Group）—— 多事件同步

| API | 作用 |
|---|---|
| `xEventGroupCreate()` | 创建事件组 |
| `xEventGroupSetBits()` | 置位（可从中断调 `FromISR` 版本） |
| `xEventGroupWaitBits()` | 等待一个或多个位 |
| `xEventGroupClearBits()` | 清位 |

**典型场景**：等待"WiFi 已连接 **且** 传感器初始化完成"才开始上报。

### 5. 任务通知（Task Notification）—— 最轻量的同步

| API | 作用 |
|---|---|
| `xTaskNotifyGive()` | 给任务发通知 |
| `ulTaskNotifyTake()` | 等待通知（类似二值信号量） |
| `xTaskNotify()` / `xTaskNotifyWait()` | 更灵活的通知（带值） |

**优势**：比信号量快约 45%，RAM 占用更少（无需额外创建对象）。
**限制**：只能一对一（一个任务只能有一个通知值）。

### 6. 中断中使用的规则（重要）

**FreeRTOS 里所有 API 都有 `FromISR` 版本**，中断中必须用带 `FromISR` 的：

```c
/* 错误写法：中断中用了非 FromISR 版本 */
void EXTI0_IRQHandler(void)
{
    xQueueSend(g_queue, &data, 0);        /* ✗ 危险，可能破坏系统 */
    xSemaphoreGive(g_sem);                /* ✗ 危险 */
}

/* 正确写法 */
void EXTI0_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(g_queue, &data, &woken);
    portYIELD_FROM_ISR(woken);            /* 如有需要则切换任务 */
}
```

### 7. 临界区

```c
/* 方式一：关中断（快，影响所有中断响应） */
taskENTER_CRITICAL();
/* 临界代码：不能调用任何 FreeRTOS API */
taskEXIT_CRITICAL();

/* 方式二：挂起调度器（允许中断，但不会任务切换） */
vTaskSuspendAll();
/* 临界代码 */
xTaskResumeAll();
```

### 8. 内存管理（heap_1 ~ heap_5）

| 方案 | 特点 | 适用 |
|---|---|---|
| `heap_1` | 只能分配不能释放 | 任务创建后不删除的简单系统 |
| `heap_2` | 可释放，但不合并碎片 | 已淘汰，不推荐 |
| `heap_3` | 包装标准 `malloc`/`free` | 需要线程安全的标准库 |
| `heap_4` | 可释放且**合并碎片** | **最常用，推荐** |
| `heap_5` | heap_4 + 支持多块不连续内存 | 内存分散在多个区域 |

配置项：`configTOTAL_HEAP_SIZE`（默认 17KB，STM32F103C8 的 20KB SRAM 下需要谨慎设置）。

---

## 四、学习目标

- [ ] 能说清裸机与 RTOS 的适用场景差异
- [ ] 能画出任务状态转换图
- [ ] 能独立创建多个任务并用队列通信
- [ ] 能区分二值信号量、计数信号量、互斥锁的使用场景
- [ ] 知道中断中必须用 `FromISR` 版本 API
- [ ] 能解释优先级反转及互斥锁的优先级继承机制
- [ ] 能根据自己的工程合理配置 `FreeRTOSConfig.h`
- [ ] 能用 RTOS 重构一个裸机项目

---

## 五、学习方法

1. **先裸机后 RTOS**：先用裸机写一遍"多任务"需求（会发现很难），再用 RTOS 重写，对比出价值。
2. **看栈使用情况**：用 `uxTaskGetStackHighWaterMark()` 查每个任务栈的剩余最小空间，避免栈溢出。
3. **任务状态可视化**：用 `vTaskList()` 打印所有任务的状态（需开 `configUSE_TRACE_FACILITY`）。
4. **故意制造优先级反转**：三个任务（高中低优先级 + 一个互斥锁），复现经典问题，再改用互斥锁解决。
5. **重构成瘾**：每学完一个 API，就把前面章节的裸机驱动改成任务形式。
6. **对比实验**：同一功能用裸机 + 状态机和用 RTOS 各写一遍，对比代码结构与可维护性。

---

## 六、标准示例

### 1. 最小 FreeRTOS 工程

```c
#include "FreeRTOS.h"
#include "task.h"

/* 任务 1：LED 闪烁 */
static void Task1_LED(void *pvParameters)
{
    for (;;) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8);
        vTaskDelay(pdMS_TO_TICKS(500));       /* 500ms，不占用 CPU */
    }
}

/* 任务 2：串口打印 */
static void Task2_Print(void *pvParameters)
{
    for (;;) {
        printf("System running, tick = %lu\r\n", xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    /* 创建任务：函数指针, 名称, 栈深度(字), 参数, 优先级, 句柄 */
    xTaskCreate(Task1_LED,   "LED",   128, NULL, 2, NULL);
    xTaskCreate(Task2_Print, "Print", 256, NULL, 1, NULL);

    vTaskStartScheduler();          /* 启动调度器，不再返回 */

    while (1);                      /* 正常不会执行到这里 */
}
```

### 2. 队列：生产者-消费者（中断 → 任务）

```c
QueueHandle_t g_queue;

typedef struct {
    uint8_t  type;
    uint32_t value;
} Msg_t;

/* 生产者：中断里写入 */
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    BaseType_t woken = pdFALSE;
    Msg_t msg = { .type = 1, .value = HAL_GetTick() };
    xQueueSendFromISR(g_queue, &msg, &woken);
    portYIELD_FROM_ISR(woken);
}

/* 消费者：任务里处理 */
static void Task_Process(void *p)
{
    Msg_t msg;
    for (;;) {
        /* 第二个参数：等待时间，portMAX_DELAY 表示一直等到有数据
           这里会阻塞，完全不占用 CPU */
        if (xQueueReceive(g_queue, &msg, portMAX_DELAY) == pdTRUE) {
            printf("Got msg type=%d value=%lu\r\n", msg.type, msg.value);
        }
    }
}

/* 创建 */
g_queue = xQueueCreate(10, sizeof(Msg_t));      /* 10 个元素的队列 */
xTaskCreate(Task_Process, "Proc", 256, NULL, 3, NULL);
```

### 3. 信号量：任务同步

```c
SemaphoreHandle_t g_bin_sem;

/* 任务 A：传感器采集完后通知任务 B */
static void Task_Sensor(void *p)
{
    for (;;) {
        if (DHT11_Read(&temp, &humi)) {
            xSemaphoreGive(g_bin_sem);          /* 发信号 */
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* 任务 B：等到信号才处理 */
static void Task_Display(void *p)
{
    for (;;) {
        if (xSemaphoreTake(g_bin_sem, portMAX_DELAY) == pdTRUE) {
            OLED_ShowNum(0, 0, temp, 3);        /* 有新数据才刷新 */
        }
    }
}

g_bin_sem = xSemaphoreCreateBinary();
```

### 4. 互斥锁：保护共享资源

```c
SemaphoreHandle_t g_i2c_mutex;

/* 多个任务都访问 IIC 总线（OLED 和 EEPROM 共用） */
static void IIC_WriteProtected(uint8_t addr, uint8_t data)
{
    xSemaphoreTake(g_i2c_mutex, portMAX_DELAY);     /* 上锁 */
    /* --- 临界区：IIC 操作 --- */
    AT24C02_WriteByte(addr, data);
    /* ---------------------- */
    xSemaphoreGive(g_i2c_mutex);                    /* 解锁 */
}

g_i2c_mutex = xSemaphoreCreateMutex();              /* 注意是 Mutex 不是 Binary */
```

> **为什么不加锁会出问题？** 任务 A 正在发 IIC 数据到一半，被高优先级任务 B 打断，B 也去用 IIC，时序就乱了。

### 5. 事件组：多条件同步

```c
EventGroupHandle_t g_events;

#define BIT_WIFI_OK     (1 << 0)
#define BIT_SENSOR_OK   (1 << 1)

/* 等待两个条件都满足 */
static void Task_Report(void *p)
{
    EventBits_t bits;
    for (;;) {
        bits = xEventGroupWaitBits(
                    g_events,
                    BIT_WIFI_OK | BIT_SENSOR_OK,  /* 等待这两个位 */
                    pdFALSE,                       /* 不清除位 */
                    pdTRUE,                        /* 两个都要满足 */
                    portMAX_DELAY);

        if ((bits & (BIT_WIFI_OK | BIT_SENSOR_OK)) == (BIT_WIFI_OK | BIT_SENSOR_OK)) {
            Send_Report();      /* 两个条件都满足，开始上报 */
        }
    }
}

g_events = xEventGroupCreate();
/* 条件满足时：xEventGroupSetBits(g_events, BIT_WIFI_OK); */
```

### 6. 固定周期任务（推荐写法）

```c
static void Task_Control(void *p)
{
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(10);   /* 10ms 周期 */

    for (;;) {
        Control_Loop();                     /* 控制算法，必须 < 10ms */
        vTaskDelayUntil(&last_wake, period); /* 严格 10ms 周期 */
    }
}
```

### 7. FreeRTOSConfig.h 关键配置

```c
#define configUSE_PREEMPTION            1    /* 抢占式调度 */
#define configUSE_TIME_SLICING          1    /* 同优先级时间片轮转 */
#define configCPU_CLOCK_HZ              (72000000)
#define configTICK_RATE_HZ              (1000)   /* 1ms 一个 tick */
#define configMAX_PRIORITIES            (7)      /* 优先级数量 */
#define configMINIMAL_STACK_SIZE        (128)    /* 最小任务栈（字） */
#define configTOTAL_HEAP_SIZE           ((size_t)(10 * 1024))  /* 堆大小 */
#define configMAX_TASK_NAME_LEN         (16)
#define configUSE_MUTEXES               1    /* 启用互斥锁 */
#define configUSE_RECURSIVE_MUTEXES     1
#define configUSE_COUNTING_SEMAPHORES   1
#define configUSE_TASK_NOTIFICATIONS    1
#define configUSE_TRACE_FACILITY        1    /* 可用 vTaskList */
#define configCHECK_FOR_STACK_OVERFLOW  2    /* 栈溢出检测 */
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0

/* 中断优先级：数值必须 >= configMAX_SYSCALL_INTERRUPT_PRIORITY 的 API 限制 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5
```

**重要**：FreeRTOS 要求 SysTick 和 PendSV 中断优先级设为**最低**，且调用 FreeRTOS API 的中断优先级不能高于 `configMAX_SYSCALL_INTERRUPT_PRIORITY`。

### 8. 调试：查看任务状态

```c
/* 需要在 FreeRTOSConfig.h 开启 configUSE_TRACE_FACILITY 和 configUSE_STATS_FORMATTING_FUNCTIONS */
static void Task_Monitor(void *p)
{
    char buf[512];
    for (;;) {
        vTaskList(buf);                     /* 生成任务列表 */
        printf("\r\nTaskName  State Prio Stack  Num\r\n%s\r\n", buf);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

**输出示例**

```
TaskName  State  Prio  Stack  Num
LED       R      2     100    3      <- R=Running, B=Blocked, S=Suspended
Print     B      1     180    4
IDLE      R      0     60     2
```

`Stack` 列是**剩余最小栈空间（字）**，如果接近 0 说明快溢出了。

---

## 七、自我检测

1. 裸机开发的三个主要痛点是什么？RTOS 怎么解决？
2. 画出任务的四种状态和转换条件。
3. `vTaskDelay` 和 `vTaskDelayUntil` 有什么区别？固定周期任务用哪个？
4. 队列、二值信号量、互斥锁各自解决什么问题？
5. 为什么中断里必须用 `FromISR` 版本的 API？
6. 什么是优先级反转？互斥锁怎么解决它？
7. 二值信号量和互斥锁最大的区别是什么？
8. 任务通知比信号量有什么优势？有什么限制？
9. `heap_4` 和 `heap_1` 的区别是什么？工程该选哪个？
10. 如何检测任务栈是否溢出？
11. 调用 FreeRTOS API 的中断优先级有什么限制？为什么？
12. 同优先级任务是怎么调度的？

---

## 八、常见坑

| 现象 | 原因 |
|---|---|
| 程序卡在 `vTaskStartScheduler` 之前 | 堆空间不够（`configTOTAL_HEAP_SIZE` 太小） |
| 系统运行一会儿就死机 | 任务栈溢出，用 `uxTaskGetStackHighWaterMark` 检查 |
| 某个任务永远不执行 | 优先级太低被饿死，或一直阻塞在等事件 |
| 中断里调 API 后系统崩溃 | 用了非 `FromISR` 版本 |
| 系统进了 HardFault | 调用 API 的中断优先级高于 `configMAX_SYSCALL_INTERRUPT_PRIORITY` |
| 共享资源偶尔出错 | 没用互斥锁保护 |
| `vTaskDelay(100)` 实际延时不对 | `configTICK_RATE_HZ` 不是 1000，应该用 `pdMS_TO_TICKS()` |
| 删除任务后内存越来越少 | 用 `heap_1` 或忘记 `vTaskDelete` 后的清理 |
| printf 在多个任务里输出乱码 | printf 非线程安全，需要互斥锁保护 |
| 高优先级任务一直占用 CPU | 没调用任何阻塞函数，饿死低优先级任务 |
| 看门狗在 RTOS 下误复位 | 喂狗任务优先级太低，需独立高优先级任务喂狗 |

---

## 九、拓展练习（按难度递增）

### 基础
1. **三任务闪烁**：三个不同频率的 LED 任务，验证任务独立运行。
2. **栈使用统计**：为每个任务打印 `uxTaskGetStackHighWaterMark()`，优化栈大小。

### 进阶
3. **串口命令 + RTOS**：串口接收任务解析命令，用队列发给执行任务。
4. **中断 → 队列 → 显示**：EXTI 中断通过队列把事件传给 OLED 显示任务。
5. **IIC 总线互斥**：OLED 和 EEPROM 分别在不同任务中访问，用互斥锁保护。

### 综合（推荐动手做）
6. **RTOS 重构第 13 章项目**：把"DHT11 + OLED + AT24C02 + 串口"的裸机代码改造成 4 个任务 + 队列 + 互斥锁的架构，对比代码组织方式。
7. **完整物联网终端**：DHT11 采集任务 + OLED 显示任务 + ESP8266 上报任务 + 按键配置任务，用事件组同步，用队列传数据，用 W25Q128 存离线数据。

### 深入
8. **优先级反转复现**：写三个任务复现经典优先级反转，再用互斥锁解决，记录现象。
9. **看门狗 + RTOS**：独立喂狗任务 + 任务健康检查（用事件组标记各任务心跳），任一任务卡死则停止喂狗。
10. **低功耗 + RTOS**：用 `configUSE_TICKLESS_IDLE` 实现空闲时进入低功耗模式。

---

## 十、学完之后

到这里，你已经走完了一条完整的嵌入式学习路径：

| 阶段 | 能力 |
|---|---|
| 01-03 | 能操作引脚、处理中断 |
| 04-09 | 能驱动定时器、通信接口、调试问题 |
| 10-13 | 能采集数据、掉电保存、人机交互 |
| 14-15 | 能对接外部存储与网络模块、会用工具链 |
| 16-17 | 能设计多任务系统架构 |

**接下来的方向（按兴趣选择）**

| 方向 | 建议补齐的内容 |
|---|---|
| 工业控制 | Modbus 协议、CAN 总线、4-20mA、隔离电路 |
| 物联网 | MQTT、HTTP、OTA 升级、低功耗设计 |
| 电机控制 | 高级定时器死区、FOC 算法、编码器接口 |
| 产品化 | Bootloader、IAP 升级、EMC 设计、RTOS 稳定性 |
| 深耕 RTOS | 任务通知机制源码、内存管理源码、移植到其他内核 |

**最后一个建议**：脱离教程，独立完成一个从需求到成品的项目。遇到问题再回头查对应章节——**能独立解决问题，才算真正学完**。
