/**
  ******************************************************************************
  * @file    oled.h
  * @brief   SSD1306 0.96寸 OLED 驱动 (硬件 I2C1, PB6=SCL, PB7=SDA)
  ******************************************************************************
  */
#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

uint8_t OLED_Init(void);                         /* 返回0成功, 非0表示I2C通信失败 */
void OLED_Clear(void);
void OLED_ShowLine(uint8_t page, const char *str);  /* 整行显示: page 0~7, 6x8字体最多21字符, 行尾自动清空 */
void OLED_ShowLineRegion(uint8_t page, uint8_t x, uint8_t width,
                         const char *str);       /* 区域内显示一行, 区域尾部自动清空 */
void OLED_ShowStrBig(uint8_t page, uint8_t x, const char *str);  /* 12x16 大字(6x8放大2倍), 占2页 */
void OLED_WriteRegion(uint8_t page, uint8_t x, uint8_t width,
                      uint8_t pages, const uint8_t *data);  /* 写矩形位图区域(页格式, 逐页连续) */
void OLED_RenderStr(uint8_t *line, uint8_t x, uint8_t width,
                    const char *str);            /* 渲染6x8文字到128列页缓冲(不发I2C) */
void OLED_RenderStrBig(uint8_t *top, uint8_t *bot, uint8_t x,
                       const char *str);         /* 渲染12x16大字到上下两页缓冲, 任一可为NULL */
void OLED_WritePage(uint8_t page, const uint8_t *buf);  /* 整页128字节写入 */
void OLED_SetStartLine(uint8_t line);            /* 显示起始行0~63, 硬件垂直滚动 */

#endif /* __OLED_H */
