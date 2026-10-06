#include "uomappp/marching.h"

#include <stddef.h>

long marching_squares_borders(const int32_t *label, uint8_t *out, int W, int H) {
    long count = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)x + (size_t)y * W;
            int32_t l = label[i];
            int border = 0;
            if (x + 1 < W && label[i + 1] != l)      border = 1;
            else if (y + 1 < H && label[i + W] != l) border = 1;
            else if (x > 0 && label[i - 1] != l)     border = 1;
            else if (y > 0 && label[i - W] != l)     border = 1;
            out[i] = (uint8_t)border;
            if (border) ++count;
        }
    return count;
}
