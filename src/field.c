#include "uomappp/field.h"

#include <stdlib.h>
#include <math.h>

#define FIELD_BIG 1.0e18f

int field_distance_transform(const uint8_t *mask, float *out, int W, int H) {
    if (W <= 0 || H <= 0)
        return -1;
    const float D1 = 1.0f;            /* orthogonal step cost */
    const float D2 = 1.41421356f;     /* diagonal step cost (~sqrt 2) */

    for (int i = 0; i < W * H; ++i)
        out[i] = mask[i] ? 0.0f : FIELD_BIG;

    /* Forward pass: top-left -> bottom-right, look up/left/up-left/up-right. */
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int i = x + y * W;
            float v = out[i];
            if (y > 0) {
                if (out[i - W] + D1 < v) v = out[i - W] + D1;
                if (x > 0     && out[i - W - 1] + D2 < v) v = out[i - W - 1] + D2;
                if (x < W - 1 && out[i - W + 1] + D2 < v) v = out[i - W + 1] + D2;
            }
            if (x > 0 && out[i - 1] + D1 < v) v = out[i - 1] + D1;
            out[i] = v;
        }
    }
    /* Backward pass: bottom-right -> top-left, look down/right/down-left/down-right. */
    for (int y = H - 1; y >= 0; --y) {
        for (int x = W - 1; x >= 0; --x) {
            int i = x + y * W;
            float v = out[i];
            if (y < H - 1) {
                if (out[i + W] + D1 < v) v = out[i + W] + D1;
                if (x > 0     && out[i + W - 1] + D2 < v) v = out[i + W - 1] + D2;
                if (x < W - 1 && out[i + W + 1] + D2 < v) v = out[i + W + 1] + D2;
            }
            if (x < W - 1 && out[i + 1] + D1 < v) v = out[i + 1] + D1;
            out[i] = v;
        }
    }
    return 0;
}

/* One separable box blur of the given radius: horizontal into scratch, then
 * vertical back into buf. Uses running sums so each pass is O(W*H). */
static void box_blur_once(float *buf, float *tmp, int W, int H, int r) {
    const int win = 2 * r + 1;
    /* Horizontal: buf -> tmp */
    for (int y = 0; y < H; ++y) {
        const float *row = buf + (size_t)y * W;
        float *dst = tmp + (size_t)y * W;
        double sum = 0.0;
        for (int x = 0; x <= r && x < W; ++x) sum += row[x];
        /* Edge handling: clamp window to [0,W-1], divide by actual count. */
        for (int x = 0; x < W; ++x) {
            int lo = x - r, hi = x + r;
            if (x > 0) {
                if (hi < W)        sum += row[hi];
                if (lo - 1 >= 0)   sum -= row[lo - 1];
            }
            int clo = lo < 0 ? 0 : lo;
            int chi = hi >= W ? W - 1 : hi;
            dst[x] = (float)(sum / (double)(chi - clo + 1));
        }
        (void)win;
    }
    /* Vertical: tmp -> buf */
    for (int x = 0; x < W; ++x) {
        double sum = 0.0;
        for (int y = 0; y <= r && y < H; ++y) sum += tmp[(size_t)y * W + x];
        for (int y = 0; y < H; ++y) {
            int lo = y - r, hi = y + r;
            if (y > 0) {
                if (hi < H)      sum += tmp[(size_t)hi * W + x];
                if (lo - 1 >= 0) sum -= tmp[(size_t)(lo - 1) * W + x];
            }
            int clo = lo < 0 ? 0 : lo;
            int chi = hi >= H ? H - 1 : hi;
            buf[(size_t)y * W + x] = (float)(sum / (double)(chi - clo + 1));
        }
    }
}

int field_blur(float *buf, int W, int H, int radius, int passes) {
    if (W <= 0 || H <= 0)
        return -1;
    if (radius < 1 || passes < 1)
        return 0;
    float *tmp = (float *)malloc((size_t)W * H * sizeof(float));
    if (!tmp)
        return -1;
    for (int p = 0; p < passes; ++p)
        box_blur_once(buf, tmp, W, H, radius);
    free(tmp);
    return 0;
}

void field_normalize(float *buf, int W, int H) {
    const size_t n = (size_t)W * H;
    if (n == 0)
        return;
    float mn = buf[0], mx = buf[0];
    for (size_t i = 1; i < n; ++i) {
        if (buf[i] < mn) mn = buf[i];
        if (buf[i] > mx) mx = buf[i];
    }
    float range = mx - mn;
    if (range <= 0.0f)
        return;
    float inv = 1.0f / range;
    for (size_t i = 0; i < n; ++i)
        buf[i] = (buf[i] - mn) * inv;
}
