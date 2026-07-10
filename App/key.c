/**
  ******************************************************************************
  * @file    key.c
  * @brief   4x4矩阵键盘扫描与消抖
  *
  * 键盘编号关系:
  *   S1=R1C1, S2=R2C1, S3=R3C1, S4=R4C1
  *   S5=R1C2 ... S16=R4C4
  ******************************************************************************
  */
#include "key.h"
#include "app_config.h"

#define KEY_ROW_COUNT           4
#define KEY_COL_COUNT           4
#define KEY_COUNT               (KEY_ROW_COUNT * KEY_COL_COUNT)

typedef struct
{
    uint8_t stable;
    uint8_t cnt;
} KeyState;

static const uint16_t s_row_pins[KEY_ROW_COUNT] =
{
    KEY_R1_PIN, KEY_R2_PIN, KEY_R3_PIN, KEY_R4_PIN
};

static const uint16_t s_col_pins[KEY_COL_COUNT] =
{
    KEY_C1_PIN, KEY_C2_PIN, KEY_C3_PIN, KEY_C4_PIN
};

/* 数组按 R1C1, R1C2 ... R4C4 排列。当前只给 S1~S4 分配功能。 */
static const KeyEvent s_evt_map[KEY_COUNT] =
{
    KEY_EVENT_UP,         KEY_EVENT_NONE, KEY_EVENT_NONE, KEY_EVENT_NONE,
    KEY_EVENT_DOWN,       KEY_EVENT_NONE, KEY_EVENT_NONE, KEY_EVENT_NONE,
    KEY_EVENT_START_STOP, KEY_EVENT_NONE, KEY_EVENT_NONE, KEY_EVENT_NONE,
    KEY_EVENT_DIR,        KEY_EVENT_NONE, KEY_EVENT_NONE, KEY_EVENT_NONE
};

static KeyState s_keys[KEY_COUNT];

static void Key_Settle(void)
{
    volatile uint8_t i;

    for (i = 0; i < 30; i++)
    {
        __NOP();
    }
}

static void Key_ReadMatrix(uint8_t raw[KEY_COUNT])
{
    uint8_t row;
    uint8_t col;

    GPIO_SetBits(KEY_ROW_PORT, KEY_R1_PIN | KEY_R2_PIN | KEY_R3_PIN | KEY_R4_PIN);

    for (row = 0; row < KEY_ROW_COUNT; row++)
    {
        GPIO_ResetBits(KEY_ROW_PORT, s_row_pins[row]);
        Key_Settle();

        for (col = 0; col < KEY_COL_COUNT; col++)
        {
            raw[row * KEY_COL_COUNT + col] =
                (GPIO_ReadInputDataBit(KEY_COL_PORT, s_col_pins[col]) == Bit_RESET) ? 1 : 0;
        }

        GPIO_SetBits(KEY_ROW_PORT, s_row_pins[row]);
    }
}

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /* 行线开漏输出: 当前扫描行拉低，其余行释放，避免多键时输出互相冲突。 */
    GPIO_SetBits(KEY_ROW_PORT, KEY_R1_PIN | KEY_R2_PIN | KEY_R3_PIN | KEY_R4_PIN);
    GPIO_InitStructure.GPIO_Pin = KEY_R1_PIN | KEY_R2_PIN | KEY_R3_PIN | KEY_R4_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(KEY_ROW_PORT, &GPIO_InitStructure);

    /* 列线上拉输入，按下时由当前行拉成低电平。 */
    GPIO_InitStructure.GPIO_Pin = KEY_C1_PIN | KEY_C2_PIN | KEY_C3_PIN | KEY_C4_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(KEY_COL_PORT, &GPIO_InitStructure);
}

KeyEvent Key_Scan(void)
{
    uint8_t raw[KEY_COUNT];
    uint8_t i;
    KeyEvent evt = KEY_EVENT_NONE;

    Key_ReadMatrix(raw);

    for (i = 0; i < KEY_COUNT; i++)
    {
        if (raw[i] != s_keys[i].stable)
        {
            s_keys[i].cnt++;
            if (s_keys[i].cnt >= KEY_DEBOUNCE_COUNT)
            {
                s_keys[i].stable = raw[i];
                s_keys[i].cnt = 0;

                if (raw[i] && evt == KEY_EVENT_NONE)
                {
                    evt = s_evt_map[i];
                }
            }
        }
        else
        {
            s_keys[i].cnt = 0;
        }
    }

    return evt;
}

uint8_t Key_IsHeld(uint8_t index)
{
    /* 复用扫描已维护的消抖稳定态, 不重新读引脚; 调用前需先执行 Key_Scan() */
    return (index < KEY_COUNT) ? s_keys[index].stable : 0;
}
