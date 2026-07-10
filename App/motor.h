/**
  ******************************************************************************
  * @file    motor.h
  * @brief   L298N B通道控制: ENB=TIM3_CH1(PA6), IN3=PB12, IN4=PB13
  ******************************************************************************
  */
#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

typedef enum
{
    MOTOR_DIR_FWD = 0,               /* 正转: IN3=1, IN4=0 */
    MOTOR_DIR_REV                    /* 反转: IN3=0, IN4=1 */
} MotorDirection;

void Motor_Init(void);
void Motor_Start(MotorDirection dir);
void Motor_Stop(void);               /* 滑行停止: IN3=IN4=0, PWM=0 */
void Motor_SetDirection(MotorDirection dir);
void Motor_SetDuty(uint8_t duty);    /* 0~100 */
uint8_t Motor_GetDuty(void);

#endif /* __MOTOR_H */
