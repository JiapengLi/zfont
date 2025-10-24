#include "zfont.h"

#include "font_robotoM12_1bpp.h"
#include "font_alipht.h"
#include "font_robotomono.h"

const uint8_t *font = font_robotomono;

void test1(char c)
{
    uint8_t gray;
    int x, y;

    zf_log(font);

    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            gray = zf_get_pixel(font, c, x, y);
            if (gray == 255) {
                printf("#");
            } else {
                printf(".");
            }
        }
        printf("\n");
    }
}

void test2(char c)
{
    zf_glyph_ctx_t g;
    int rc;
    int x = 0, y = 0;

    rc = zf_get_glyph(font, c, &g);
    if (rc == ZF_OK) {
        ZF_PRINT("Glyph A found: left=%d, bottom=%d, width=%d, height=%d, advance=%d",
            g.info.left, g.info.bottom, g.info.width, g.info.height, g.info.advance);
        ZF_PRINT("Glyph bitnum: %d, %d, %d",
            g.glyph_pixel_bitnum,
            g.glyph_repeat_black_bitnum,
            g.glyph_repeat_white_bitnum);
        ZF_PRINT("Glyph BS: size=%d, byte_index=%d, bit_index=%d, data=%p",
            g.bs.size, g.bs.byte_index, g.bs.bit_index, g.bs.data);

    } else {
        ZF_PRINT("Glyph A not found");
    }

    for (y = 0; y < g.info.height; y++) {
        for (x = 0; x < g.info.width; x++) {
            int pixel = zf_get_pixel_from_glyph(&g, x, y);
            if (pixel == 255) {
                printf("#");
            } else {
                printf(".");
            }
        }
        printf("\n");
    }
}

void test3(const char *text)
{
    int num, w, h, pixel;

    int i, x, y;

    zf_glyph_ctx_t glyph[256];

    num = zf_get_glyph_by_text(font, text, glyph, sizeof(glyph) / sizeof(glyph[0]));

    w = 0;
    h = zf_get_font_height(font);
    for (int i = 0; i < num; i++) {
        printf("Glyph %d: codepoint=0x%x, left=%d, bottom=%d, width=%d, height=%d, advance=%d\n",
            i,
            glyph[i].codepoint,
            glyph[i].info.left, glyph[i].info.bottom, glyph[i].info.width, glyph[i].info.height,
            glyph[i].info.advance);
        w += glyph[i].info.advance;
    }

    // printf("Text: \"%s\", num glyphs=%d, total width=%d, height=%d\n", text, num, w, h);
    // for (y = 0; y < h; y++) {
    //     for (i = 0; i < num; i++) {
    //         for (x = 0; x < glyph[i].info.width; x++) {
    //             pixel = zf_get_pixel_from_glyph(&glyph[i], x, y);
    //             if (pixel == 255) {
    //                 printf("#");
    //             } else {
    //                 printf(".");
    //             }
    //         }
    //     }
    //     printf("\n");
    // }

    printf("Text: num glyphs=%d, total width=%d, height=%d, \"%s\"\n", num, w, h, text);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int gi;
            int acc_w = 0;
            for (gi = 0; gi < num; gi++) {
                if ((x >= acc_w) && (x < acc_w + glyph[gi].info.advance)) {
                    break;
                }
                acc_w += glyph[gi].info.advance;
            }

            pixel = zf_get_pixel_from_glyph_with_box(&glyph[gi], x, y, acc_w, 0, w, h);
            if (pixel == 255) {
                printf("#");
            } else {
                printf(".");
            }
        }
        printf("\n");
    }
}

void test4(void)
{
    char text[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

    test3(text);
}

int main(int argc, char *argv[])
{
    uint8_t gray;
    int x, y;

    char c = '0';

    zf_log(font);

    if (argc > 1) {
        c = argv[1][0];
    }

    test1(c);

    test2(c);

    test3(argv[1]);

    test4();

    return 0;
}
