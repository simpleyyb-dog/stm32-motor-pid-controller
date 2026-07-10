/**
  ******************************************************************************
  * @file    oled_ui.c
  * @brief   OLED 显示页面: 运行页 / 停止页 双界面, 切换带垂直推屏动画
  *
  * 运行页 (按加速/减速/启动后):
  *   左侧 0~83 列:                右侧 86~125 列 (页1~5):
  *     page 0  : Tar : 110         40x40 3D电机动画
  *     page 2~3: 112 rpm OK        转速越快转得越快
  *               (Real 12x16大字)  反转时反向旋转
  *     page 5  : Duty:  86 %
  *   page 7 整行: 转速条形图, 实心=Real, 竖线刻度=Tar, 满刻度=MOTOR_MAX_RPM
  *
  *   状态标记 (大字右侧):
  *     OK  = 实际转速进入目标 ±(5%+2rpm) 区间, PID 已锁定
  *     MAX = 占空比饱和仍达不到目标, 说明目标超出电机能力
  *
  * 停止页 (按停止后, 独立界面):
  *     page 2~3: 大字 "STOP" (12x16 居中)
  *     page 5  : Tar : 110 rpm  (保留的目标转速)
  *     page 6  : Dir : FWD      (停止时才能换向, 在此给反馈)
  *
  * 页面切换推屏: 利用SSD1306显示起始行寄存器做硬件垂直滚动。
  * 每个动画tick先把"即将从屏幕边缘进入的那一页GRAM"写成新页面内容,
  * 再把起始行移动8行, 新页面就一页一页推进来, 8步完成, 零整屏重发。
  * 过渡期间文字刷新和电机动画暂停, 结束后恢复正常局部刷新。
  ******************************************************************************
  */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "oled_ui.h"
#include "oled.h"
#include "app_config.h"
#include "anim_frames.h"

#define UI_LINE_MAX              22
#define UI_TEXT_WIDTH            84              /* 运行页文字区宽度, 86起是动画区 */
#define UI_TRANS_STEPS           8               /* 推屏总步数 = 8页 */

#define UI_BAR_PAGE              7               /* 转速条所在页, 整行128列 */
#define UI_BAR_FILL              0x7E            /* 实心段: 中间6行 */
#define UI_BAR_TRACK             0x18            /* 空段轨道: 中间2行细线 */

#define UI_JOG_BADGE_W           24              /* 点动徽标 "JOG " 宽度: 4字符x6px */
#define UI_JOG_BLINK_TICKS       8               /* 徽标脉冲周期: 每8个anim tick(40ms)翻转, 约1.5Hz */

/* 风扇粒子化: 给40x40动画叠径向抖动遮罩, 靠近转轴密、向外渐稀(粒子渐隐) */
#define UI_FAN_CENTER            20              /* 动画区中心(20,20) */
#define UI_FAN_LVL_MAX           16              /* 中心粒子密度档(=全亮) */
#define UI_FAN_RADIUS            22.0f           /* 密度衰减到0的半径 */

typedef enum
{
    UI_PAGE_NONE = 0,
    UI_PAGE_RUN,
    UI_PAGE_STOP
} UIPage;

static UIPage s_page = UI_PAGE_NONE;
static char s_line_cache[8][UI_LINE_MAX];
static float s_anim_phase = 0.0f;
static uint8_t s_anim_frame = 0;

/* 推屏过渡状态 */
static uint8_t s_trans_step = 0;                 /* 0=空闲, 1~8=进行中 */
static UIPage s_trans_target = UI_PAGE_NONE;

/* UI_Update 每次都会刷新的最新显示值, 过渡渲染也用它 */
static int16_t s_target_rpm = 0;
static float s_real_rpm = 0.0f;
static uint8_t s_duty = 0;
static uint8_t s_dir_fwd = 1;
static uint8_t s_jog = 0;             /* 运行页处于点动模式: 标题显示 JOG 而非 Tar */
static uint8_t s_jog_phase = 0;       /* 点动徽标脉冲相位: 0=正常, 1=反白 */
static uint8_t s_jog_tick = 0;        /* 点动徽标脉冲计时(累计 anim tick 数) */

/* 4x4 有序抖动(Bayer)阈值: 阈值低于密度档的像素才点亮, 用于把图形打散成粒子 */
static const uint8_t s_bayer4[4][4] =
{
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

/* 风扇粒子遮罩(页格式, 5页x40列): 上电时按径向密度生成一次, 之后与每帧图形按位与 */
static uint8_t s_fan_mask[ANIM_PAGES * ANIM_SIZE_PX];

/* 大字Real块与转速条的变化检测缓存 */
static int16_t s_blk_real = -1;
static char s_blk_marker[4] = "";
static int16_t s_bar_len = -1;
static int16_t s_bar_tick = -1;

static void UI_CacheClear(void)
{
    memset(s_line_cache, 0, sizeof(s_line_cache));
    s_blk_real = -1;
    s_blk_marker[0] = '\0';
    s_bar_len = -1;
    s_bar_tick = -1;
}

static void UI_WriteLine(uint8_t page, uint8_t x, uint8_t width, const char *str)
{
    if (strncmp(s_line_cache[page], str, UI_LINE_MAX - 1) != 0)
    {
        strncpy(s_line_cache[page], str, UI_LINE_MAX - 1);
        s_line_cache[page][UI_LINE_MAX - 1] = '\0';
        OLED_ShowLineRegion(page, x, width, str);
    }
}

/* PID 状态标记: MAX=输出饱和仍欠速(目标超能力), OK=进入目标区间 */
static const char *UI_Marker(void)
{
    float err = (float)s_target_rpm - s_real_rpm;
    float aerr = (err < 0.0f) ? -err : err;

    if (s_duty >= PWM_DUTY_MAX && err > (float)s_target_rpm * 0.08f + 3.0f)
    {
        return "MAX";
    }
    if (s_target_rpm > 0 && aerr <= (float)s_target_rpm * 0.05f + 2.0f)
    {
        return "OK";
    }
    return "";
}

/* 组装大字Real块(页2~3, 0~83列): 12x16数字 + 小字rpm + 状态标记 */
static void UI_ComposeReal(uint8_t *top, uint8_t *bot)
{
    char num[8];

    memset(top, 0, UI_TEXT_WIDTH);
    memset(bot, 0, UI_TEXT_WIDTH);

    sprintf(num, "%3d", (int)(s_real_rpm + 0.5f));
    OLED_RenderStrBig(top, bot, 0, num);             /* 0~35列 */
    OLED_RenderStr(bot, 40, 20, "rpm");
    OLED_RenderStr(top, 46, 38, UI_Marker());
}

/* 组装转速条(页7, 整行128列): 实心=Real, 全高竖线=Tar刻度 */
static void UI_ComposeBar(uint8_t *buf)
{
    int16_t len = (int16_t)(s_real_rpm * 128.0f / (float)MOTOR_MAX_RPM + 0.5f);
    int16_t tick = (int16_t)((float)s_target_rpm * 126.0f / (float)MOTOR_MAX_RPM + 0.5f);
    uint8_t x;

    if (len < 0) { len = 0; }
    if (len > 128) { len = 128; }
    if (tick < 0) { tick = 0; }
    if (tick > 126) { tick = 126; }

    for (x = 0; x < 128; x++)
    {
        buf[x] = (x < len) ? UI_BAR_FILL : UI_BAR_TRACK;
    }
    buf[tick] = 0xFF;
    buf[tick + 1] = 0xFF;
}

static void UI_DrawRealBlock(void)
{
    uint8_t blk[2 * UI_TEXT_WIDTH];
    int16_t real = (int16_t)(s_real_rpm + 0.5f);
    const char *mk = UI_Marker();

    if (real == s_blk_real && strcmp(mk, s_blk_marker) == 0)
    {
        return;                                  /* 没变化不占 I2C */
    }
    s_blk_real = real;
    strcpy(s_blk_marker, mk);

    UI_ComposeReal(&blk[0], &blk[UI_TEXT_WIDTH]);
    OLED_WriteRegion(2, 0, UI_TEXT_WIDTH, 2, blk);
}

static void UI_DrawBar(void)
{
    uint8_t buf[128];
    int16_t len = (int16_t)(s_real_rpm * 128.0f / (float)MOTOR_MAX_RPM + 0.5f);
    int16_t tick = (int16_t)((float)s_target_rpm * 126.0f / (float)MOTOR_MAX_RPM + 0.5f);

    if (len == s_bar_len && tick == s_bar_tick)
    {
        return;
    }
    s_bar_len = len;
    s_bar_tick = tick;

    UI_ComposeBar(buf);
    OLED_WritePage(UI_BAR_PAGE, buf);
}

/* 点动徽标(运行页左上角0~23列): "JOG" 在正常/反白间脉冲, 标识寸动激活 */
static void UI_DrawJogBadge(uint8_t invert)
{
    uint8_t buf[UI_JOG_BADGE_W];
    uint8_t i;

    OLED_RenderStr(buf, 0, UI_JOG_BADGE_W, "JOG ");
    if (invert)
    {
        for (i = 0; i < UI_JOG_BADGE_W; i++)
        {
            buf[i] = (uint8_t)~buf[i];           /* 反白: 黑底白字 -> 白底黑字 */
        }
    }
    OLED_WriteRegion(0, 0, UI_JOG_BADGE_W, 1, buf);
}

/* 上电生成风扇粒子遮罩: 每个像素按到中心的距离决定密度档, 距离越远档越低,
 * 再用Bayer抖动转成"是否保留"的位, 于是图形被打散成中心密、边缘稀的粒子 */
static void UI_FanMaskInit(void)
{
    uint8_t pg;
    uint8_t x;
    uint8_t b;

    for (pg = 0; pg < ANIM_PAGES; pg++)
    {
        for (x = 0; x < ANIM_SIZE_PX; x++)
        {
            uint8_t col = 0;

            for (b = 0; b < 8; b++)
            {
                uint8_t py = (uint8_t)(pg * 8 + b);
                float dx = (float)x - (float)UI_FAN_CENTER;
                float dy = (float)py - (float)UI_FAN_CENTER;
                float r = sqrtf(dx * dx + dy * dy);
                int level = (int)((float)UI_FAN_LVL_MAX * (1.0f - r / UI_FAN_RADIUS) + 0.5f);

                if (level > 0 && s_bayer4[py & 3][x & 3] < level)
                {
                    col |= (uint8_t)(1 << b);
                }
            }
            s_fan_mask[pg * ANIM_SIZE_PX + x] = col;
        }
    }
}

/* 取第 frame 帧并叠加粒子遮罩, 结果写入 out(ANIM_PAGES*ANIM_SIZE_PX 字节) */
static void UI_ComposeFan(uint8_t frame, uint8_t *out)
{
    const uint8_t *src = ANIM_FRAMES[frame];
    uint16_t i;

    for (i = 0; i < (uint16_t)(ANIM_PAGES * ANIM_SIZE_PX); i++)
    {
        out[i] = (uint8_t)(src[i] & s_fan_mask[i]);
    }
}

/* 渲染目标页面的第 p 页(8像素行)到128字节缓冲, 供推屏逐页送入GRAM */
static void UI_RenderPage(UIPage pg, uint8_t p, uint8_t *buf)
{
    char str[UI_LINE_MAX];

    memset(buf, 0, 128);

    if (pg == UI_PAGE_RUN)
    {
        switch (p)
        {
        case 0:
            sprintf(str, "%s: %3d", s_jog ? "JOG " : "Tar ", (int)s_target_rpm);
            OLED_RenderStr(buf, 0, UI_TEXT_WIDTH, str);
            break;
        case 2:
        case 3:
        {
            uint8_t top[UI_TEXT_WIDTH];
            uint8_t bot[UI_TEXT_WIDTH];

            UI_ComposeReal(top, bot);
            memcpy(buf, (p == 2) ? top : bot, UI_TEXT_WIDTH);
            break;
        }
        case 5:
            sprintf(str, "Duty: %3d %%", (int)s_duty);
            OLED_RenderStr(buf, 0, UI_TEXT_WIDTH, str);
            break;
        case UI_BAR_PAGE:
            UI_ComposeBar(buf);
            break;
        default:
            break;
        }

        if (p >= ANIM_PAGE && p < ANIM_PAGE + ANIM_PAGES)
        {
            uint16_t off = (uint16_t)(p - ANIM_PAGE) * ANIM_SIZE_PX;
            uint8_t col;

            for (col = 0; col < ANIM_SIZE_PX; col++)   /* 叠粒子遮罩 */
            {
                buf[ANIM_X + col] = (uint8_t)(ANIM_FRAMES[s_anim_frame][off + col]
                                              & s_fan_mask[off + col]);
            }
        }
    }
    else
    {
        switch (p)
        {
        case 2:
            OLED_RenderStrBig(buf, 0, (128 - 4 * 12) / 2, "STOP");
            break;
        case 3:
            OLED_RenderStrBig(0, buf, (128 - 4 * 12) / 2, "STOP");
            break;
        case 5:
            sprintf(str, "  Tar : %3d rpm", (int)s_target_rpm);
            OLED_RenderStr(buf, 0, 128, str);
            break;
        case 6:
            sprintf(str, "  Dir : %s", s_dir_fwd ? "FWD" : "REV");
            OLED_RenderStr(buf, 0, 128, str);
            break;
        default:
            break;
        }
    }
}

/* 推屏走一步: 先写入即将露出的GRAM页, 再移动显示起始行 */
static void UI_TransStep(void)
{
    uint8_t buf[128];
    uint8_t k = s_trans_step;
    uint8_t gram_page;
    uint8_t start_line;

#if UI_TRANS_REVERSE
    gram_page = (uint8_t)(UI_TRANS_STEPS - k);
    start_line = (uint8_t)((64 - 8 * k) & 0x3F);
#else
    gram_page = (uint8_t)(k - 1);
    start_line = (uint8_t)((8 * k) & 0x3F);
#endif

    UI_RenderPage(s_trans_target, gram_page, buf);
    OLED_WritePage(gram_page, buf);
    OLED_SetStartLine(start_line);

    if (++s_trans_step > UI_TRANS_STEPS)
    {
        s_trans_step = 0;                        /* 过渡完成, 起始行已回到0 */
        s_page = s_trans_target;
        UI_CacheClear();                         /* 下次文字刷新重建缓存(内容相同, 无闪烁) */
    }
}

/* 整屏直接画出页面(仅上电首次, 无过渡) */
static void UI_DrawPageFull(UIPage pg)
{
    uint8_t buf[128];
    uint8_t p;

    for (p = 0; p < 8; p++)
    {
        UI_RenderPage(pg, p, buf);
        OLED_WritePage(p, buf);
    }
    s_page = pg;
    UI_CacheClear();
}

void UI_Init(void)
{
    UI_CacheClear();
    UI_FanMaskInit();                /* 上电生成一次风扇粒子遮罩 */
    OLED_Init();
}

void UI_Update(int16_t target_rpm, float real_rpm, uint8_t duty,
               uint8_t running, uint8_t dir_fwd, uint8_t jog)
{
    char buf[UI_LINE_MAX];
    UIPage want = running ? UI_PAGE_RUN : UI_PAGE_STOP;

    s_target_rpm = target_rpm;
    s_real_rpm = real_rpm;
    s_duty = duty;
    s_dir_fwd = dir_fwd;
    if (jog && !s_jog)                            /* 进入点动: 徽标脉冲从正常相位重新计时 */
    {
        s_jog_phase = 0;
        s_jog_tick = 0;
    }
    s_jog = jog;

    if (s_trans_step != 0)
    {
        return;                                  /* 推屏中, 只更新数值不动屏 */
    }

    if (want != s_page)
    {
        if (s_page == UI_PAGE_NONE)
        {
            UI_DrawPageFull(want);               /* 上电首屏直接画 */
        }
        else
        {
            s_trans_target = want;               /* 启动推屏, 由动画tick推进 */
            s_trans_step = 1;
        }
        return;
    }

    if (s_page == UI_PAGE_RUN)
    {
        if (jog)
        {
            /* 徽标(0~23列)交给动画脉冲, 这里只刷新目标值(24列起), 避免两边抢同一区域 */
            sprintf(buf, ": %3d", (int)target_rpm);
            UI_WriteLine(0, UI_JOG_BADGE_W, UI_TEXT_WIDTH - UI_JOG_BADGE_W, buf);
        }
        else
        {
            sprintf(buf, "Tar : %3d", (int)target_rpm);
            UI_WriteLine(0, 0, UI_TEXT_WIDTH, buf);
        }

        UI_DrawRealBlock();

        sprintf(buf, "Duty: %3d %%", (int)duty);
        UI_WriteLine(5, 0, UI_TEXT_WIDTH, buf);

        UI_DrawBar();
    }
    else
    {
        sprintf(buf, "  Tar : %3d rpm", (int)target_rpm);
        UI_WriteLine(5, 0, 128, buf);

        sprintf(buf, "  Dir : %s", dir_fwd ? "FWD" : "REV");
        UI_WriteLine(6, 0, 128, buf);
    }
}

void UI_AnimTick(float real_rpm, uint8_t dir_fwd, uint32_t elapsed_ms)
{
    uint8_t frame;

    if (s_trans_step != 0)
    {
        UI_TransStep();                          /* 推屏优先, 期间电机动画暂停 */
        return;
    }

    if (s_page != UI_PAGE_RUN)
    {
        return;
    }

    /* 点动徽标脉冲: "JOG" 在正常/反白间闪动, 标识寸动模式正在生效 */
    if (s_jog && ++s_jog_tick >= UI_JOG_BLINK_TICKS)
    {
        s_jog_tick = 0;
        s_jog_phase ^= 1;
        UI_DrawJogBadge(s_jog_phase);
    }

    /* 相位累加: 帧频 = rpm * ANIM_FPS_PER_RPM, 反转时相位倒退 */
    {
        float step = real_rpm * ANIM_FPS_PER_RPM
                   * (float)elapsed_ms / 1000.0f;

        s_anim_phase += dir_fwd ? step : -step;
        while (s_anim_phase >= (float)ANIM_FRAME_COUNT)
        {
            s_anim_phase -= (float)ANIM_FRAME_COUNT;
        }
        while (s_anim_phase < 0.0f)
        {
            s_anim_phase += (float)ANIM_FRAME_COUNT;
        }
    }

    frame = (uint8_t)s_anim_phase;
    if (frame != s_anim_frame)                   /* 帧没变就不占 I2C */
    {
        uint8_t fbuf[ANIM_PAGES * ANIM_SIZE_PX];

        s_anim_frame = frame;
        UI_ComposeFan(frame, fbuf);              /* 叠粒子遮罩后再送屏 */
        OLED_WriteRegion(ANIM_PAGE, ANIM_X, ANIM_SIZE_PX, ANIM_PAGES, fbuf);
    }
}
