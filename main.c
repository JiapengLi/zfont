#include "zfont.h"

#include "font_robotoM12_1bpp.h"

int main(int argc, char *argv[])
{
    uint8_t gray;
    int x, y;

    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            gray = zf_get_pixel(font_robotoM12_1bpp, '0', x, y);
            if (gray == 255) {
                printf("#");
            } else {
                printf(".");
            }
        }
        printf("\n");
    }

    return 0;
}
