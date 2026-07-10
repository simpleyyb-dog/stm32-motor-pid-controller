/**
  ******************************************************************************
  * @file    main.c
  * @brief   基于 STM32F103C8T6 的 JGA25-370 直流电机 PID 调速系统
  *
  * 系统组成:
87  *   PWM 调速   : TIM3_CH1(PA6) 10kHz -> L298N ENB (ENB 跳帽必须拔掉)
  *   方向控制   : PB12->IN3, PB13->IN4
  *   测速      : 编码器A/B相 -> PA0/PA1, TIM2正交编码器模式
  *   闭环      : 前馈 + PI(D), 200ms 控制周期
  *   按键      : 4x4矩阵键盘, S1加速/S2减速/S3启停/S4换向
  *   显示      : SSD1306 OLED, I2C1(PB6/PB7), 200ms 刷新
  *
  * 主循环用 millis() 做周期调度, 无长阻塞延时;
  * TIM2硬件完成A/B相正交计数, 主循环每200ms计算一次转速。
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "app_config.h"
#include "systick.h"
#include "motor.h"
#include "key.h"
#include "speed.h"
#include "pid.h"
#include "oled_ui.h"

typedef enum
{
    MOTOR_STATE_STOP = 0,
    MOTOR_STATE_RUN
} MotorState;

static int16_t target_rpm = MOTOR_DEFAULT_RPM;   /* 峰值转速: 运行与点动共用, S1/S2调节 */
static MotorState motor_state = MOTOR_STATE_STOP;
static MotorDirection motor_dir = MOTOR_DIR_FWD;
static uint8_t pwm_duty = 0;
static uint8_t jog_active = 0;        /* 点动进行中: 仅停止状态下生效 */
static MotorDirection jog_dir = MOTOR_DIR_FWD;   /* 当前点动方向: S5正转 / S6反转 */
static float set_ramp = 0.0f;         /* 送给PID的斜坡目标(rpm), 向 target_rpm 平滑爬升; 运行/点动共用 */

static void App_HandleKey(KeyEvent evt)
{
    switch (evt)
    {
    case KEY_EVENT_UP:
        target_rpm += TARGET_RPM_STEP;
        if (target_rpm > MOTOR_MAX_RPM)
        {
            target_rpm = MOTOR_MAX_RPM;
        }
        break;

    case KEY_EVENT_DOWN:
        target_rpm -= TARGET_RPM_STEP;
        if (target_rpm < 0)
        {
            target_rpm = 0;
        }
        break;

    case KEY_EVENT_START_STOP:
        if (motor_state == MOTOR_STATE_RUN)
        {
            motor_state = MOTOR_STATE_STOP;
            Motor_Stop();            /* 滑行停止, target_rpm 保留 */
            PID_Reset();
            pwm_duty = 0;
        }
        else
        {
            motor_state = MOTOR_STATE_RUN;
            Motor_Start(motor_dir);
        }
        break;

    case KEY_EVENT_DIR:
        /* 只允许停止状态换向: 运行中反接 H 桥会产生很大的电流冲击 */
        if (motor_state == MOTOR_STATE_STOP)
        {
            motor_dir = (motor_dir == MOTOR_DIR_FWD) ? MOTOR_DIR_REV
                                                     : MOTOR_DIR_FWD;
        }
        break;

    default:
        break;
    }
}

int main(void)
{
    uint32_t last_key_ms = 0;
    uint32_t last_ctrl_ms = 0;
    uint32_t last_ui_ms = 0;
    uint32_t last_anim_ms = 20;      /* 错开初相, 避免和200ms控制tick同毫秒抢I2C */

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SysTick_Init();

    Motor_Init();
    Key_Init();
    Speed_Init();
    PID_Init();
    UI_Init();

    Motor_Stop();                    /* 上电默认: STOP, target=MOTOR_DEFAULT_RPM, FWD, duty=0 */

    while (1)
    {
        uint32_t now = millis();

        /* ---- 每 10ms: 按键扫描 ---- */
        if ((uint32_t)(now - last_key_ms) >= KEY_SCAN_PERIOD_MS)
        {
            KeyEvent evt;
            uint8_t fwd_held;
            uint8_t rev_held;
            uint8_t jog_held;

            last_key_ms = now;
            evt = Key_Scan();            /* 先更新所有按键(含点动键)的消抖状态 */
            fwd_held = Key_IsHeld(KEY_JOG_FWD_INDEX);
            rev_held = Key_IsHeld(KEY_JOG_REV_INDEX);
            jog_held = (uint8_t)(fwd_held ^ rev_held);   /* 恰好一个键按下才点动, 同按视为无效 */

            /* 点动: 仅停止状态下生效, S5正转/S6反转, 实际转速由闭环斜坡跟到共用峰值 target_rpm */
            if (jog_held && motor_state == MOTOR_STATE_STOP)
            {
                if (!jog_active)
                {
                    jog_active = 1;
                    jog_dir = fwd_held ? MOTOR_DIR_FWD : MOTOR_DIR_REV;
                    set_ramp = 0.0f;             /* 每次按下从0开始平滑起步 */
                    PID_Reset();
                    Motor_Start(jog_dir);
                    last_ui_ms = now - UI_REFRESH_PERIOD_MS;   /* 立即刷新 */
                }
            }
            else if (jog_active)
            {
                jog_active = 0;
                Motor_Stop();            /* 松开/双键: 滑行停 */
                PID_Reset();
                pwm_duty = 0;
                last_ui_ms = now - UI_REFRESH_PERIOD_MS;
            }

            /* 点动期间忽略其它按键, 避免与启停/换向冲突 */
            if (!jog_active && evt != KEY_EVENT_NONE)
            {
                App_HandleKey(evt);
                /* 按键后立即刷新UI, 不等200ms周期, 手感跟手 */
                last_ui_ms = now - UI_REFRESH_PERIOD_MS;
            }
        }

        /* ---- 每 200ms: 测速 + PID 控制 ---- */
        if ((uint32_t)(now - last_ctrl_ms) >= PID_CONTROL_PERIOD_MS)
        {
            float real_rpm;

            last_ctrl_ms = now;
            Speed_Update();
            real_rpm = Speed_GetRPM();

            if (motor_state == MOTOR_STATE_RUN || jog_active)
            {
                /* 目标斜坡: 运行与点动都让目标按设定加减速率平滑逼近 target_rpm,
                   不让目标跳变把电机冲过头(电机无法主动刹车, 过冲后回落很慢) */
                float step = MOTOR_ACCEL_RPM_S
                           * ((float)PID_CONTROL_PERIOD_MS / 1000.0f);
                float duty_f;

                if (set_ramp + step < (float)target_rpm)
                {
                    set_ramp += step;
                }
                else if (set_ramp - step > (float)target_rpm)
                {
                    set_ramp -= step;
                }
                else
                {
                    set_ramp = (float)target_rpm;
                }

                duty_f = PID_Update(set_ramp, real_rpm,
                                    (float)PID_CONTROL_PERIOD_MS / 1000.0f);
                pwm_duty = (uint8_t)(duty_f + 0.5f);
                Motor_SetDirection(jog_active ? jog_dir : motor_dir);
                Motor_SetDuty(pwm_duty);
            }
            else
            {
                pwm_duty = 0;
                Motor_SetDuty(0);
                PID_Reset();
                set_ramp = 0.0f;             /* 停止时复位斜坡, 下次从0平滑起步 */
            }
        }

        /* ---- 每 200ms: OLED 文字刷新 ---- */
        if ((uint32_t)(now - last_ui_ms) >= UI_REFRESH_PERIOD_MS)
        {
            last_ui_ms = now;
            /* 运行与点动共用 target_rpm 作峰值; 方向用点动方向(S5正/S6反), 让风扇随之反向 */
            UI_Update(target_rpm,
                      Speed_GetDisplayRPM(), pwm_duty,
                      (uint8_t)(motor_state == MOTOR_STATE_RUN || jog_active),
                      (uint8_t)((jog_active ? jog_dir : motor_dir) == MOTOR_DIR_FWD),
                      jog_active);
        }

        /* ---- 每 40ms: 风扇动画 (仅运行页生效) ---- */
        if ((uint32_t)(now - last_anim_ms) >= ANIM_TICK_MS)
        {
            uint32_t elapsed = now - last_anim_ms;

            last_anim_ms = now;
            UI_AnimTick(Speed_GetDisplayRPM(),
                        (uint8_t)((jog_active ? jog_dir : motor_dir) == MOTOR_DIR_FWD),
                        elapsed);
        }
    }
}
