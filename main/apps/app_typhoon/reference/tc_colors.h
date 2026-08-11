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
// Muted nautical instrument palette.  The AMOLED panel makes saturated blues
// appear much stronger than on a desktop preview, so information hierarchy is
// carried by luminance and contrast rather than by neon colour.
#define TC_BG_DEEP    0x0862   // near-black graphite navy
#define TC_OCEAN      0x1105   // desaturated blue-grey sea
#define TC_PANEL_BG   0x1906   // charcoal slate panel
#define TC_TEXT       0xD6BA   // soft warm white
#define TC_TEXT_DIM   0x7BEF   // neutral secondary text
#define TC_CYAN       0x4CF4   // restrained instrument teal
#define TC_CYAN_DIM   0x22EC   // dim teal / guides
#define TC_GREEN      0x452F   // muted confirmation mint
#define TC_YELLOW     0xD588   // amber advisory
#define TC_ORANGE     0xE3A9   // storm warning orange
#define TC_RED        0xCA48   // storm warning red

/* Storm category colors (TD → SSTY) */
#define TC_CAT_TD     0x6B6D
#define TC_CAT_TS     0x2A93
#define TC_CAT_STS    0x4DFF
#define TC_CAT_TY     0xFFE0
#define TC_CAT_STY    0xFD20
#define TC_CAT_SSTY   0xF800
