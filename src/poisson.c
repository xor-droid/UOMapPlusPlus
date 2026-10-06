#include "uomappp/poisson.h"
#include "uomappp/noise.h"

#include <stdlib.h>
#include <math.h>

static double u01(uint64_t *st) {
    return (double)(noise_splitmix64(st) >> 11) * (1.0 / 9007199254740992.0);
}

int poisson_sample(int W, int H, double radius, const uint8_t *allow, int k,
                   uint64_t seed, uint32_t salt,
                   int *outX, int *outY, int maxPts) {
    if (W <= 0 || H <= 0 || radius < 1.0 || maxPts <= 0)
        return 0;
    if (k < 1) k = 30;

    const double cell = radius / 1.4142135624;   /* so a cell holds <=1 point */
    const int gw = (int)(W / cell) + 1;
    const int gh = (int)(H / cell) + 1;
    int *grid = (int *)malloc((size_t)gw * gh * sizeof(int));
    int *activeX = (int *)malloc((size_t)maxPts * sizeof(int));
    int *activeY = (int *)malloc((size_t)maxPts * sizeof(int));
    if (!grid || !activeX || !activeY) {
        free(grid); free(activeX); free(activeY); return 0;
    }
    for (int i = 0; i < gw * gh; ++i) grid[i] = -1;

    uint64_t st = seed ^ (0xD1B54A32D192ED03ULL * (uint64_t)(salt + 1));

    int n = 0, nactive = 0;

    /* Seed point: scan deterministically for the first allowed cell near a
     * splitmix-chosen position. */
    int sx = (int)(u01(&st) * (W - 1));
    int sy = (int)(u01(&st) * (H - 1));
    int found = 0;
    for (int off = 0; off < W * H && !found; ++off) {
        int x = (sx + off) % W;
        int y = (sy + (off / W)) % H;
        if (!allow || allow[(size_t)x + (size_t)y * W]) { sx = x; sy = y; found = 1; }
    }
    if (!found) { free(grid); free(activeX); free(activeY); return 0; }

    outX[n] = sx; outY[n] = sy;
    grid[(int)(sx / cell) + (int)(sy / cell) * gw] = n;
    activeX[nactive] = sx; activeY[nactive] = sy; ++nactive; ++n;

    while (nactive > 0 && n < maxPts) {
        /* Fixed order: always take the last active point. */
        int ai = nactive - 1;
        double px = activeX[ai], py = activeY[ai];
        int placed = 0;
        for (int attempt = 0; attempt < k; ++attempt) {
            double ang = u01(&st) * 6.283185307179586;
            double rr = radius * (1.0 + u01(&st));     /* [r, 2r) */
            int cx = (int)lround(px + rr * cos(ang));
            int cy = (int)lround(py + rr * sin(ang));
            if (cx < 0 || cy < 0 || cx >= W || cy >= H) continue;
            if (allow && !allow[(size_t)cx + (size_t)cy * W]) continue;

            int gcx = (int)(cx / cell), gcy = (int)(cy / cell);
            int ok = 1;
            for (int dy = -2; dy <= 2 && ok; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = gcx + dx, ny = gcy + dy;
                    if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                    int o = grid[nx + ny * gw];
                    if (o < 0) continue;
                    double ddx = cx - outX[o], ddy = cy - outY[o];
                    if (ddx * ddx + ddy * ddy < radius * radius) { ok = 0; break; }
                }
            if (!ok) continue;

            outX[n] = cx; outY[n] = cy;
            grid[gcx + gcy * gw] = n;
            activeX[nactive] = cx; activeY[nactive] = cy; ++nactive;
            ++n; placed = 1;
            break;
        }
        if (!placed) --nactive;   /* retire this active point */
    }

    free(grid); free(activeX); free(activeY);
    return n;
}
