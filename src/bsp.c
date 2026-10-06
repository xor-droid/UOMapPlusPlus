#include "uomappp/bsp.h"
#include "uomappp/noise.h"

static double u01(uint64_t *st) {
    return (double)(noise_splitmix64(st) >> 11) * (1.0 / 9007199254740992.0);
}

static void split(int x, int y, int w, int h, int minSize, int depth,
                  int maxDepth, uint64_t *st, bsp_rect *out, int maxOut, int *n) {
    if (*n >= maxOut)
        return;
    int canH = (w >= 2 * minSize);   /* can split vertically (into left/right) */
    int canV = (h >= 2 * minSize);   /* can split horizontally (into top/bottom) */

    if (depth >= maxDepth || (!canH && !canV)) {
        out[(*n)++] = (bsp_rect){ x, y, w, h };
        return;
    }

    int splitH;                      /* 1 = split the width, 0 = split the height */
    if (canH && canV) splitH = (w >= h);          /* cut the longer axis */
    else              splitH = canH;

    if (splitH) {
        int lo = minSize, hi = w - minSize;
        int at = lo + (int)(u01(st) * (hi - lo + 1));
        split(x, y, at, h, minSize, depth + 1, maxDepth, st, out, maxOut, n);
        split(x + at, y, w - at, h, minSize, depth + 1, maxDepth, st, out, maxOut, n);
    } else {
        int lo = minSize, hi = h - minSize;
        int at = lo + (int)(u01(st) * (hi - lo + 1));
        split(x, y, w, at, minSize, depth + 1, maxDepth, st, out, maxOut, n);
        split(x, y + at, w, h - at, minSize, depth + 1, maxDepth, st, out, maxOut, n);
    }
}

int bsp_partition(int x, int y, int w, int h, int minSize, int maxDepth,
                  uint64_t seed, uint32_t salt, bsp_rect *out, int maxOut) {
    if (w <= 0 || h <= 0 || minSize < 1 || maxOut <= 0)
        return 0;
    uint64_t st = seed ^ (0xD1B54A32D192ED03ULL * (uint64_t)(salt + 1));
    int n = 0;
    split(x, y, w, h, minSize, 0, maxDepth, &st, out, maxOut, &n);
    return n;
}
