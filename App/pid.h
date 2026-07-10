/**
  ******************************************************************************
  * @file    pid.h
  * @brief   转速 PID 闭环控制 (前馈 + PI[D] + 抗积分饱和 + 死区补偿)
  ******************************************************************************
  */
#ifndef __PID_H
#define __PID_H

#include "stm32f10x.h"

void PID_Init(void);
void PID_Reset(void);                            /* 积分/历史误差清零, STOP 时调用 */
void PID_SetParams(float kp, float ki, float kd);
float PID_Update(float target_rpm, float real_rpm, float dt);  /* 返回 0~100 占空比 */
float PID_GetOutput(void);                       /* 最近一次纯 PID 输出(不含前馈), 调参观察用 */

#endif /* __PID_H */
