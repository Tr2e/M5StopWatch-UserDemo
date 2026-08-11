#pragma once
/* Typhoon Compass — RGB565 palette (matches pages/colors_and_type.css) */

// StopWatch round panel is 466×466. StickS3 UI was authored at 240 — use tcU()
// to scale layout constants so we draw natively (no chunky upscale).
#define TC_REF_W      240
#define TC_SCREEN_W   466
#define TC_SCREEN_H   466
#define TC_FONT_SIZE  2
#define TC_CHAR_W     (6 * TC_FONT_SIZE)
#define TC_CHAR_H     (8 * TC_FONT_SIZE)

static inline constexpr int tcU(int v)
{
    return (v * TC_SCREEN_W + TC_REF_W / 2) / TC_REF_W;
}

#define TC_BLACK      0x0000
#define TC_WHITE      0xFFFF
#define TC_BG_DEEP    0x0803   // #050e1c
#define TC_OCEAN      0x0A15   // deep ocean fill
#define TC_PANEL_BG   0x1084   // panel/card
#define TC_TEXT       0xCE79   // primary text
#define TC_TEXT_DIM   0x6B6D   // secondary labels
#define TC_CYAN       0x4DFF   // accent #4dd0e1
#define TC_CYAN_DIM   0x2A93
#define TC_GREEN      0x07E0
#define TC_YELLOW     0xFFE0
#define TC_ORANGE     0xFD20
#define TC_RED        0xF800

/* Storm category colors (TD → SSTY) */
#define TC_CAT_TD     0x6B6D
#define TC_CAT_TS     0x2A93
#define TC_CAT_STS    0x4DFF
#define TC_CAT_TY     0xFFE0
#define TC_CAT_STY    0xFD20
#define TC_CAT_SSTY   0xF800
