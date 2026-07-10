/**
  ******************************************************************************
  * @file    oled.c
  * @brief   SSD1306 OLED 驱动, 硬件 I2C1 + 超时保护
  *
  * 重要: F103 硬件 I2C 有 BUSY 标志卡死的勘误问题。所有等待循环都带
  * 超时退出, 超时后软复位 I2C 外设并放弃本次传输, 由下个刷新周期重试,
  * 保证 OLED 异常(接触不良/掉线)时不会拖死电机控制主循环。
  ******************************************************************************
  */
#include <string.h>
#include "oled.h"
#include "app_config.h"
#include "systick.h"
#include "oled_font.h"

#define OLED_I2C_TIMEOUT         20000           /* 等待循环超时次数 */

static void OLED_I2C_Config(void)
{
    I2C_InitTypeDef I2C_InitStructure;

    I2C_DeInit(I2C1);
    I2C_InitStructure.I2C_ClockSpeed = 400000;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitStructure);
    I2C_Cmd(I2C1, ENABLE);
}

static void OLED_I2C_Recover(void)
{
    I2C_GenerateSTOP(I2C1, ENABLE);
    I2C_SoftwareResetCmd(I2C1, ENABLE);
    I2C_SoftwareResetCmd(I2C1, DISABLE);
    OLED_I2C_Config();
}

static uint8_t OLED_WaitEvent(uint32_t event)
{
    uint32_t t = OLED_I2C_TIMEOUT;

    while (I2C_CheckEvent(I2C1, event) != SUCCESS)
    {
        if (--t == 0)
        {
            return 1;
        }
    }
    return 0;
}

/* control: 0x00=命令流, 0x40=数据流; 返回0成功 */
static uint8_t OLED_WriteBytes(uint8_t control, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint32_t t = OLED_I2C_TIMEOUT;

    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) == SET)
    {
        if (--t == 0)
        {
            OLED_I2C_Recover();
            return 1;
        }
    }

    I2C_GenerateSTART(I2C1, ENABLE);
    if (OLED_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT))
    {
        goto fail;
    }

    I2C_Send7bitAddress(I2C1, OLED_ADDR, I2C_Direction_Transmitter);
    if (OLED_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        goto fail;
    }

    I2C_SendData(I2C1, control);
    if (OLED_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING))
    {
        goto fail;
    }

    for (i = 0; i < len; i++)
    {
        I2C_SendData(I2C1, data[i]);
        if (OLED_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING))
        {
            goto fail;
        }
    }

    if (OLED_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        goto fail;
    }
    I2C_GenerateSTOP(I2C1, ENABLE);
    return 0;

fail:
    I2C_GenerateSTOP(I2C1, ENABLE);
    return 1;
}

static uint8_t OLED_WriteCmd(uint8_t cmd)
{
    return OLED_WriteBytes(0x00, &cmd, 1);
}

static void OLED_SetCursor(uint8_t page, uint8_t x)
{
    OLED_WriteCmd(0xB0 | (page & 0x07));
    OLED_WriteCmd(0x00 | (x & 0x0F));
    OLED_WriteCmd(0x10 | ((x >> 4) & 0x0F));
}

uint8_t OLED_Init(void)
{
    static const uint8_t init_cmds[] =
    {
        0xAE,                        /* 关显示 */
        0xD5, 0x80,                  /* 时钟分频 */
        0xA8, 0x3F,                  /* 1/64 占空 */
        0xD3, 0x00,                  /* 显示偏移 0 */
        0x40,                        /* 起始行 0 */
        0x8D, 0x14,                  /* 电荷泵开 */
        0x20, 0x02,                  /* 页寻址模式 */
        0xA1,                        /* 段重映射 */
        0xC8,                        /* COM 反向扫描 */
        0xDA, 0x12,                  /* COM 引脚配置 */
        0x81, 0xCF,                  /* 对比度 */
        0xD9, 0xF1,                  /* 预充电周期 */
        0xDB, 0x30,                  /* VCOMH */
        0xA4,                        /* 显示跟随 RAM */
        0xA6,                        /* 正常显示(非反色) */
        0xAF                         /* 开显示 */
    };
    GPIO_InitTypeDef GPIO_InitStructure;
    uint8_t err = 0;
    uint8_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    /* PB6=SCL, PB7=SDA 复用开漏 (OLED 模块板载上拉) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    OLED_I2C_Config();

    Delay_ms(100);                   /* SSD1306 上电稳定时间 */

    for (i = 0; i < sizeof(init_cmds); i++)
    {
        err |= OLED_WriteCmd(init_cmds[i]);
    }
    OLED_Clear();
    return err;
}

void OLED_Clear(void)
{
    uint8_t zeros[128] = {0};
    uint8_t page;

    for (page = 0; page < 8; page++)
    {
        OLED_SetCursor(page, 0);
        OLED_WriteBytes(0x40, zeros, 128);
    }
}

void OLED_RenderStr(uint8_t *line, uint8_t x, uint8_t width, const char *str)
{
    uint16_t pos = x;
    uint16_t end;

    if (x >= 128)
    {
        return;
    }
    if (width > 128 - x)
    {
        width = 128 - x;
    }
    end = (uint16_t)(x + width);

    while (*str != '\0' && (pos + 6) <= end)
    {
        uint8_t c = (uint8_t)*str;
        uint8_t k;

        if (c < ' ' || c > 'z')
        {
            c = ' ';                 /* 字库只覆盖 ASCII 32~122 */
        }
        for (k = 0; k < 6; k++)
        {
            line[pos++] = OLED_F6x8[c - ' '][k];
        }
        str++;
    }
    while (pos < end)
    {
        line[pos++] = 0x00;          /* 区域尾部清空, 避免残影 */
    }
}

void OLED_ShowLineRegion(uint8_t page, uint8_t x, uint8_t width, const char *str)
{
    uint8_t buf[128];

    if (x >= 128)
    {
        return;
    }
    if (width > 128 - x)
    {
        width = 128 - x;
    }

    OLED_RenderStr(buf, 0, width, str);
    OLED_SetCursor(page, x);
    OLED_WriteBytes(0x40, buf, width);
}

void OLED_ShowLine(uint8_t page, const char *str)
{
    OLED_ShowLineRegion(page, 0, 128, str);
}

/* 把字节的低/高4位各自按位拉伸成8位, 用于字体纵向2倍放大 */
static uint8_t OLED_StretchNibble(uint8_t b)
{
    uint8_t out = 0;
    uint8_t i;

    for (i = 0; i < 4; i++)
    {
        if (b & (1 << i))
        {
            out |= (uint8_t)(0x03 << (i * 2));
        }
    }
    return out;
}

void OLED_RenderStrBig(uint8_t *top, uint8_t *bot, uint8_t x, const char *str)
{
    uint16_t pos = x;

    while (*str != '\0' && (pos + 12) <= 128)
    {
        uint8_t c = (uint8_t)*str;
        uint8_t k;

        if (c < ' ' || c > 'z')
        {
            c = ' ';
        }
        for (k = 0; k < 6; k++)
        {
            uint8_t col = OLED_F6x8[c - ' '][k];
            uint8_t t = OLED_StretchNibble(col & 0x0F);
            uint8_t b = OLED_StretchNibble((uint8_t)(col >> 4));
            uint8_t r;

            for (r = 0; r < 2; r++)              /* 横向同列重复 = 2倍宽 */
            {
                if (top != 0) { top[pos] = t; }
                if (bot != 0) { bot[pos] = b; }
                pos++;
            }
        }
        str++;
    }
}

void OLED_ShowStrBig(uint8_t page, uint8_t x, const char *str)
{
    uint8_t top[128];
    uint8_t bot[128];
    uint16_t len;

    OLED_RenderStrBig(top, bot, x, str);
    len = (uint16_t)(strlen(str) * 12);
    if (len > (uint16_t)(128 - x))
    {
        len = (uint16_t)(128 - x);
    }

    OLED_SetCursor(page, x);
    OLED_WriteBytes(0x40, &top[x], len);
    OLED_SetCursor((uint8_t)(page + 1), x);
    OLED_WriteBytes(0x40, &bot[x], len);
}

void OLED_WriteRegion(uint8_t page, uint8_t x, uint8_t width,
                      uint8_t pages, const uint8_t *data)
{
    uint8_t p;

    for (p = 0; p < pages; p++)
    {
        OLED_SetCursor((uint8_t)(page + p), x);
        OLED_WriteBytes(0x40, &data[(uint16_t)p * width], width);
    }
}

void OLED_WritePage(uint8_t page, const uint8_t *buf)
{
    OLED_SetCursor(page, 0);
    OLED_WriteBytes(0x40, buf, 128);
}

void OLED_SetStartLine(uint8_t line)
{
    OLED_WriteCmd((uint8_t)(0x40 | (line & 0x3F)));
}
