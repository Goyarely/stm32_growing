#include "key.h"
#include "stm32f1xx_hal.h"

/* ==================================================================
 * 下面这些是【已经给你的基础设施】，不用改
 * ================================================================== */

/* 硬件映射表：加一个按键只改这里 */
typedef struct
{
    GPIO_TypeDef *port;      /* GPIOA / GPIOB / GPIOC */
    uint16_t      pin;       /* GPIO_PIN_x            */
} Key_Hw_t;

static const Key_Hw_t g_keyHw[] =
{
    /* KEY1 (SW2) */ { GPIOB, GPIO_PIN_12 },
    /* KEY2 (SW3) */ { GPIOB, GPIO_PIN_13 },
};

#define KEY_HW_NUM   (sizeof(g_keyHw) / sizeof(g_keyHw[0]))

/* 编译期断言：表项数必须和 Key_Id_t 里的 KEY_NUM 一致 */
typedef char key_hw_num_must_match[(KEY_NUM == (int)KEY_HW_NUM) ? 1 : -1];

/* 中断事件标志：static → 只在本文件可见 */
static volatile uint8_t s_keyFlag[KEY_NUM];


/* ##################################################################
 * TODO ① · Key_Init()  —— 初始化
 * ##################################################################
 *
 * 目标：把 KEY1 / KEY2 配成"下降沿触发的外部中断"，并让 NVIC 放行
 *
 * 要做 4 件事：
 *
 *   [1] 开时钟
 *       - 哪个 GPIO 口？（看 g_keyHw 表）
 *       - 还要开一个和中断有关的外设时钟 —— 想清楚是哪个，
 *         它在 APB2 上，是 F1 特有的一个坑
 *
 *   [2] 配 GPIO
 *       - Mode  = ?  ← 去 STM32_Hal/Inc/stm32f1xx_hal_gpio.h 里找那个
 *                      "下降沿中断"的枚举名（不是 GPIO_MODE_INPUT）
 *       - Pull  = ?  ← 先回答：板上这个引脚有没有外部上拉/下拉？
 *       - 两个按键在同一端口，可以一次配完（Pin 用 | 拼起来）
 *       - 别忘了最后要真正把参数交给 HAL 的那个调用
 *
 *   [3] 清掉可能残留的挂起标志
 *       - 清哪个寄存器？
 *       - 用什么运算符？（这个寄存器是"写 1 清除"型，运算符只有一种）
 *
 *   [4] NVIC
 *       - 设优先级 + 使能
 *       - KEY1/KEY2 的 EXTI 线共用一个向量 —— 是哪一条？
 *         （提示：EXTI10~15 是一组，EXTI5~9 是一组）
 *       - 向量名去 Arm/stm32f103xb.h 里搜 EXTI 相关的 IRQn 枚举
 *
 * 卡住就把"TODO① 的 [n]"发我，我带你推，不直接给代码。
 * ================================================================== */
void Key_Init(void)
{
    /* TODO ① */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint32_t pins = GPIO_PIN_12 | GPIO_PIN_13;

    //使能
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    //配gpio
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pin = pins;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    EXTI->PR = pins;
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}


/* ##################################################################
 * TODO ② · Key_IrqHandler()  —— 中断处理
 * ##################################################################
 *
 * 目标：不用 HAL_GPIO_EXTI_IRQHandler()，自己把它的三件事写出来
 *
 * 参考：STM32_Hal/Src/stm32f1xx_hal_gpio.c 第 549~554 行
 *       （先去看那几行，就是标准答案的骨架）
 *
 * 三件事（顺序很重要，先想清楚为什么）：
 *
 *   ① 查：这条 EXTI 线挂起了吗？
 *        - 读哪个寄存器？怎么判断某一位？
 *
 *   ② 清：把标志清掉
 *        - 同一个寄存器，但这次是"写"
 *        - 注意那个"写 1 清除"的特性：只能用 = ，不能用 |= 或 &= ~
 *
 *   ③ 报：让上层知道"有按键事件"
 *        - 方式 A：调用 HAL_GPIO_EXTI_Callback(pin)（那你要自己保留回调的实现）
 *        - 方式 B：直接写 s_keyFlag[i] = 1
 *        - 自己写寄存器版时，方式 B 更自然，也少一层
 *
 * 提示：
 *   - g_keyHw[i].pin 的值和 EXTI 线的掩码位刚好对应，可以直接复用
 *   - 可以用 for 循环遍历 g_keyHw[]（和原型一样），也可以两条 if 展开
 *   - 想清楚【为什么不能无条件把所有位都清一遍】
 *
 * 卡住就把你写到哪一步、卡在哪发我。
 * ================================================================== */
void Key_IrqHandler(void)
{
    /* TODO ② */
    for(uint8_t i = 0; i<KEY_HW_NUM; i++)
    {
        if(__HAL_GPIO_EXTI_GET_IT(g_keyHw[i].pin))
        {
            EXTI->PR = g_keyHw[i].pin;
            s_keyFlag[i] = 1;
        }
    }
    
}


/* ##################################################################
 * TODO ③ · Key_IsPressed()  —— 读原始电平（非阻塞）
 * ##################################################################
 *
 * 目标：按下返回 1，松开返回 0。不做任何延时、不阻塞。
 *
 * 要点：
 *   - 先做参数合法性检查（keyNo 越界怎么办）
 *   - 板上是【低电平按下】，HAL 读回来的是 GPIO_PinState 枚举，
 *     要想办法把它翻译成 0/1
 * ================================================================== */
uint8_t Key_IsPressed(Key_Id_t keyNo)
{
    /* TODO ③ */
    if((unsigned)keyNo >= KEY_HW_NUM)
    {
        return 0U;
    }
    if(HAL_GPIO_ReadPin(g_keyHw[keyNo].port, g_keyHw[keyNo].pin) == GPIO_PIN_RESET)
    {
        return 1;
    }
    return 0U;
}


/* ##################################################################
 * TODO ④ · Key_HasEvent()  —— 取"按下"事件（非阻塞）
 * ##################################################################
 *
 * 目标：有事件 → 返回 1，【并顺便把标志清掉】；没有 → 返回 0
 *
 * 要点：
 *   - 参数合法性检查
 *   - 标志是 s_keyFlag[]，类型是 volatile uint8_t
 *   - "读走即清"是设计的关键：调用者不可能忘记清标志
 * ================================================================== */
uint8_t Key_HasEvent(Key_Id_t keyNo)
{
    /* TODO ④ */
    if(s_keyFlag[keyNo] == 1u)
    {
        s_keyFlag[keyNo] = 0u;
        return 1u;
    }

    return 0U;
}
