/**
  ******************************************************************************
  * @file    oled_ui.h
  * @brief   OLED 显示页面: 目标转速/实际转速/占空比/运行状态/方向
  ******************************************************************************
  */
#ifndef __OLED_UI_H
#define __OLED_UI_H

#include "stm32f10x.h"

void UI_Init(void);
void UI_Update(int16_t target_rpm, float real_rpm, uint8_t duty,
               uint8_t running, uint8_t dir_fwd, uint8_t jog);  /* jog=1: 运行页标题显示 JOG */
void UI_AnimTick(float real_rpm, uint8_t dir_fwd, uint32_t elapsed_ms);  /* 每ANIM_TICK_MS调用, 推进风扇动画 */

#endif /* __OLED_UI_H */
