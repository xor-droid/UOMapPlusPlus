#include "uomappp/lsystem.h"
#include "uomappp/noise.h"

#include <math.h>

static double u01(uint64_t *st) {
    return (double)(noise_splitmix64(st) >> 11) * (1.0 / 9007199254740992.0);
}

/* Draw one forward segment of `len` tiles from (x,y) along angle, appending
 * visited cells. Returns the tip position via *tx,*ty. */
static void draw_segment(int W, int H, double x, double y, double angle,
                         double len, int *out, int maxOut, int *n,
                         double *tx, double *ty) {
    double dx = cos(angle), dy = sin(angle);
    int steps = (int)(len + 0.5);
    for (int s = 1; s <= steps; ++s) {
        int cx = (int)lround(x + dx * s);
        int cy = (int)lround(y + dy * s);
        if (cx < 0 || cy < 0 || cx >= W || cy >= H) break;
        if (*n < maxOut) out[(*n)++] = cx + cy * W;
    }
    *tx = x + dx * steps;
    *ty = y + dy * steps;
}

static void grow(int W, int H, double x, double y, double angle, double step,
                 double turn, int depth, uint64_t *st,
                 int *out, int maxOut, int *n) {
    if (depth < 0 || *n >= maxOut)
        return;
    if (x < 0 || y < 0 || x >= W || y >= H)
        return;

    double tx, ty;
    draw_segment(W, H, x, y, angle, step, out, maxOut, n, &tx, &ty);
    if (depth == 0)
        return;

    int branches = (u01(st) < 0.5) ? 2 : 3;    /* stochastic fan */
    for (int b = 0; b < branches; ++b) {
        double jitter = (u01(st) - 0.5) * turn;
        double da = (b == 0) ? -turn : (b == 1) ? turn : 0.0;
        grow(W, H, tx, ty, angle + da + jitter, step * 0.78, turn,
             depth - 1, st, out, maxOut, n);
    }
}

int lsystem_trail(int W, int H, int x0, int y0, double angle0, double step,
                  double turn, int depth, uint64_t seed, uint32_t salt,
                  int *out, int maxOut) {
    if (W <= 0 || H <= 0 || maxOut <= 0)
        return 0;
    uint64_t st = seed
                ^ (0xD1B54A32D192ED03ULL * (uint64_t)(salt + 1))
                ^ ((uint64_t)(uint32_t)(x0 * 73856093) )
                ^ ((uint64_t)(uint32_t)(y0 * 19349663) << 16);
    int n = 0;
    grow(W, H, (double)x0, (double)y0, angle0, step, turn, depth,
         &st, out, maxOut, &n);
    return n;
}
