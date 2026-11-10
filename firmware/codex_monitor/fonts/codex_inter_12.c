/*******************************************************************************
 * Size: 12 px
 * Bpp: 2
 * Opts: --font .state/Inter-Black.ttf --size 12 --bpp 2 --format lvgl --symbols ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789% /:.-!dhm --no-compress -o firmware/codex_monitor/fonts/codex_inter_12.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef CODEX_INTER_12
#define CODEX_INTER_12 1
#endif

#if CODEX_INTER_12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0xbe, 0xfb, 0xde, 0x79, 0xd7, 0x9e,

    /* U+0025 "%" */
    0x2f, 0x40, 0xd0, 0x3a, 0xc3, 0x80, 0x36, 0xcb,
    0x0, 0x2f, 0x9c, 0x0, 0x0, 0x35, 0xf8, 0x0,
    0xe3, 0xdd, 0x2, 0xc3, 0xdd, 0x7, 0x1, 0xf8,

    /* U+002D "-" */
    0x2a, 0x9f, 0xf0,

    /* U+002E "." */
    0x12, 0xe7, 0x80,

    /* U+002F "/" */
    0x1, 0xe0, 0x2d, 0x3, 0xc0, 0x3c, 0x7, 0x80,
    0xf0, 0xf, 0x1, 0xe0, 0x2d, 0x3, 0xc0,

    /* U+0030 "0" */
    0xb, 0xf4, 0x3f, 0xfd, 0xbc, 0x2e, 0xb8, 0x1f,
    0xb8, 0x1f, 0xbc, 0x2e, 0x3f, 0xfd, 0xb, 0xf4,

    /* U+0031 "1" */
    0x2f, 0x7f, 0xd9, 0xf4, 0x3d, 0xf, 0x43, 0xd0,
    0xf4, 0x3d,

    /* U+0032 "2" */
    0x1f, 0xe0, 0x7f, 0xfc, 0xb8, 0x3c, 0x0, 0xbc,
    0x3, 0xf0, 0x1f, 0x80, 0x7f, 0xfc, 0xbf, 0xfc,

    /* U+0033 "3" */
    0x1f, 0xe0, 0x7e, 0xfc, 0x14, 0x3c, 0x3, 0xf4,
    0x2, 0xfc, 0xb8, 0x3d, 0x7f, 0xfc, 0x1f, 0xe0,

    /* U+0034 "4" */
    0x1, 0xfc, 0x3, 0xfc, 0xf, 0xbc, 0x2e, 0x7c,
    0x7c, 0x7c, 0xbf, 0xff, 0x6a, 0xfe, 0x0, 0x7c,

    /* U+0035 "5" */
    0x7f, 0xf8, 0x7f, 0xf8, 0xbf, 0xe0, 0xbe, 0xfc,
    0x14, 0x3c, 0xb8, 0x3c, 0x7f, 0xf8, 0x1f, 0xe0,

    /* U+0036 "6" */
    0xb, 0xf4, 0x3f, 0xfc, 0xb9, 0xa4, 0xbf, 0xfc,
    0xbc, 0x3d, 0xbc, 0x3d, 0x3f, 0xfc, 0xb, 0xf0,

    /* U+0037 "7" */
    0xff, 0xfa, 0xff, 0xe0, 0xf, 0x40, 0xbc, 0x3,
    0xc0, 0x3e, 0x1, 0xf0, 0xf, 0x80,

    /* U+0038 "8" */
    0x1f, 0xf4, 0x7e, 0xfc, 0x7c, 0x7c, 0x1f, 0xf4,
    0x7e, 0xfc, 0xbc, 0x3d, 0xbe, 0xfd, 0x2f, 0xf4,

    /* U+0039 "9" */
    0x1f, 0xe0, 0x7f, 0xfc, 0xb8, 0x3d, 0xbe, 0xfd,
    0x2f, 0xbd, 0x64, 0x3d, 0x3f, 0xfc, 0x1f, 0xe0,

    /* U+003A ":" */
    0x7a, 0xe1, 0x4, 0xb9, 0xe0,

    /* U+0041 "A" */
    0x7, 0xf4, 0x0, 0xbf, 0xc0, 0xf, 0xbc, 0x1,
    0xf3, 0xe0, 0x2f, 0x2f, 0x3, 0xff, 0xf4, 0x7f,
    0xff, 0x8f, 0xc0, 0xbc,

    /* U+0042 "B" */
    0xbf, 0xf4, 0xbe, 0xfc, 0xbc, 0x7c, 0xbf, 0xf4,
    0xbe, 0xbc, 0xbc, 0x3e, 0xbe, 0xfd, 0xbf, 0xf8,

    /* U+0043 "C" */
    0xb, 0xf8, 0xf, 0xff, 0xcb, 0xd1, 0xf6, 0xf0,
    0x0, 0xbc, 0x0, 0x2f, 0x47, 0xd3, 0xff, 0xf0,
    0x2f, 0xe0,

    /* U+0044 "D" */
    0xbf, 0xf4, 0x2f, 0xff, 0x4b, 0xc2, 0xf2, 0xf0,
    0x3d, 0xbc, 0xf, 0x6f, 0xb, 0xcb, 0xff, 0xd2,
    0xff, 0xd0,

    /* U+0045 "E" */
    0xbf, 0xfc, 0xbf, 0xfc, 0xbc, 0x0, 0xbf, 0xfc,
    0xbf, 0xfc, 0xbc, 0x0, 0xbf, 0xfc, 0xbf, 0xfc,

    /* U+0046 "F" */
    0xbf, 0xfc, 0xbf, 0xfc, 0xbc, 0x0, 0xbf, 0xf8,
    0xbf, 0xfc, 0xbc, 0x0, 0xbc, 0x0, 0xbc, 0x0,

    /* U+0047 "G" */
    0xb, 0xf8, 0xf, 0xff, 0xcb, 0xd0, 0xf6, 0xf1,
    0xa9, 0xbc, 0x7f, 0xaf, 0x43, 0xd3, 0xff, 0xf0,
    0x2f, 0xe0,

    /* U+0048 "H" */
    0xbc, 0xf, 0x6f, 0x3, 0xdb, 0xc0, 0xf6, 0xff,
    0xfd, 0xbf, 0xff, 0x6f, 0x3, 0xdb, 0xc0, 0xf6,
    0xf0, 0x3d,

    /* U+0049 "I" */
    0xbe, 0xfb, 0xef, 0xbe, 0xfb, 0xef,

    /* U+004A "J" */
    0x0, 0xf8, 0x3, 0xe0, 0xf, 0x80, 0x3e, 0x10,
    0xfa, 0xe3, 0xe7, 0xff, 0x4b, 0xf4,

    /* U+004B "K" */
    0xbc, 0x2f, 0x2f, 0x1f, 0x4b, 0xdf, 0x42, 0xff,
    0x80, 0xbf, 0xf4, 0x2f, 0x6f, 0xb, 0xc3, 0xe2,
    0xf0, 0x7d,

    /* U+004C "L" */
    0xbc, 0x2, 0xf0, 0xb, 0xc0, 0x2f, 0x0, 0xbc,
    0x2, 0xf0, 0xb, 0xff, 0xaf, 0xfe,

    /* U+004D "M" */
    0xbf, 0x43, 0xfa, 0xfe, 0x1f, 0xeb, 0xfc, 0xff,
    0xaf, 0xf7, 0xee, 0xbe, 0xfe, 0xfa, 0xf3, 0xf7,
    0xeb, 0xcf, 0xcf, 0xaf, 0x2e, 0x3e,

    /* U+004E "N" */
    0xbd, 0xf, 0x6f, 0xc3, 0xdb, 0xf8, 0xf6, 0xff,
    0x7d, 0xbd, 0xff, 0x6f, 0x3f, 0xdb, 0xc3, 0xf6,
    0xf0, 0x7d,

    /* U+004F "O" */
    0xb, 0xf8, 0xf, 0xff, 0xcb, 0xd1, 0xf6, 0xf0,
    0x3e, 0xbc, 0xf, 0xaf, 0x47, 0xd3, 0xff, 0xf0,
    0x2f, 0xe0,

    /* U+0050 "P" */
    0xbf, 0xf4, 0xbf, 0xfc, 0xbc, 0x3e, 0xbc, 0x3e,
    0xbf, 0xfc, 0xbf, 0xe0, 0xbc, 0x0, 0xbc, 0x0,

    /* U+0051 "Q" */
    0xb, 0xf8, 0xf, 0xff, 0xcb, 0xd1, 0xf6, 0xf0,
    0x3e, 0xbc, 0xf, 0xaf, 0x7f, 0xd3, 0xff, 0xf0,
    0x2f, 0xfc, 0x0, 0x6, 0x0,

    /* U+0052 "R" */
    0xbf, 0xf4, 0xbf, 0xfc, 0xbc, 0x3e, 0xbc, 0x3e,
    0xbf, 0xfc, 0xbf, 0xf4, 0xbc, 0xbc, 0xbc, 0x3e,

    /* U+0053 "S" */
    0x1f, 0xf4, 0x3f, 0xfd, 0x7c, 0x19, 0x3f, 0xe4,
    0x6, 0xfe, 0x78, 0x2f, 0x7f, 0xfe, 0x1f, 0xf8,

    /* U+0054 "T" */
    0xff, 0xfe, 0xbf, 0xfe, 0x7, 0xc0, 0x7, 0xc0,
    0x7, 0xc0, 0x7, 0xc0, 0x7, 0xc0, 0x7, 0xc0,

    /* U+0055 "U" */
    0xbc, 0x1f, 0xbc, 0x1f, 0xbc, 0x1f, 0xbc, 0x1f,
    0xbc, 0x1f, 0x7c, 0x2f, 0x3f, 0xfe, 0xb, 0xf4,

    /* U+0056 "V" */
    0xfc, 0xf, 0xdf, 0x3, 0xd3, 0xe1, 0xf0, 0xbc,
    0xbc, 0x1f, 0x3d, 0x3, 0xff, 0x0, 0xbf, 0x80,
    0x1f, 0xd0,

    /* U+0057 "W" */
    0xfc, 0x3e, 0x1f, 0x6f, 0x1f, 0xcb, 0xc7, 0xcb,
    0xf3, 0xe0, 0xf7, 0xfd, 0xf4, 0x2e, 0xf7, 0xbc,
    0x7, 0xf8, 0xff, 0x0, 0xfd, 0x3f, 0x80, 0x3f,
    0xb, 0xd0,

    /* U+0058 "X" */
    0x7d, 0x1f, 0xb, 0xdf, 0x40, 0xff, 0xc0, 0xf,
    0xc0, 0x7, 0xf4, 0x3, 0xff, 0x3, 0xf3, 0xf2,
    0xf0, 0x7e,

    /* U+0059 "Y" */
    0xbc, 0x1f, 0x4f, 0x8f, 0xc1, 0xfb, 0xd0, 0x3f,
    0xe0, 0x3, 0xf0, 0x0, 0xf8, 0x0, 0x3e, 0x0,
    0xf, 0x80,

    /* U+005A "Z" */
    0xbf, 0xfe, 0xbf, 0xfd, 0x0, 0xf8, 0x3, 0xf0,
    0xf, 0xc0, 0x3f, 0x0, 0xbf, 0xfe, 0xbf, 0xfe,

    /* U+0064 "d" */
    0x0, 0x7c, 0x1, 0xf2, 0xfb, 0xdf, 0xff, 0xb8,
    0x7e, 0xe1, 0xf7, 0xff, 0xcb, 0xef,

    /* U+0068 "h" */
    0xbc, 0x2, 0xf0, 0xb, 0xef, 0x6f, 0xff, 0xbc,
    0xbe, 0xf1, 0xfb, 0xc7, 0xef, 0x1f,

    /* U+006D "m" */
    0xbe, 0xf7, 0xe2, 0xfb, 0xfb, 0xdb, 0xcb, 0x8f,
    0xaf, 0x2e, 0x3e, 0xbc, 0xb8, 0xfa, 0xf2, 0xe3,
    0xe0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 35, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 52, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 6, .adv_w = 195, .box_w = 12, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 30, .adv_w = 88, .box_w = 5, .box_h = 2, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 33, .adv_w = 51, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 36, .adv_w = 76, .box_w = 6, .box_h = 10, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 51, .adv_w = 131, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 67, .adv_w = 79, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 121, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 122, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 109, .adv_w = 133, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 125, .adv_w = 119, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 141, .adv_w = 124, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 157, .adv_w = 111, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 171, .adv_w = 123, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 124, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 51, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 208, .adv_w = 147, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 228, .adv_w = 125, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 244, .adv_w = 140, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 262, .adv_w = 137, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 121, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 296, .adv_w = 116, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 312, .adv_w = 142, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 330, .adv_w = 140, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 348, .adv_w = 54, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 354, .adv_w = 112, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 368, .adv_w = 140, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 386, .adv_w = 110, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 400, .adv_w = 178, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 422, .adv_w = 142, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 440, .adv_w = 143, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 458, .adv_w = 124, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 474, .adv_w = 143, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 495, .adv_w = 128, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 511, .adv_w = 132, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 527, .adv_w = 126, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 543, .adv_w = 135, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 559, .adv_w = 144, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 577, .adv_w = 199, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 603, .adv_w = 142, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 621, .adv_w = 140, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 639, .adv_w = 125, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 655, .adv_w = 120, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 669, .adv_w = 119, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 683, .adv_w = 177, .box_w = 11, .box_h = 6, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint8_t glyph_id_ofs_list_0[] = {
    0, 1, 0, 0, 0, 2
};

static const uint16_t unicode_list_3[] = {
    0x0, 0x4, 0x9
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 6, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = glyph_id_ofs_list_0, .list_length = 6, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL
    },
    {
        .range_start = 45, .range_length = 14, .glyph_id_start = 4,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 65, .range_length = 26, .glyph_id_start = 18,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 100, .range_length = 10, .glyph_id_start = 44,
        .unicode_list = unicode_list_3, .glyph_id_ofs_list = NULL, .list_length = 3, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    4, 18,
    4, 27,
    4, 37,
    4, 39,
    4, 40,
    4, 41,
    4, 42,
    5, 20,
    5, 24,
    5, 32,
    5, 34,
    5, 37,
    5, 42,
    6, 18,
    6, 27,
    6, 39,
    6, 40,
    7, 18,
    7, 39,
    7, 41,
    7, 42,
    10, 41,
    11, 18,
    14, 18,
    14, 27,
    14, 44,
    14, 46,
    15, 41,
    16, 18,
    16, 39,
    16, 41,
    16, 42,
    17, 37,
    17, 39,
    18, 4,
    18, 6,
    18, 7,
    18, 8,
    18, 13,
    18, 18,
    18, 20,
    18, 24,
    18, 32,
    18, 34,
    18, 36,
    18, 37,
    18, 38,
    18, 39,
    18, 40,
    18, 42,
    18, 44,
    19, 37,
    20, 18,
    20, 41,
    21, 5,
    21, 6,
    21, 18,
    21, 37,
    21, 39,
    21, 40,
    21, 41,
    21, 42,
    23, 5,
    23, 6,
    23, 18,
    23, 27,
    23, 44,
    23, 46,
    24, 18,
    24, 37,
    24, 39,
    24, 41,
    24, 42,
    27, 5,
    27, 6,
    27, 18,
    28, 4,
    28, 20,
    28, 24,
    28, 32,
    28, 34,
    28, 36,
    28, 44,
    29, 4,
    29, 8,
    29, 20,
    29, 24,
    29, 32,
    29, 34,
    29, 37,
    29, 39,
    29, 40,
    29, 42,
    32, 5,
    32, 6,
    32, 18,
    32, 37,
    32, 39,
    32, 40,
    32, 41,
    32, 42,
    33, 5,
    33, 18,
    33, 27,
    33, 37,
    33, 44,
    34, 5,
    34, 6,
    34, 18,
    34, 37,
    34, 39,
    34, 40,
    34, 41,
    34, 42,
    35, 20,
    35, 24,
    35, 32,
    35, 34,
    35, 37,
    35, 39,
    35, 42,
    35, 44,
    36, 14,
    36, 18,
    36, 37,
    36, 41,
    37, 4,
    37, 5,
    37, 6,
    37, 7,
    37, 11,
    37, 13,
    37, 14,
    37, 17,
    37, 18,
    37, 20,
    37, 24,
    37, 27,
    37, 32,
    37, 34,
    37, 36,
    37, 44,
    37, 46,
    38, 5,
    38, 6,
    38, 18,
    39, 4,
    39, 5,
    39, 6,
    39, 7,
    39, 11,
    39, 13,
    39, 17,
    39, 18,
    39, 20,
    39, 24,
    39, 27,
    39, 32,
    39, 34,
    39, 39,
    39, 44,
    39, 46,
    40, 4,
    40, 5,
    40, 6,
    40, 11,
    40, 17,
    40, 18,
    40, 20,
    40, 24,
    40, 27,
    40, 32,
    40, 34,
    40, 36,
    40, 40,
    40, 44,
    40, 46,
    41, 4,
    41, 7,
    41, 10,
    41, 11,
    41, 13,
    41, 15,
    41, 20,
    41, 24,
    41, 32,
    41, 34,
    41, 36,
    41, 44,
    42, 4,
    42, 5,
    42, 11,
    42, 17,
    42, 18,
    42, 20,
    42, 24,
    42, 27,
    42, 32,
    42, 34,
    42, 44,
    42, 46,
    43, 4,
    43, 20,
    43, 24,
    43, 32,
    43, 34,
    45, 8,
    45, 37,
    45, 39,
    45, 40,
    45, 42,
    46, 8,
    46, 37,
    46, 39,
    46, 40,
    46, 42
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -6, -9, -15, -8, -7, -9, -10, -8,
    -8, -8, -8, -6, -17, -19, -12, 3,
    3, -7, -10, -9, -6, -5, -2, -21,
    -18, -10, -10, -5, -7, -10, -9, -6,
    -6, -12, -6, 2, -7, -22, -7, 3,
    -9, -9, -9, -9, -5, -19, -9, -22,
    -22, -21, -5, -4, -7, -4, -8, -8,
    -9, -6, -10, -7, -13, -9, -6, -9,
    -17, -13, -1, -1, -7, -6, -4, -4,
    -6, -6, -11, -9, -10, -12, -12, -12,
    -12, -8, -8, -10, -9, -5, -5, -5,
    -5, -17, -16, -13, -25, -8, -8, -9,
    -6, -10, -7, -13, -9, -6, -17, -18,
    -1, -5, -8, -8, -9, -6, -10, -7,
    -13, -9, -1, -1, -1, -1, -5, -4,
    -6, -6, -3, -5, -4, -7, -15, -6,
    -10, -6, -13, -6, -15, -6, -19, -6,
    -6, -17, -6, -6, -3, -24, -11, -6,
    -11, -9, -12, -18, -21, -10, -6, -10,
    -12, -22, -10, -10, -18, -10, -10, 3,
    -17, -7, -7, -18, -11, -6, -10, -22,
    -7, -7, -12, -7, -7, -2, 4, -12,
    -6, -9, -9, -5, -8, -9, -5, -13,
    -13, -13, -13, -7, -5, -10, -17, -13,
    -12, -21, -9, -9, -6, -9, -9, -26,
    -16, -6, -6, -6, -6, -6, -8, -19,
    -10, -10, -22, -8, -19, -10, -10, -22
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 216,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 4,
    .bpp = 2,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t codex_inter_12 = {
#else
lv_font_t codex_inter_12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 10,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 2,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if CODEX_INTER_12*/
