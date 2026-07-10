/**
  ******************************************************************************
  * @file    app_config.h
  * @brief   集中存放系统所有可调参数:引脚、PWM、测速、PID、按键、OLED
  *
  * 引脚总表:
  *   PA6  -> TIM3_CH1 PWM     -> L298N ENB (拔掉ENB跳帽!)
  *   PB12 -> GPIO 推挽输出     -> L298N IN3
  *   PB13 -> GPIO 推挽输出     -> L298N IN4
  *   PB0/PB1/PB10/PB11       -> 4x4键盘 R1/R2/R3/R4
  *   PA8/PA9/PA10/PA11       -> 4x4键盘 C1/C2/C3/C4
  *   PB6  -> I2C1_SCL         -> OLED SCL
  *   PB7  -> I2C1_SDA         -> OLED SDA
  *   PA0  -> TIM2_CH1         -> 编码器A相
  *   PA1  -> TIM2_CH2         -> 编码器B相 (编码器建议用3.3V供电)
  ******************************************************************************
  */
#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* ===== 电机 PWM: TIM3_CH1 -> PA6 -> L298N ENB =====
 * L298N 开关沿很慢(us级), 频率越高有效导通时间损失越大;
 * 降到 1kHz 可减小开关损耗, 提高满占空比下的实际电压和峰值转速。 */
#define MOTOR_PWM_FREQ_HZ        1000            /* PWM 频率 1kHz */
#define MOTOR_PWM_PSC            (720 - 1)       /* 72MHz/720 = 100kHz 计数 */
#define MOTOR_PWM_ARR            (100 - 1)       /* 100kHz/100 = 1kHz, CCR 0~100 即占空比 */
#define PWM_DUTY_MIN             0
#define PWM_DUTY_MAX             100
#define MOTOR_MIN_RUN_DUTY       20              /* 低速启动死区补偿占空比 */

/* ===== L298N B通道: ENB / IN3 / IN4 ===== */
#define MOTOR_ENB_PORT           GPIOA
#define MOTOR_ENB_PIN            GPIO_Pin_6
#define MOTOR_IN3_PORT           GPIOB
#define MOTOR_IN3_PIN            GPIO_Pin_12
#define MOTOR_IN4_PORT           GPIOB
#define MOTOR_IN4_PIN            GPIO_Pin_13

/* ===== 电机参数 =====
 * MOTOR_MAX_RPM 不要直接抄电机额定值!
 * L298N 压降约 2.5~4V, 12V 供电时电机实际只有 8~9V。
 * 标定方法: 先把 target 设到最大让 PID 输出饱和(占空比100%),
 * 记下 OLED 上稳定的 Real 读数, 把它填到这里。 */
#define MOTOR_MAX_RPM            220             /* 转速上限(满速): 前馈满速参考 / 目标上限 / 条形图满刻度 */
#define TARGET_RPM_STEP          10              /* 按键每次增减步进 */

/* ===== 4x4矩阵键盘 =====
 * R1/R2/R3/R4 -> PB0/PB1/PB10/PB11 (开漏扫描输出)
 * C1/C2/C3/C4 -> PA8/PA9/PA10/PA11 (上拉输入)
 * S1=加速, S2=减速, S3=启动/停止, S4=换向
 */
#define KEY_ROW_PORT             GPIOB
#define KEY_R1_PIN               GPIO_Pin_0
#define KEY_R2_PIN               GPIO_Pin_1
#define KEY_R3_PIN               GPIO_Pin_10
#define KEY_R4_PIN               GPIO_Pin_11
#define KEY_COL_PORT             GPIOA
#define KEY_C1_PIN               GPIO_Pin_8
#define KEY_C2_PIN               GPIO_Pin_9
#define KEY_C3_PIN               GPIO_Pin_10
#define KEY_C4_PIN               GPIO_Pin_11
#define KEY_SCAN_PERIOD_MS       10
#define KEY_DEBOUNCE_COUNT       2               /* 2次扫描一致才确认 = 20ms 消抖 */

/* ===== 点动(Jog): 按住闭环低速寸动, 松开滑行停; S5正转 / S6反转 =====
 * 矩阵索引 = 行*列数 + 列。S5=R1C2=0*4+1=1, S6=R2C2=1*4+1=5。
 * 只在电机停止状态下生效; 两键同时按下视为无效。 */
#define KEY_JOG_FWD_INDEX        1               /* 点动正转键 S5 的扫描矩阵索引 */
#define KEY_JOG_REV_INDEX        5               /* 点动反转键 S6 的扫描矩阵索引 */
#define MOTOR_DEFAULT_RPM        90              /* 上电默认目标转速(rpm); 点动与运行共用此峰值, 用S1/S2调节 */
#define MOTOR_ACCEL_RPM_S        45.0f           /* 运行与点动共用的目标加减速率(rpm/s), 越小越柔, 防过冲 */

/* ===== 正交编码器: PA0/TIM2_CH1=A, PA1/TIM2_CH2=B =====
 * TIM2使用编码器模式TI12，对A/B两相进行4倍频计数。
 * 若实际电机参数不同，请修改每转脉冲数和减速比。 */
#define ENCODER_PPR_MOTOR        11              /* 编码器电机轴每转脉冲数 */
#define GEAR_RATIO               18.4f           /* 减速比: 按实测 屏60/实114 标定(35×60/114); 标准值多为18.8 */
#define ENCODER_QUADRATURE       4               /* A/B两相4倍频 */
#define SPEED_COUNTS_PER_REV     (ENCODER_PPR_MOTOR * GEAR_RATIO * ENCODER_QUADRATURE)
#define SPEED_SAMPLE_PERIOD_MS   50              /* 测速窗口=控制周期; 实际按 elapsed_ms 换算 */
#define SPEED_FILTER_ALPHA       0.85f           /* 显示滤波(50ms更新更频繁, 提高α让显示更平稳) */

/* ===== PID (推荐先 PI, Kd=0, 测速噪声会让微分项抖动) ===== */
#define PID_CONTROL_PERIOD_MS    50              /* 控制周期, 从200ms提速到50ms以减小滞后/超调 */
#define PID_KP                   1.00f
#define PID_KI                   0.80f           /* 积分修正前馈残差; setpoint斜坡已防过冲 */
#define PID_KD                   0.00f
#define PID_INTEGRAL_MAX         300.0f          /* 积分限幅 (rpm*s), 留足修正前馈偏差的余量 */

/* ===== OLED: SSD1306, I2C1, PB6/PB7 ===== */
#define OLED_ADDR                0x78            /* 7位地址0x3C左移1位 */
#define UI_REFRESH_PERIOD_MS     200

/* ===== 运行页风扇动画 =====
 * 动画速度与实际转速成正比: 屏幕帧频(帧/s) = rpm * ANIM_FPS_PER_RPM。
 * 一个视觉周期8帧(三叶120度), 0.2f 时 120rpm 约等于屏幕1转/s,
 * 再快会因刷新率不够出现"车轮倒转"错觉。 */
#define ANIM_TICK_MS             40              /* 动画刷新周期 (25fps) */
#define ANIM_FPS_PER_RPM         0.2f            /* 转速->动画帧频映射系数 */
#define ANIM_X                   86              /* 动画区域左上角: 列86, 页1 (40x40) */
#define ANIM_PAGE                1

/* ===== 页面切换推屏动画 =====
 * 利用SSD1306显示起始行寄存器做硬件垂直滚动, 每个动画tick推一页(8行),
 * 8步完成, 共约320ms。方向与屏幕装配(A1/C8旋转)有关:
 * 若实机上新页面是从顶部推下来的, 把 UI_TRANS_REVERSE 改为 1 即可。 */
#define UI_TRANS_REVERSE         0

#endif /* __APP_CONFIG_H */
