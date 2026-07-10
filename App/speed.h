/**
  ******************************************************************************
  * @file    speed.h
  * @brief   TIM2正交编码器测速: PA0=A相, PA1=B相
  ******************************************************************************
  */
#ifndef __SPEED_H
#define __SPEED_H

#include "stm32f10x.h"

void Speed_Init(void);
void Speed_Update(void);                 /* 每 SPEED_SAMPLE_PERIOD_MS 调用一次 */
float Speed_GetRPM(void);                /* 最近一次计算的实际转速 (输出轴 rpm) */
float Speed_GetDisplayRPM(void);         /* 一阶滤波后的显示转速 */

#endif /* __SPEED_H */
