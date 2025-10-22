#include "zfont.h"

#ifndef   __PACKED
#define __PACKED                               __attribute__((packed, aligned(1)))
#endif

/* set pack */
typedef struct __PACKED {
    uint32_t size;
    int16_t cap_height;
    int16_t ascent;
    int16_t descent;

    /* font boundingbox */
    int16_t fbb_left;
    int16_t fbb_bottom;
    int16_t fbb_width;
    int16_t fbb_height;

    /* glyph boundingbox */
    uint8_t gbb_left_bitnum;
    uint8_t gbb_bottom_bitnum;
    uint8_t gbb_width_bitnum;
    uint8_t gbb_height_bitnum;

    uint8_t glyph_advance_bitnum;
    uint8_t glyph_pixel_bitnum;
    uint8_t glyph_repeat_black_bitnum;
    uint8_t glyph_repeat_white_bitnum;
} zf_font_header_t;

typedef struct {
    const uint8_t *data;
    int size;

    int byte_index;
    int bit_index;
} zf_bs_t;

typedef struct
{
    zf_bs_t bs;

    zf_glyph_t info;
} zf_glyph_ctx_t;

int32_t zf_read_variable(const uint8_t **p)
{
    int32_t value = 0;

    while (true) {
        uint8_t n = **p;
        value = (value << 7) | (n & 0x7F);
        (*p)++;
        if ((n & 0x80) == 0) {
            break;
        }
    }

    return value;
}


static uint8_t zf_read_unsigned_bits(zf_bs_t *br, uint8_t bitnum)
{
    uint8_t start_bit_index = br->bit_index;
    uint8_t end_bit_index = start_bit_index + bitnum;

    uint8_t value = (*br->data >> start_bit_index);

    if (end_bit_index >= 8)
    {
        br->data++;
        br->byte_index++;

        value |= *br->data << (8 - start_bit_index);

        end_bit_index -= 8;
    }

    br->bit_index = end_bit_index;

    value &= (1 << bitnum) - 1;

    return value;
}

static int8_t zf_read_signed_bits(zf_bs_t *br, uint8_t bitnum)
{
    int8_t value = (int8_t)zf_read_unsigned_bits(br, bitnum);

    uint8_t shift = 8 - bitnum;
    value <<= shift;
    value >>= shift;

    return value;
}

static int8_t zf_read_rice(zf_bs_t *br, int8_t runlength_bitnum)
{
    uint16_t runlength_remainder = zf_read_unsigned_bits(br, runlength_bitnum);
    uint16_t runlength_quotient = 0;
    while (zf_read_unsigned_bits(br, 1)) {
        runlength_quotient++;
    }

    return (runlength_quotient << runlength_bitnum) + runlength_remainder;
}

static int8_t zf_read_unary(zf_bs_t *br)
{
    int8_t value = 0;
    while (zf_read_unsigned_bits(br, 1)) {
        value++;
    }
    return value;
}

#include <stdint.h>

static const uint8_t pixel_lut[4][16] = {
    /* bitnum = 1 */ {
        0, 255,                         // 0..1
        255,255,255,255,255,255,255,255,255,255,255,255
    },
    /* bitnum = 2 */ {
        0, 85, 170, 255,                // 0..3
        255,255,255,255,255,255,255,255,255,255,255,255
    },
    /* bitnum = 3 */ {
        0, 36, 73, 109, 146, 182, 219, 255,  // 0..7
        255,255,255,255,255,255,255,255
    },
    /* bitnum = 4 */ {
        0, 17, 34, 51, 68, 85, 102, 119,
        136, 153, 170, 187, 204, 221, 238, 255
    }
};

static inline uint8_t zf_pixel_fmt(uint8_t val, uint8_t bitnum)
{
    return pixel_lut[bitnum - 1][val];
}

/*
header->glyph_pixel_bitnum: 1, 2, 3, 4
val
*/
int zf_get_pixel(const char *font, uint16_t codepoint, int x, int y)
{
    const zf_font_header_t *header = (const zf_font_header_t *)font;

    const uint8_t *block_cur, *glyph=NULL, *glyph_end;
    int glyph_size, blk_l, blk_cp_cnt, blk_cp, glyph_code, glyph_l;
    int value, black_value, white_value, run_length;

    int repeat_length, repeat_num, repeat_index;
    zf_bs_t repeat_bs;

    int pixel, total_pixels, target_pixel;

    glyph_size = header->size - sizeof(zf_font_header_t);

    ZF_LOG("");
    ZF_LOG("Font size: %d", header->size);
    ZF_LOG("Font cap height: %d", header->cap_height);
    ZF_LOG("Font ascent: %d", header->ascent);
    ZF_LOG("Font descent: %d", header->descent);
    ZF_LOG("Font bounding box: left=%d, bottom=%d, width=%d, height=%d",
        header->fbb_left, header->fbb_bottom,
        header->fbb_width, header->fbb_height);
    ZF_LOG("Font glyph bounding box bitnum: left=%d, bottom=%d, width=%d, height=%d advance=%d",
        header->gbb_left_bitnum, header->gbb_bottom_bitnum,
        header->gbb_width_bitnum, header->gbb_height_bitnum,
        header->glyph_advance_bitnum
    );
    ZF_LOG("Font glyph pixel bitnum: %d", header->glyph_pixel_bitnum);
    ZF_LOG("Font glyph repeat black bitnum: %d", header->glyph_repeat_black_bitnum);
    ZF_LOG("Font glyph repeat white bitnum: %d", header->glyph_repeat_white_bitnum);

    // ZF_HEXDUMP(font, header->size);

    // zf_bs_t _bs, *bs = &_bs;
    // bs->data = font + sizeof(zf_font_header_t);
    // bs->size = header->size - sizeof(zf_font_header_t);

    // ZF_LOG("bs:");
    // ZF_HEXDUMP(bs->data, bs->size);

    block_cur = font + sizeof(zf_font_header_t);
    while (true) {
        blk_l = zf_read_variable(&block_cur);
        if (blk_l == 0) {
#if ZF_DEBUG
            break;
#else
            return ZF_ERR_GLYPH_NOT_FOUND;
#endif
        }
        blk_cp_cnt = zf_read_variable(&block_cur);
        blk_cp = zf_read_variable(&block_cur);

        ZF_LOG("Block: length=%d, codepoint_count=%d, first_codepoint=0x%04X",
            blk_l, blk_cp_cnt, blk_cp);

        if ((codepoint >= blk_cp) && (codepoint < blk_cp + blk_cp_cnt)) {
            /* block matched */
            glyph = block_cur;
            glyph_end = block_cur + blk_l;
            glyph_code = blk_cp;

#if ZF_DEBUG == 0
            break;
#endif
        }

        block_cur += blk_l;
    }

    if (!glyph) {
        return ZF_ERR_GLYPH_NOT_FOUND;
    }

    ZF_LOG("Glyph block data:");
    ZF_HEXDUMP(glyph, glyph_end - glyph);

    black_value = 0;
    white_value = (1 << header->glyph_pixel_bitnum) - 1;
    while (glyph < glyph_end) {
        glyph_l = zf_read_variable(&glyph);
        ZF_LOG("Glyph: length=%d, codepoint=0x%04X", glyph_l, glyph_code);

        if (glyph_code == codepoint) {
            /* glyph matched */
            ZF_LOG("Glyph found for codepoint 0x%04X", codepoint);

            ZF_LOG("Glyph data:");
            ZF_HEXDUMP(glyph, glyph_l);

            // 80 64 6A C7 1E CD 46 D8 1B 9B C0 EC B1 23

            zf_glyph_ctx_t g;

            g.bs.data = glyph;
            g.bs.size = glyph_l;
            g.bs.byte_index = 0;
            g.bs.bit_index = 0;

            g.info.left = zf_read_signed_bits(&g.bs, header->gbb_left_bitnum);
            g.info.bottom = zf_read_signed_bits(&g.bs, header->gbb_bottom_bitnum);
            g.info.width = zf_read_unsigned_bits(&g.bs, header->gbb_width_bitnum);
            g.info.height = zf_read_unsigned_bits(&g.bs, header->gbb_height_bitnum);
            g.info.advance = zf_read_unsigned_bits(&g.bs, header->glyph_advance_bitnum);

            ZF_LOG("Glyph bitnum: left=%d, bottom=%d, width=%d, height=%d, advance=%d",
                header->gbb_left_bitnum,
                header->gbb_bottom_bitnum,
                header->gbb_width_bitnum,
                header->gbb_height_bitnum,
                header->glyph_advance_bitnum
            );
            ZF_LOG("Glyph metrics: left=%d, bottom=%d, width=%d, height=%d, advance=%d",
                g.info.left, g.info.bottom, g.info.width, g.info.height, g.info.advance);

            ZF_LOG("BS: size=%d, byte_index=%d, bit_index=%d",
                g.bs.size, g.bs.byte_index, g.bs.bit_index);

            if ( (x < 0) || (x >= g.info.width) ||
                 (y < 0) || (y >= g.info.height) ) {
                return 0;
            }

            pixel = 0;
            total_pixels = g.info.width * g.info.height;
            target_pixel = y * g.info.width + x;
            repeat_length = 0;
            repeat_num = 0;
            repeat_index = 0;
            while ((g.bs.byte_index < g.bs.size) && (pixel < total_pixels)) {
                value = zf_read_unsigned_bits(&g.bs, header->glyph_pixel_bitnum);
                if (value == black_value) {
                    /* black */
                    run_length = zf_read_rice(&g.bs, header->glyph_repeat_black_bitnum);
                    if (run_length == 0) {
                        /* repeat */
                        repeat_length = zf_read_unary(&g.bs) + 2;
                        repeat_num = zf_read_unary(&g.bs) + 2;

                        repeat_index = repeat_length;
                        repeat_bs = g.bs;
                        continue;
                    }
                } else if (value == white_value) {
                    /* white */
                    run_length = zf_read_rice(&g.bs, header->glyph_repeat_white_bitnum);
                    run_length += 1;
                } else {
                    run_length = 1;
                    /* pixel value (only one pixel) */
                }

                pixel += run_length;
                ZF_LOG("pixel %d/%d, value: %d, run length: %d, repeat length: %d, repeat num: %d",
                    pixel, total_pixels,
                    value, run_length, repeat_length, repeat_num);

                if (target_pixel < pixel) {
                    return zf_pixel_fmt(value, header->glyph_pixel_bitnum);
                }

                if (repeat_index) {
                    repeat_index--;
                    if (repeat_index == 0) {
                        /* do repeat */
                        repeat_num--;
                        if (repeat_num) {
                            g.bs = repeat_bs;
                            repeat_index = repeat_length;
                        } else {
                            repeat_length = 0;
                        }
                    }
                }
            }
        }

        glyph += glyph_l;
        glyph_code++;
    }

    return 0;
}

int zf_get_glyph(const char *font, uint16_t codepoint, int x, int y)
{

}

