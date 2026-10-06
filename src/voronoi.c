#include "uomappp/voronoi.h"
#include "uomappp/noise.h"

#include <stdlib.h>

static double u01(uint64_t *st) {
    return (double)(noise_splitmix64(st) >> 11) * (1.0 / 9007199254740992.0);
}

int voronoi_build(voronoi_diagram *vd, int32_t *region, int W, int H,
                  uint64_t seed, int spacing, double jitter,
                  double warp_amp, double warp_freq) {
    if (W <= 0 || H <= 0)
        return -1;
    if (spacing < 1)
        spacing = 1;
    if (jitter < 0.0) jitter = 0.0;
    if (jitter > 1.0) jitter = 1.0;

    /* Optional domain-warp fields for organic borders. */
    noise_layer *wx = NULL, *wy = NULL;
    if (warp_amp > 0.0) {
        double f = warp_freq > 0.0 ? warp_freq : 0.01;
        wx = noise_layer_create(seed, NOISE_LAYER_WARP, f, 3);
        wy = noise_layer_create(seed, NOISE_LAYER_WARP + 0x1000u, f, 3);
        if (!wx || !wy) { noise_layer_free(wx); noise_layer_free(wy); wx = wy = NULL; }
    }

    const int gx = (W + spacing - 1) / spacing;
    const int gy = (H + spacing - 1) / spacing;
    const int n = gx * gy;

    voronoi_site *sites = (voronoi_site *)malloc((size_t)n * sizeof(voronoi_site));
    if (!sites)
        return -1;

    for (int b = 0; b < gy; ++b)
        for (int a = 0; a < gx; ++a) {
            int id = b * gx + a;
            uint64_t st = seed
                        ^ (0x9E3779B97F4A7C15ULL * (uint64_t)(id + 1))
                        ^ ((uint64_t)NOISE_LAYER_VORONOI << 56);
            double jx = (u01(&st) - 0.5) * jitter;
            double jy = (u01(&st) - 0.5) * jitter;
            double cx = ((double)a + 0.5 + jx) * (double)spacing;
            double cy = ((double)b + 0.5 + jy) * (double)spacing;
            if (cx < 0.0) cx = 0.0;
            if (cx > W - 1) cx = W - 1;
            if (cy < 0.0) cy = 0.0;
            if (cy > H - 1) cy = H - 1;
            sites[id].x = cx;
            sites[id].y = cy;
            sites[id].id = id;
        }

    /* Nearest-site label. With jitter <= 1 cell the nearest site is always
     * within a 2-cell (5x5) neighbourhood of the containing grid cell. */
    const int R = 2;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            double qx = (double)x, qy = (double)y;
            if (wx) {
                qx += warp_amp * noise_layer_sample(wx, x, y);
                qy += warp_amp * noise_layer_sample(wy, x, y);
                if (qx < 0.0) qx = 0.0;
                if (qx > W - 1) qx = W - 1;
                if (qy < 0.0) qy = 0.0;
                if (qy > H - 1) qy = H - 1;
            }
            int a = (int)qx / spacing, b = (int)qy / spacing;
            double best = 1e30;
            int bi = b * gx + a;
            for (int db = -R; db <= R; ++db)
                for (int da = -R; da <= R; ++da) {
                    int na = a + da, nb = b + db;
                    if (na < 0 || nb < 0 || na >= gx || nb >= gy) continue;
                    int id = nb * gx + na;
                    double dx = qx - sites[id].x;
                    double dy = qy - sites[id].y;
                    double d = dx * dx + dy * dy;
                    if (d < best) { best = d; bi = id; }
                }
            region[(size_t)x + (size_t)y * W] = bi;
        }
    noise_layer_free(wx);
    noise_layer_free(wy);

    vd->sites = sites;
    vd->n = n;
    vd->gx = gx;
    vd->gy = gy;
    vd->spacing = spacing;
    return 0;
}

void voronoi_free(voronoi_diagram *vd) {
    if (!vd)
        return;
    free(vd->sites);
    vd->sites = NULL;
    vd->n = vd->gx = vd->gy = 0;
}
