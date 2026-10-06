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

    /* Outer sweep seeds EVERY allowed region (so disconnected landmasses each get
     * points, not just the one containing the first seed). Start the sweep at a
     * splitmix-chosen offset for per-seed variety. */
    const long NC = (long)W * (long)H;
    long startoff = (long)(u01(&st) * (double)NC);
    if (startoff < 0) startoff = 0;
    if (startoff >= NC) startoff = NC - 1;

    for (long s = 0; s < NC && n < maxPts; ++s) {
        long idx = startoff + s; if (idx >= NC) idx -= NC;
        int sx = (int)(idx % W), sy = (int)(idx / W);
        if (allow && !allow[(size_t)idx]) continue;
        int gcx = (int)(sx / cell), gcy = (int)(sy / cell);
        if (grid[gcx + gcy * gw] >= 0) continue;           /* cell already taken */
        /* Skip if already within radius of a placed point (covered). */
        int covered = 0;
        for (int dy = -2; dy <= 2 && !covered; ++dy)
            for (int dx = -2; dx <= 2; ++dx) {
                int nx = gcx + dx, ny = gcy + dy;
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                int o = grid[nx + ny * gw];
                if (o < 0) continue;
                double ddx = sx - outX[o], ddy = sy - outY[o];
                if (ddx * ddx + ddy * ddy < radius * radius) { covered = 1; break; }
            }
        if (covered) continue;

        /* Seed a new region here and grow it (Bridson active list). */
        outX[n] = sx; outY[n] = sy;
        grid[gcx + gcy * gw] = n;
        nactive = 0;
        activeX[nactive] = sx; activeY[nactive] = sy; ++nactive; ++n;

        while (nactive > 0 && n < maxPts) {
            int ai = nactive - 1;                          /* fixed order: last active */
            double px = activeX[ai], py = activeY[ai];
            int placed = 0;
            for (int attempt = 0; attempt < k; ++attempt) {
                double ang = u01(&st) * 6.283185307179586;
                double rr = radius * (1.0 + u01(&st));      /* [r, 2r) */
                int cx = (int)lround(px + rr * cos(ang));
                int cy = (int)lround(py + rr * sin(ang));
                if (cx < 0 || cy < 0 || cx >= W || cy >= H) continue;
                if (allow && !allow[(size_t)cx + (size_t)cy * W]) continue;

                int ngx = (int)(cx / cell), ngy = (int)(cy / cell);
                int ok = 1;
                for (int dy = -2; dy <= 2 && ok; ++dy)
                    for (int dx = -2; dx <= 2; ++dx) {
                        int nx = ngx + dx, ny = ngy + dy;
                        if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                        int o = grid[nx + ny * gw];
                        if (o < 0) continue;
                        double ddx = cx - outX[o], ddy = cy - outY[o];
                        if (ddx * ddx + ddy * ddy < radius * radius) { ok = 0; break; }
                    }
                if (!ok) continue;

                outX[n] = cx; outY[n] = cy;
                grid[ngx + ngy * gw] = n;
                if (nactive < maxPts) { activeX[nactive] = cx; activeY[nactive] = cy; ++nactive; }
                ++n; placed = 1;
                break;
            }
            if (!placed) --nactive;                         /* retire this active point */
        }
    }

    free(grid); free(activeX); free(activeY);
    return n;
}
