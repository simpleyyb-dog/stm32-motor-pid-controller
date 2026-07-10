/**
  ******************************************************************************
  * @file    speed.c
  * @brief   TIM2正交编码器测速，PA0=A相，PA1=B相
  *
  * TIM2工作在Encoder Mode 3 (TI12)，同时利用A/B两相边沿进行4倍频计数。
  * 每个控制周期读取计数差，根据实际经过时间换算输出轴转速。
  ******************************************************************************
  */
#include "speed.h"
#include "app_config.h"
#include "systick.h"

static uint16_t s_last_count = 0;
static uint32_t s_last_update_ms = 0;
static float s_rpm = 0.0f;
static float s_display_rpm = 0.0f;

void Speed_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* PA0=A相、PA1=B相。片内上拉适用于开漏编码器输出。 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_EncoderInterfaceConfig(TIM2, TIM_EncoderMode_TI12,
                               TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

    /* 两相均使用数字滤波，减小电机干扰造成的误计数。 */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICFilter = 8;
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
    TIM_ICInit(TIM2, &TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    TIM_ICInit(TIM2, &TIM_ICInitStructure);

    TIM_SetCounter(TIM2, 0);
    TIM_ClearFlag(TIM2, TIM_FLAG_Update | TIM_FLAG_CC1 | TIM_FLAG_CC2);
    TIM_Cmd(TIM2, ENABLE);

    s_last_count = 0;
    s_last_update_ms = millis();
}

void Speed_Update(void)
{
    uint16_t current_count = TIM_GetCounter(TIM2);
    int16_t signed_delta = (int16_t)(current_count - s_last_count);
    uint16_t pulse_delta;
    uint32_t now = millis();
    uint32_t elapsed_ms = (uint32_t)(now - s_last_update_ms);

    s_last_count = current_count;
    s_last_update_ms = now;

    pulse_delta = (signed_delta < 0) ? (uint16_t)(-signed_delta)
                                     : (uint16_t)signed_delta;

    if (elapsed_ms > 0)
    {
        s_rpm = ((float)pulse_delta * 60000.0f)
              / ((float)SPEED_COUNTS_PER_REV * (float)elapsed_ms);
    }
    else
    {
        s_rpm = 0.0f;
    }

    s_display_rpm = s_display_rpm * SPEED_FILTER_ALPHA
                  + s_rpm * (1.0f - SPEED_FILTER_ALPHA);
}

float Speed_GetRPM(void)
{
    return s_rpm;
}

float Speed_GetDisplayRPM(void)
{
    return s_display_rpm;
}
