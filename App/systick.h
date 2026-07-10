/**
  ******************************************************************************
  * @file    systick.h
  * @brief   SysTick 1ms 系统节拍, 提供 millis() 给主循环做周期调度
  ******************************************************************************
  */
#ifndef __SYSTICK_H
#define __SYSTICK_H

#include "stm32f10x.h"

void SysTick_Init(void);
void SysTick_IncTick(void);          /* 仅由 stm32f10x_it.c 的 SysTick_Handler 调用 */
uint32_t millis(void);
void Delay_ms(uint32_t ms);          /* 阻塞延时, 只允许在初始化阶段使用 */

#endif /* __SYSTICK_H */
