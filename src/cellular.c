#include "uomappp/cellular.h"
#include "uomappp/noise.h"

#include <stdlib.h>

void cellular_fill(uint8_t *mask, const uint8_t *domain, int W, int H,
                   double p, uint64_t seed, uint32_t salt) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)x + (size_t)y * W;
            if (domain && !domain[i]) { mask[i] = 0; continue; }
            uint64_t s = seed
                       ^ (0x2545F4914F6CDD1DULL * (uint64_t)(uint32_t)x)
                       ^ (0x9E3779B97F4A7C15ULL * (uint64_t)(uint32_t)y)
                       ^ ((uint64_t)salt << 56);
            double u = (double)(noise_splitmix64(&s) >> 11) * (1.0 / 9007199254740992.0);
            mask[i] = (u < p) ? 1 : 0;
        }
}

static int count_neighbours(const uint8_t *m, int W, int H, int x, int y, int edge) {
    int c = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (!dx && !dy) continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) c += edge;
            else c += m[(size_t)nx + (size_t)ny * W];
        }
    return c;
}

int cellular_step(uint8_t *mask, const uint8_t *domain, int W, int H,
                  int birth, int survive, int edge, int iterations) {
    if (iterations < 1)
        return 0;
    const size_t n = (size_t)W * (size_t)H;
    uint8_t *next = (uint8_t *)malloc(n);
    if (!next)
        return -1;
    for (int it = 0; it < iterations; ++it) {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                size_t i = (size_t)x + (size_t)y * W;
                if (domain && !domain[i]) { next[i] = 0; continue; }
                int c = count_neighbours(mask, W, H, x, y, edge);
                next[i] = mask[i] ? (c >= survive) : (c >= birth);
            }
        for (size_t i = 0; i < n; ++i) mask[i] = next[i];
    }
    free(next);
    return 0;
}
