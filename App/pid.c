/**
  ******************************************************************************
  * @file    pid.c
  * @brief   转速 PID 闭环控制
  *
  * pwm_duty = 前馈 + Kp*e + Ki*∫e + Kd*de/dt
  *
  * 前馈: 以死区占空比 MOTOR_MIN_RUN_DUTY 和实测最高转速 MOTOR_MAX_RPM
  *       做线性标定, PID 只需修正残差, 调参更容易。
  * 抗积分饱和: setpoint斜坡(从源头避免大误差) + 积分限幅 + 输出饱和且误差同向时放弃本次积分。
  ******************************************************************************
  */
#include "pid.h"
#include "app_config.h"

typedef struct
{
    float kp;
    float ki;
    float kd;
    float integral;
    float last_error;
    float out;                       /* 最近一次纯 PID 输出 */
} PID_t;

static PID_t s_pid;

void PID_Init(void)
{
    PID_SetParams(PID_KP, PID_KI, PID_KD);
    PID_Reset();
}

void PID_Reset(void)
{
    s_pid.integral = 0.0f;
    s_pid.last_error = 0.0f;
    s_pid.out = 0.0f;
}

void PID_SetParams(float kp, float ki, float kd)
{
    s_pid.kp = kp;
    s_pid.ki = ki;
    s_pid.kd = kd;
}

float PID_Update(float target_rpm, float real_rpm, float dt)
{
    float error = target_rpm - real_rpm;
    float new_integral;
    float derivative = 0.0f;
    float base_duty;
    float out;

    /* 前馈 */
    if (target_rpm > 0.5f)
    {
        base_duty = (float)MOTOR_MIN_RUN_DUTY
                  + target_rpm * (float)(PWM_DUTY_MAX - MOTOR_MIN_RUN_DUTY)
                  / (float)MOTOR_MAX_RPM;
    }
    else
    {
        base_duty = 0.0f;
    }

    /* 积分始终累加(配合setpoint斜坡防过冲, 不再用积分分离, 否则稳态偏差大时会把纠偏堵死) */
    new_integral = s_pid.integral + error * dt;
    if (new_integral > PID_INTEGRAL_MAX)
    {
        new_integral = PID_INTEGRAL_MAX;
    }
    if (new_integral < -PID_INTEGRAL_MAX)
    {
        new_integral = -PID_INTEGRAL_MAX;
    }

    if (dt > 0.0f)
    {
        derivative = (error - s_pid.last_error) / dt;
    }
    s_pid.last_error = error;

    s_pid.out = s_pid.kp * error + s_pid.ki * new_integral + s_pid.kd * derivative;
    out = base_duty + s_pid.out;

    /* 输出饱和且误差仍朝饱和方向时不更新积分, 防止积分越堆越大 */
    if ((out > (float)PWM_DUTY_MAX && error > 0.0f)
     || (out < (float)PWM_DUTY_MIN && error < 0.0f))
    {
        /* 保持原积分 */
    }
    else
    {
        s_pid.integral = new_integral;
    }

    if (out > (float)PWM_DUTY_MAX)
    {
        out = (float)PWM_DUTY_MAX;
    }
    if (out < (float)PWM_DUTY_MIN)
    {
        out = (float)PWM_DUTY_MIN;
    }

    /* 直流电机启动死区补偿: 目标不为 0 时保证最小运行占空比 */
    if (target_rpm > 0.5f && out < (float)MOTOR_MIN_RUN_DUTY)
    {
        out = (float)MOTOR_MIN_RUN_DUTY;
    }

    return out;
}

float PID_GetOutput(void)
{
    return s_pid.out;
}
