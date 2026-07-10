/**
  ******************************************************************************
  * @file    systick.c
  * @brief   SysTick 1ms 系统节拍
  *
  * 注意: SysTick_Handler 在工程模板的 stm32f10x_it.c 中已有定义,
  *       这里不能重复定义, 否则链接报重复符号错误。
  *       做法: stm32f10x_it.c 的 SysTick_Handler 里调用 SysTick_IncTick()。
  ******************************************************************************
  */
#include "systick.h"

static volatile uint32_t system_ms = 0;

void SysTick_Init(void)
{
    /* SystemCoreClock = 72MHz, 1ms 中断一次 */
    SysTick_Config(SystemCoreClock / 1000);
}

void SysTick_IncTick(void)
{
    system_ms++;
}

uint32_t millis(void)
{
    return system_ms;
}

void Delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < ms)
    {
    }
}
