# STM32 DC Motor PID Speed Controller

基于 **STM32F103C8T6** 的直流减速电机闭环调速系统。

项目使用 **TIM2 正交编码器模式**采集电机转速，通过 **前馈 + PI(D) 闭环控制**实时调节 PWM，占空比驱动 L298N，实现目标转速跟踪、平滑启停、正反转、点动控制以及 OLED 实时数据显示。

---

## 项目简介

本项目以 JGA25-370 编码器直流减速电机为控制对象，通过 STM32F103C8T6 完成：

- 编码器测速
- PWM 电机驱动
- PI/PID 闭环调速
- 前馈补偿
- 抗积分饱和
- 目标转速斜坡
- 正反转控制
- 点动 Jog 控制
- OLED 状态显示
- 4×4 矩阵键盘输入
- 非阻塞式周期任务调度

控制系统核心流程：

```text
       Target RPM
           │
           ▼
    Setpoint Ramp
           │
           ▼
   Feedforward + PI(D)
           │
           ▼
      PWM Duty
           │
           ▼
        L298N
           │
           ▼
      DC Motor
           │
           ▼
       Encoder
           │
           ▼
 TIM2 Encoder Interface
           │
           └──────── Feedback ────────┘
```

---

## 1. Hardware

| Component | Model |
|---|---|
| MCU | STM32F103C8T6 |
| Motor | JGA25-370 DC Gear Motor |
| Motor Driver | L298N |
| Encoder | A/B Quadrature Encoder |
| Display | SSD1306 0.96" OLED |
| Input | 4×4 Matrix Keypad |
| IDE | Keil MDK |

### Pin Assignment

#### Motor Driver

| Function | STM32 Pin | Peripheral |
|---|---|---|
| PWM / ENB | PA6 | TIM3_CH1 |
| IN3 | PB12 | GPIO |
| IN4 | PB13 | GPIO |

> 使用 PWM 控制 ENB 时，需要拔掉 L298N 模块上的 ENB 跳帽。

#### Encoder

| Function | STM32 Pin | Peripheral |
|---|---|---|
| Encoder A | PA0 | TIM2_CH1 |
| Encoder B | PA1 | TIM2_CH2 |

TIM2 工作于 **Encoder Mode TI12**，利用 A/B 两相信号进行正交计数。

#### OLED

| Function | STM32 Pin |
|---|---|
| SCL | PB6 |
| SDA | PB7 |

通信方式：I2C1  
OLED：SSD1306，128×64，7 位地址 0x3C。

#### 4×4 Matrix Keypad

Rows：

```text
R1 -> PB0
R2 -> PB1
R3 -> PB10
R4 -> PB11
```

Columns：

```text
C1 -> PA8
C2 -> PA9
C3 -> PA10
C4 -> PA11
```

| Key | Function |
|---|---|
| S1 | Increase target RPM |
| S2 | Decrease target RPM |
| S3 | Start / Stop |
| S4 | Change direction |
| S5 | Forward Jog |
| S6 | Reverse Jog |

---

## 2. Control Algorithm

控制器采用：

```text
Feedforward + PI(D)
```

整体输出：

```text
u = u_ff + u_PID
```

其中：

```text
u_PID = Kp·e + Ki·∫e dt + Kd·de/dt
```

误差：

```text
e = target_rpm - real_rpm
```

### Feedforward Control

为了减少 PID 需要补偿的误差，控制器根据目标转速预先估计基础 PWM：

```text
u_ff = u_min + target_rpm / RPM_max × (u_max - u_min)
```

PID 主要用于修正：

- 电源变化
- 电机负载变化
- 摩擦
- 模型误差
- 驱动器压降

相比完全依赖积分项逐渐提高 PWM，前馈能够改善目标转速附近的响应速度。

---

## 3. Anti-Windup

项目实现了多种抗积分饱和机制。

### Integral Limit

积分项限制在 `PID_INTEGRAL_MAX` 范围内，防止积分持续累积。

### Conditional Integration

当 PWM 已达到输出上限，同时误差仍要求继续增大 PWM 时，不继续累加积分。

例如：

```text
PWM = 100%
Error > 0
```

此时继续积分已经不能增加实际执行器输出，因此保持原积分值。

### Setpoint Ramp

目标值不会瞬间从 0 RPM 跳变到目标 RPM，而是按照设定斜率逐渐逼近。

当前：

```c
#define MOTOR_ACCEL_RPM_S 45.0f
```

目标斜坡可以减小：

- 启动冲击
- 转速超调
- PID 积分堆积

---

## 4. Speed Measurement

编码器测速采用 TIM2 硬件 Encoder Mode。

输出轴转速计算：

```text
RPM = ΔCount × 60000 /
      (PPR × GearRatio × Quadrature × Δt_ms)
```

当前主要参数：

```c
#define ENCODER_PPR_MOTOR   11
#define GEAR_RATIO          18.4f
#define ENCODER_QUADRATURE  4
```

因此输出轴每转对应的理论计数为：

```text
11 × 18.4 × 4
```

程序使用实际经过的 `elapsed_ms` 计算转速，而不是假设任务周期绝对固定。

---

## 5. Speed Filtering

控制器内部使用实时测速值参与闭环控制。

OLED 显示值另外进行一阶低通滤波：

```text
y[k] = α·y[k-1] + (1-α)·x[k]
```

当前：

```c
#define SPEED_FILTER_ALPHA 0.85f
```

这样可以减小显示跳动，同时避免给 PID 控制回路额外增加较大的滤波延迟。

---

## 6. PWM

PWM 由 TIM3_CH1 / PA6 输出。

当前配置：

```text
System Clock : 72 MHz
PSC          : 720 - 1
ARR          : 100 - 1
```

因此：

```text
PWM Frequency = 72 MHz / 720 / 100 = 1 kHz
```

占空比范围：

```text
0 ~ 100 %
```

---

## 7. Control Period

当前 PID 控制周期：

```c
#define PID_CONTROL_PERIOD_MS 50
```

即约 20 Hz。

一次控制循环主要完成：

```text
Read Encoder
     ↓
Calculate RPM
     ↓
Update Setpoint Ramp
     ↓
Feedforward + PI(D)
     ↓
Update PWM
```

---

## 8. Non-blocking Scheduler

主循环采用 `millis()` 进行多个周期任务调度，没有依赖大量阻塞式 `Delay_ms()`。

| Task | Period |
|---|---:|
| Key Scan | 10 ms |
| Speed + PID | 50 ms |
| OLED UI | 200 ms |
| Motor Animation | 40 ms |

典型结构：

```c
if ((uint32_t)(now - last_task) >= task_period)
{
    last_task = now;
    Task_Update();
}
```

这种方式可以让按键、控制、显示和动画并行推进，提高系统响应性。

---

## 9. Direction Protection

为了避免电机运行过程中直接反接 H 桥造成较大的瞬时电流，程序限制运行状态下直接换向。

```text
Motor Running
     ↓
Direction Change Disabled
```

只有在停止状态下才允许修改电机方向。

---

## 10. Jog Mode

系统支持点动控制。

```text
Hold S5 -> Forward Jog
Release -> Stop

Hold S6 -> Reverse Jog
Release -> Stop
```

点动模式同样使用闭环速度控制以及目标转速斜坡。

当 S5 与 S6 同时按下时，点动无效，用于避免正反转指令冲突。

---

## 11. OLED Interface

OLED 显示内容包括：

- Target RPM
- Real RPM
- PWM Duty
- Motor State
- Motor Direction
- Jog State

运行页面还包含简单的电机 / 风扇旋转动画，动画速度根据实际转速进行调整。

---

## 12. Project Structure

```text
stm32-motor-pid-controller
│
├── App
│   ├── app_config.h
│   ├── motor.c / motor.h
│   ├── speed.c / speed.h
│   ├── pid.c / pid.h
│   ├── key.c / key.h
│   ├── oled.c / oled.h
│   ├── oled_ui.c / oled_ui.h
│   ├── systick.c / systick.h
│   └── anim_frames.h
│
├── User
│   ├── main.c
│   ├── stm32f10x_conf.h
│   └── stm32f10x_it.*
│
├── Library
│   └── STM32F10x Standard Peripheral Library
│
├── Start
│   └── Cortex-M3 startup files
│
├── Tools
│   └── gen_anim.ps1
│
├── docs
├── Project.uvprojx
└── README.md
```

---

## 13. Software Architecture

### `motor.c`

负责 PWM、电机方向、启动和停止。

### `speed.c`

负责编码器初始化、计数读取、RPM 计算和显示滤波。

### `pid.c`

负责前馈、PI/PID、积分限幅、Anti-Windup 和输出饱和。

### `key.c`

负责 4×4 矩阵键盘扫描、消抖、按键事件和点动键状态。

### `oled.c`

负责 SSD1306 底层驱动。

### `oled_ui.c`

负责 UI 渲染、RPM 显示、运行状态和动画。

### `app_config.h`

集中管理 Pin Mapping、PWM、电机、编码器、PID、键盘、OLED 和动画参数。

---

## 14. Current Control Parameters

```c
#define MOTOR_DEFAULT_RPM  90
#define MOTOR_MAX_RPM      220
#define MOTOR_ACCEL_RPM_S  45.0f
```

当前 PI(D) 参数：

```c
#define PID_KP 1.00f
#define PID_KI 0.80f
#define PID_KD 0.00f
```

当前优先使用 **Feedforward + PI**。

由于编码器测速存在量化误差、电机机械振动和电磁干扰，微分项容易放大高频噪声，因此默认 `Kd = 0`。

---

## 15. PID Tuning

建议调参顺序：

```text
Kp
 ↓
Ki
 ↓
Feedforward calibration
 ↓
Ramp rate
 ↓
Optional Kd
```

对于当前编码器转速控制系统，一般使用 `Feedforward + PI` 已经能够获得较好的控制效果。

---

## 16. Motor Calibration

`MOTOR_MAX_RPM` 不建议直接填写电机铭牌额定转速。

L298N 存在较明显的导通压降，因此电机实际端电压会低于电源电压。

推荐标定流程：

1. 将 PWM 提升至 100%
2. 等待电机转速稳定
3. 读取 OLED 实际 RPM
4. 将稳定转速填入 `MOTOR_MAX_RPM`

这样可以让前馈模型更加接近真实系统。

---

## 17. Build

开发环境：

```text
Keil MDK
STM32F10x Standard Peripheral Library
```

打开：

```text
Project.uvprojx
```

然后：

```text
Build
  ↓
Flash
  ↓
Run
```

---

## 18. Future Improvements

- [ ] 串口实时输出 RPM / PWM / Error
- [ ] MATLAB / Python 绘制阶跃响应曲线
- [ ] 自动记录 PID 调参数据
- [ ] 测量超调量和调节时间
- [ ] 增加负载扰动实验
- [ ] 优化低速死区补偿
- [ ] 增加主动制动策略
- [ ] 增加串口在线修改 PID 参数
- [ ] 增加 Flash 参数保存
- [ ] 增加故障保护
- [ ] 增加电流检测
- [ ] 更换低压降 MOSFET H-Bridge 驱动器

---

## 19. Control Performance

后续建议记录典型阶跃响应，例如：

```text
0 -> 90 RPM
```

并测量：

- Rise Time
- Overshoot
- Settling Time
- Steady-State Error

同时可以进行负载扰动实验：

```text
Stable Speed
     ↓
Apply Mechanical Load
     ↓
RPM Drops
     ↓
PI Increases PWM
     ↓
RPM Returns to Target
```

用于验证闭环控制系统的抗扰性能。

---

## About

This project is intended for learning and practicing:

- Embedded Systems
- STM32
- Motor Control
- PID Control
- Encoder Feedback
- Real-Time Control
- Control Engineering

The project focuses not only on making the motor rotate, but on building a complete closed-loop control system from sensing, control algorithm, actuator output to user interaction and visualization.
