#include "uomappp/erosion.h"
#include "uomappp/noise.h"
#include "uomappp/field.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Tuned droplet constants (kept internal; the config exposes the knobs that
 * matter most for look: density, lifetime, radius, erode, deposition, z-scale). */
#define EROS_INERTIA     0.05
#define EROS_CAPACITY    4.0
#define EROS_EVAPORATION 0.01
#define EROS_GRAVITY     4.0
#define EROS_MIN_SLOPE   0.01
#define EROS_COAST_FADE  6.0   /* tiles: erosion fades out within this of the coast */

#define IS_OCEAN(c) ((c) == TCAT_WATER_DEEP || (c) == TCAT_WATER_SHALLOW)

static double randf01(uint64_t *st) {
    /* 53-bit mantissa uniform in [0,1). */
    return (double)(noise_splitmix64(st) >> 11) * (1.0 / 9007199254740992.0);
}

/* Bilinear height and gradient at continuous (px,py). The 4-corner cell is
 * clamped so (X,Y)..(X+1,Y+1) stay in bounds. */
static void height_grad(const float *hm, int W, int H, double px, double py,
                        double *h, double *gx, double *gy) {
    int X = (int)px, Y = (int)py;
    if (X < 0) X = 0;
    if (X > W - 2) X = W - 2;
    if (Y < 0) Y = 0;
    if (Y > H - 2) Y = H - 2;
    double u = px - X, v = py - Y;
    double hNW = hm[X + Y * W],       hNE = hm[(X + 1) + Y * W];
    double hSW = hm[X + (Y + 1) * W], hSE = hm[(X + 1) + (Y + 1) * W];
    *gx = (hNE - hNW) * (1.0 - v) + (hSE - hSW) * v;
    *gy = (hSW - hNW) * (1.0 - u) + (hSE - hNE) * u;
    *h  = hNW * (1.0 - u) * (1.0 - v) + hNE * u * (1.0 - v)
        + hSW * (1.0 - u) * v         + hSE * u * v;
}

int erosion_apply(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const size_t n = (size_t)W * (size_t)H;
    if (W < 2 || H < 2)
        return 0;

    long ndrops = (long)(cfg->erosion_density * (double)W * (double)H);
    if (ndrops <= 0)
        return 0;

    const int   life   = cfg->erosion_lifetime > 0 ? cfg->erosion_lifetime : 30;
    const int   radius = cfg->erosion_radius   > 0 ? cfg->erosion_radius   : 3;
    const double erode = cfg->erosion_erode;
    const double depos = cfg->erosion_deposition;
    const double zscale = cfg->erosion_z_scale;

    /* Working height field (hm) + original copy (src). Ocean is pinned to 0 so
     * droplets drain to the sea; land keeps its routing height (>=0). */
    float *hm  = (float *)malloc(n * sizeof(float));
    float *src = (float *)malloc(n * sizeof(float));
    uint8_t *coastmask = (uint8_t *)malloc(n);
    float *coast = (float *)malloc(n * sizeof(float));
    if (!hm || !src || !coastmask || !coast) {
        free(hm); free(src); free(coastmask); free(coast);
        fprintf(stderr, "warning: erosion skipped (out of memory)\n");
        return -1;
    }
    /* Erode the actual relief (z, 0..127 on land): it has real tile-scale
     * gradients, so droplets carve visible drainage. Ocean is pinned to 0 so
     * droplets flow to the sea. */
    for (size_t i = 0; i < n; ++i) {
        hm[i] = IS_OCEAN(g->cat[i]) ? 0.0f : (float)g->z[i];
        src[i] = hm[i];
        coastmask[i] = IS_OCEAN(g->cat[i]) ? 1 : 0;
    }
    /* Distance-to-coast so erosion fades out near the shore (no pockmarked
     * coasts); this is the field_distance_transform utility in action. */
    field_distance_transform(coastmask, coast, W, H);
    free(coastmask);

    /* Erosion brush: radius-r falloff weights, applied around the droplet cell
     * so erosion is spread rather than gouging a single pixel. */
    int brushN = 0;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            if (dx * dx + dy * dy <= radius * radius) ++brushN;
    int   *bx = (int *)malloc((size_t)brushN * sizeof(int));
    int   *by = (int *)malloc((size_t)brushN * sizeof(int));
    double *bw = (double *)malloc((size_t)brushN * sizeof(double));
    if (!bx || !by || !bw) {
        free(hm); free(src); free(coast); free(bx); free(by); free(bw);
        fprintf(stderr, "warning: erosion skipped (out of memory)\n");
        return -1;
    }
    {
        int k = 0; double wsum = 0.0;
        for (int dy = -radius; dy <= radius; ++dy)
            for (int dx = -radius; dx <= radius; ++dx) {
                double d2 = (double)(dx * dx + dy * dy);
                if (d2 > (double)(radius * radius)) continue;
                double w = 1.0 - sqrt(d2) / (double)radius;   /* linear falloff */
                bx[k] = dx; by[k] = dy; bw[k] = w; wsum += w; ++k;
            }
        for (int j = 0; j < brushN; ++j) bw[j] /= wsum;       /* normalize to 1 */
    }

    /* Deterministic droplet-spawn stream. */
    uint64_t st = cfg->seed ^ (0xD1B54A32D192ED03ULL *
                               (uint64_t)(NOISE_LAYER_EROSION + 1));

    for (long d = 0; d < ndrops; ++d) {
        double px = randf01(&st) * (double)(W - 1);
        double py = randf01(&st) * (double)(H - 1);
        double dx = 0.0, dy = 0.0, speed = 1.0, water = 1.0, sediment = 0.0;

        for (int step = 0; step < life; ++step) {
            int nodeX = (int)px, nodeY = (int)py;
            if (nodeX < 0 || nodeY < 0 || nodeX >= W || nodeY >= H) break;
            double u = px - nodeX, v = py - nodeY;

            double h, gx, gy;
            height_grad(hm, W, H, px, py, &h, &gx, &gy);

            /* New flow direction (blend old direction with the gradient). */
            dx = dx * EROS_INERTIA - gx * (1.0 - EROS_INERTIA);
            dy = dy * EROS_INERTIA - gy * (1.0 - EROS_INERTIA);
            double len = sqrt(dx * dx + dy * dy);
            if (len < 1e-8) break;            /* stuck in a flat / pit */
            dx /= len; dy /= len;

            double npx = px + dx, npy = py + dy;
            if (npx < 0.0 || npy < 0.0 || npx >= W - 1 || npy >= H - 1) break;

            int nnx = (int)npx, nny = (int)npy;
            if (IS_OCEAN(g->cat[nnx + nny * W])) {
                /* Reached the sea: drop remaining sediment and stop. */
                int bX = nodeX < W - 2 ? nodeX : W - 2;
                int bY = nodeY < H - 2 ? nodeY : H - 2;
                hm[bX + bY * W]             += (float)(sediment * (1 - u) * (1 - v));
                hm[(bX + 1) + bY * W]       += (float)(sediment * u * (1 - v));
                hm[bX + (bY + 1) * W]       += (float)(sediment * (1 - u) * v);
                hm[(bX + 1) + (bY + 1) * W] += (float)(sediment * u * v);
                break;
            }

            double nh, ngx, ngy;
            height_grad(hm, W, H, npx, npy, &nh, &ngx, &ngy);
            double dh = nh - h;   /* <0 downhill */

            double capacity = fmax(-dh, EROS_MIN_SLOPE) * speed * water * EROS_CAPACITY;

            if (sediment > capacity || dh > 0.0) {
                /* Deposit: fill the pit (dh>0) or shed excess sediment. */
                double dep = (dh > 0.0) ? fmin(dh, sediment)
                                        : (sediment - capacity) * depos;
                sediment -= dep;
                int bX = nodeX < W - 2 ? nodeX : W - 2;
                int bY = nodeY < H - 2 ? nodeY : H - 2;
                hm[bX + bY * W]             += (float)(dep * (1 - u) * (1 - v));
                hm[(bX + 1) + bY * W]       += (float)(dep * u * (1 - v));
                hm[bX + (bY + 1) * W]       += (float)(dep * (1 - u) * v);
                hm[(bX + 1) + (bY + 1) * W] += (float)(dep * u * v);
            } else {
                /* Erode: remove up to the slope height, faded near the coast,
                 * spread over the brush. */
                double fade = coast[nodeX + nodeY * W] / EROS_COAST_FADE;
                if (fade > 1.0) fade = 1.0;
                double ero = fmin((capacity - sediment) * erode * fade, -dh);
                if (ero > 0.0) {
                    double removed = 0.0;
                    for (int j = 0; j < brushN; ++j) {
                        int cx = nodeX + bx[j], cy = nodeY + by[j];
                        if (cx < 0 || cy < 0 || cx >= W || cy >= H) continue;
                        int ci = cx + cy * W;
                        if (IS_OCEAN(g->cat[ci])) continue;   /* never gouge the sea */
                        double take = ero * bw[j];
                        hm[ci] -= (float)take;
                        removed += take;
                    }
                    sediment += removed;
                }
            }

            speed = sqrt(fmax(0.0, speed * speed + (-dh) * EROS_GRAVITY));
            water *= (1.0 - EROS_EVAPORATION);
            px = npx; py = npy;
        }
    }
    free(bx); free(by); free(bw); free(coast);

    /* Carve delta = eroded - original; gently smooth it (field_blur) so the
     * result reads as valleys rather than noise, then fold it back into the
     * routing height and relief. Delta is ~0 over ocean, so smoothing does not
     * pull the coastline down. */
    for (size_t i = 0; i < n; ++i) hm[i] -= src[i];   /* hm now holds the delta */
    field_blur(hm, W, H, 1, 1);

    long lowered = 0, raised = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)x + (size_t)y * W;
            if (IS_OCEAN(g->cat[i])) continue;
            double delta = hm[i];
            g->hfield[i] = src[i] + (float)delta;     /* eroded routing height */
            if (g->cat[i] == TCAT_MOUNTAIN) continue;  /* keep peaks dramatic */
            int dz = (int)lround(zscale * delta);
            int z = (int)g->z[i] + dz;
            if (z < 0) z = 0;
            if (z > 127) z = 127;
            if (dz < 0) ++lowered; else if (dz > 0) ++raised;
            g->z[i] = (int8_t)z;
        }

    free(hm); free(src);
    fprintf(stderr, "erosion: %ld droplets, %ld cells lowered, %ld raised\n",
            ndrops, lowered, raised);
    return 0;
}
