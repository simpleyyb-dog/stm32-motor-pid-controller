/**
  ******************************************************************************
  * @file    motor.c
  * @brief   L298N 直流电机控制
  *
  * PWM: 72MHz / 72(PSC) / 100(ARR) = 10kHz, CCR1 = 0~100 对应占空比 0~100%
  ******************************************************************************
  */
#include "motor.h"
#include "app_config.h"

static uint8_t s_duty = 0;

void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB
                         | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* L298N IN3(PB12) / IN4(PB13), 上电默认全 0 = 滑行 */
    GPIO_InitStructure.GPIO_Pin = MOTOR_IN3_PIN | MOTOR_IN4_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MOTOR_IN3_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(MOTOR_IN3_PORT, MOTOR_IN3_PIN | MOTOR_IN4_PIN);

    /* PA6 -> TIM3_CH1 复用推挽 -> L298N ENB */
    GPIO_InitStructure.GPIO_Pin = MOTOR_ENB_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MOTOR_ENB_PORT, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Prescaler = MOTOR_PWM_PSC;
    TIM_TimeBaseStructure.TIM_Period = MOTOR_PWM_ARR;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);

    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

void Motor_SetDirection(MotorDirection dir)
{
    if (dir == MOTOR_DIR_FWD)
    {
        GPIO_SetBits(MOTOR_IN3_PORT, MOTOR_IN3_PIN);
        GPIO_ResetBits(MOTOR_IN4_PORT, MOTOR_IN4_PIN);
    }
    else
    {
        GPIO_ResetBits(MOTOR_IN3_PORT, MOTOR_IN3_PIN);
        GPIO_SetBits(MOTOR_IN4_PORT, MOTOR_IN4_PIN);
    }
}

void Motor_Start(MotorDirection dir)
{
    Motor_SetDirection(dir);
}

void Motor_Stop(void)
{
    GPIO_ResetBits(MOTOR_IN3_PORT, MOTOR_IN3_PIN);
    GPIO_ResetBits(MOTOR_IN4_PORT, MOTOR_IN4_PIN);
    Motor_SetDuty(0);
}

void Motor_SetDuty(uint8_t duty)
{
    if (duty > PWM_DUTY_MAX)
    {
        duty = PWM_DUTY_MAX;
    }
    s_duty = duty;
    TIM_SetCompare1(TIM3, duty);
}

uint8_t Motor_GetDuty(void)
{
    return s_duty;
}
