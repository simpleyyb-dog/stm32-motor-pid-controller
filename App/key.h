/**
  ******************************************************************************
  * @file    key.h
  * @brief   4x4矩阵键盘扫描与消抖，S1~S4用于电机控制
  ******************************************************************************
  */
#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

typedef enum
{
    KEY_EVENT_NONE = 0,
    KEY_EVENT_UP,                    /* S1: 目标转速 + */
    KEY_EVENT_DOWN,                  /* S2: 目标转速 - */
    KEY_EVENT_START_STOP,            /* S3: 启动/停止 */
    KEY_EVENT_DIR                    /* S4: 方向切换 */
} KeyEvent;

void Key_Init(void);
KeyEvent Key_Scan(void);             /* 每 10ms 调用一次, 返回按下事件(一次按下只触发一次) */
uint8_t  Key_IsHeld(uint8_t index);  /* 指定矩阵索引按键当前消抖电平: 按住=1, 松开=0; 须在Key_Scan之后读取 */

#endif /* __KEY_H */
