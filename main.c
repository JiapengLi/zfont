#include <stdio.h>
#include <stdlib.h>
#include <iconv.h>

#include "zfont.h"

#include "font_alipht.h"
#include "font_alipht_m32.h"
#include "font_alipht_m16.h"
#include "font_robotomono.h"

const uint8_t *font = font_robotomono;


unsigned char* load_file_to_buffer(const char *filename, size_t *out_size) {
    FILE *file = fopen(filename, "rb");  // Open file in binary read mode
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    // Move to end to determine file size
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("fseek failed");
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        perror("ftell failed");
        fclose(file);
        return NULL;
    }
    rewind(file);

    // Allocate memory for the file contents
    unsigned char *buffer = (unsigned char *)malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    // Read the file into the buffer
    size_t read_size = fread(buffer, 1, file_size, file);
    if (read_size != (size_t)file_size) {
        fprintf(stderr, "Failed to read file completely\n");
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);

    if (out_size)
        *out_size = read_size;

    return buffer;
}

// Convert from one encoding to another using iconv
static int convert_encoding(const char *from_charset, const char *to_charset,
                            const char *input, size_t in_size,
                            char *output, size_t out_size)
{
    iconv_t cd;
    char *inbuf = (char *)input;
    char *outbuf = output;
    size_t inbytesleft = in_size;
    size_t outbytesleft = out_size;

    cd = iconv_open(to_charset, from_charset);
    if (cd == (iconv_t)-1) {
        perror("iconv_open");
        return -1;
    }

    memset(output, 0, out_size); // clear output buffer

    if (iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1) {
        perror("iconv");
        iconv_close(cd);
        return -1;
    }

    iconv_close(cd);
    return (int)(out_size - outbytesleft); // return number of bytes written
}

// Convert GBK → UTF-8
int gbk_to_utf8(const uint8_t *gbk, char *utf8, int utf8_size)
{
    if (!gbk || !utf8 || utf8_size <= 0) return -1;
    return convert_encoding("GBK", "UTF-8", (const char *)gbk, strlen((const char *)gbk), utf8, utf8_size);
}

// Convert UTF-8 → GBK
int utf8_to_gbk(const char *utf8, uint8_t *gbk, int gbk_size)
{
    if (!utf8 || !gbk || gbk_size <= 0) return -1;
    return convert_encoding("UTF-8", "GBK", utf8, strlen(utf8), (char *)gbk, gbk_size);
}


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

void test3(const char *utf8_str)
{
    int num, w, h, pixel;

    int i, x, y;

    zf_glyph_ctx_t glyph[256];

    char gbk_str[256];

    utf8_to_gbk(utf8_str, (uint8_t *)gbk_str, sizeof(gbk_str));


    ZF_PRINTHEXDUMP(utf8_str, strlen(utf8_str));

    num = zf_get_glyph_by_text(font, utf8_str, glyph, sizeof(glyph) / sizeof(glyph[0]));

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

    printf("Text: num glyphs=%d, total width=%d, height=%d, \"%s\"\n", num, w, h, gbk_str);
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

void test_ascii_single(void)
{
    int c;
    for (c = ' '; c <= '~'; c++) {
        printf("Testing ASCII char: '%c' (0x%02X)\n", c, c);
        test1(c);
    }
}

void test_ascii_line(void)
{
    char text[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

    test3(text);
}

int main(int argc, char *argv[])
{
    uint8_t gray;

    uint8_t utf8_str[256];
    uint8_t gbk_str[256];

    /* dynamic loading font library */
    if (argc > 2) {
        const char *f;
        size_t fsize;
        f = load_file_to_buffer(argv[2], &fsize);
        if (f) {
            font = f;
            printf("font loaded\n");
        }
    }

    zf_log(font);

    gbk_to_utf8(argv[1], (char *)utf8_str, sizeof(utf8_str));

    // test_ascii_single();

    // test_ascii_line();

    test3(utf8_str);

    return 0;
}

/*
build:
gcc main.c zfont/zfont.c -DZF_DEBUG=0 -I zfont/ -llibiconv -o test_main
*/
