#include "uomappp/terrain.h"
#include "uomappp/noise.h"
#include "uomappp/biome.h"
#include "uomappp/preview.h"
#include "uomappp/erosion.h"
#include "uomappp/voronoi.h"
#include "uomappp/marching.h"
#include "uomappp/mst.h"
#include "uomappp/cellular.h"
#include "uomappp/wfc.h"
#include "uomappp/poisson.h"
#include "uomappp/astar.h"
#include "uomappp/bsp.h"
#include "uomappp/lsystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Deterministic per-cell hash (for tile variation). */
static uint64_t cell_hash(uint64_t seed, int x, int y, uint32_t salt) {
    uint64_t s = seed
               ^ (0x100000001B3ULL * (uint64_t)(uint32_t)x)
               ^ (0xC2B2AE3D27D4EB4FULL * (uint64_t)(uint32_t)y)
               ^ ((uint64_t)salt << 48);
    return noise_splitmix64(&s);
}

/*
 * Centralized land-tile palette (classic UO land tile IDs, verified against the
 * reference client's tiledata.mul). Keep them here so they are easy to tune.
 */
#define TILE_WATER_DEEP     0x00A8  /* ocean (Wet) */
#define TILE_WATER_SHALLOW  0x00AB  /* ocean variant */
#define TILE_RIVER          0x00A8  /* rivers rendered with ocean water */
#define TILE_SAND           0x0016  /* coast sand */
#define TILE_GRASS          0x0003  /* grass */
#define TILE_FOREST         0x00C4  /* forest */
#define TILE_HILL           0x0071  /* dirt / high ground */
#define TILE_MOUNTAIN       0x00E4  /* rock (impassable -> true mountain barrier) */

static uint16_t tile_for_cat(int cat) {
    switch (cat) {
        case TCAT_WATER_DEEP:    return TILE_WATER_DEEP;
        case TCAT_WATER_SHALLOW: return TILE_WATER_SHALLOW;
        case TCAT_RIVER:         return TILE_RIVER;
        case TCAT_SAND:          return TILE_SAND;
        case TCAT_GRASS:         return TILE_GRASS;
        case TCAT_FOREST:        return TILE_FOREST;
        case TCAT_HILL:          return TILE_HILL;
        case TCAT_MOUNTAIN:      return TILE_MOUNTAIN;
        default:                 return TILE_GRASS;
    }
}

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

#define IS_WATER_CAT(c) ((c) == TCAT_WATER_DEEP || (c) == TCAT_WATER_SHALLOW || \
                         (c) == TCAT_RIVER || (c) == TCAT_LAKE)
#define IS_OCEAN_CAT(c) ((c) == TCAT_WATER_DEEP || (c) == TCAT_WATER_SHALLOW)
#define IS_FIXED_CAT(c) (IS_WATER_CAT(c) || (c) == TCAT_MOUNTAIN)

/* Slope-limiting relaxation so adjacent WALKABLE land cells never differ by
 * more than cfg->max_slope in z. Water, rivers and mountains are left as-is
 * (mountains stay steep, water/rivers stay flat). Two fixed-order passes keep
 * this fully deterministic. */
static void limit_slope(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const int step = cfg->max_slope < 1 ? 1 : cfg->max_slope;

    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 0) {
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    int i = x + y * W;
                    if (IS_FIXED_CAT(g->cat[i])) continue;
                    int zi = g->z[i];
                    if (x > 0) { int n = g->z[i - 1]; if (zi > n + step) zi = n + step; }
                    if (y > 0) { int n = g->z[i - W]; if (zi > n + step) zi = n + step; }
                    g->z[i] = (int8_t)zi;
                }
        } else {
            for (int y = H - 1; y >= 0; --y)
                for (int x = W - 1; x >= 0; --x) {
                    int i = x + y * W;
                    if (IS_FIXED_CAT(g->cat[i])) continue;
                    int zi = g->z[i];
                    if (x < W - 1) { int n = g->z[i + 1]; if (zi > n + step) zi = n + step; }
                    if (y < H - 1) { int n = g->z[i + W]; if (zi > n + step) zi = n + step; }
                    g->z[i] = (int8_t)zi;
                }
        }
    }
}

static void validate_palette(const tiledata_land *td) {
    if (!td->loaded)
        return;
    if (!tiledata_is_wet(td, TILE_WATER_DEEP))
        fprintf(stderr, "warning: water tile 0x%04X is not flagged Wet in tiledata\n", TILE_WATER_DEEP);
    /* Note: TILE_MOUNTAIN (rock) is intentionally Impassable. */
    const uint16_t land[] = { TILE_SAND, TILE_GRASS, TILE_FOREST, TILE_HILL };
    for (size_t k = 0; k < sizeof(land) / sizeof(land[0]); ++k)
        if (tiledata_is_impassable(td, land[k]))
            fprintf(stderr, "warning: land tile 0x%04X is flagged Impassable in tiledata\n", land[k]);
}

/* ------------------------------------------------------------------------- */

typedef struct { double x, y; } vec2;

/* True if any 8-neighbour of (x,y) is a mountain cell. */
static int adjacent_to_mountain(const terrain_grid *g, int x, int y) {
    const int W = g->width, H = g->height;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (!dx && !dy) continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            if (g->cat[nx + ny * W] == TCAT_MOUNTAIN) return 1;
        }
    return 0;
}

/* Deterministically place continent_count centers on an ellipse inscribed in
 * the map (a triangle for n=3), seed-rotated. This spreads them in 2D so they
 * stay well separated with ocean between, rather than merging along one axis.
 * n==1 is placed at the center. */
static void place_centers(const mapgen_config *cfg, vec2 *c, int n) {
    uint64_t s = cfg->seed ^ 0xC0FFEE123456789ULL;
    double phase = (double)(noise_splitmix64(&s) >> 11) / 9007199254740992.0 * 6.2831853;
    double cx = cfg->width * 0.5, cy = cfg->height * 0.5;
    if (n == 1) { c[0].x = cx; c[0].y = cy; return; }
    double a = cfg->width * 0.26, b = cfg->height * 0.27;
    for (int k = 0; k < n; ++k) {
        double ang = phase + k * (6.2831853 / (double)n);
        c[k].x = cx + a * cos(ang);
        c[k].y = cy + b * sin(ang);
    }
}

/*
 * River carving: pick high-ground sources spread across a coarse grid, then
 * trace each one strictly downhill over the height field, marking a ~3-wide
 * water channel until it reaches the sea (or a local pit). Rivers merge where
 * paths meet, giving a dendritic network. Deterministic throughout.
 */
static void carve_rivers(terrain_grid *g, const mapgen_config *cfg,
                         const float *hf) {
    const int W = g->width, H = g->height;
    const int S = 96;                 /* coarse source-grid cell size */
    const int gx = (W + S - 1) / S, gy = (H + S - 1) / S;

    /* Meander steering: a smooth noise field rotates each downhill step so the
     * river wanders laterally instead of running straight down the slope. */
    noise_layer *mnd = noise_layer_create(cfg->seed, NOISE_LAYER_MEANDER, 0.012, 2);
    const double MEANDER_MAX = 1.15;  /* max angular deflection (radians) */

    /* Ford anchors: one crossing every FORD_STEP cells along each river path.
     * Each becomes a small compact land bridge (not a long sandbar). */
    const int FORD_STEP = 120;
    int *fordAnchors = NULL; int nFord = 0, capFord = 0;
    int *lakeAnchors = NULL; int nLake = 0, capLake = 0;  /* river sinks -> ponds */

    /* Collect one highest candidate per coarse cell (mountain if available). */
    typedef struct { float h; int idx; } cand;
    cand *cs = (cand *)malloc((size_t)gx * gy * sizeof(cand));
    if (!cs) { noise_layer_free(mnd); return; }
    int nc = 0;
    for (int cy = 0; cy < gy; ++cy)
        for (int cx = 0; cx < gx; ++cx) {
            /* Rivers never originate in or run through mountains. Prefer a
             * "spring" at a mountain's foot (highest non-mountain cell that
             * touches a range); otherwise fall back to the highest high-ground
             * cell in this grid cell. */
            float best = -1e30f;  int bi = -1;
            float bestF = -1e30f; int biF = -1;
            for (int y = cy * S; y < (cy + 1) * S && y < H; ++y)
                for (int x = cx * S; x < (cx + 1) * S && x < W; ++x) {
                    int i = x + y * W;
                    if (IS_WATER_CAT(g->cat[i]) || g->cat[i] == TCAT_MOUNTAIN)
                        continue;
                    if (hf[i] > best) { best = hf[i]; bi = i; }
                    if (adjacent_to_mountain(g, x, y) && hf[i] > bestF) {
                        bestF = hf[i]; biF = i;
                    }
                }
            if (biF >= 0)
                cs[nc++] = (cand){ bestF, biF };      /* mountain-fed spring */
            else if (bi >= 0 && best > 0.55f)
                cs[nc++] = (cand){ best, bi };         /* high-ground source */
        }

    /* Sort candidates by height descending (simple insertion on the modest
     * count; deterministic). */
    for (int a = 1; a < nc; ++a) {
        cand key = cs[a]; int b = a - 1;
        while (b >= 0 && cs[b].h < key.h) { cs[b + 1] = cs[b]; --b; }
        cs[b + 1] = key;
    }

    int cap = cfg->river_density > 0 ? cfg->river_density : (W + H) / 400;
    if (cap > nc) cap = nc;

    long carved = 0, reached = 0;
    for (int s = 0; s < cap; ++s) {
        int cur = cs[s].idx;
        int got_sea = 0;
        int prevx = cur % W, prevy = cur / W;
        for (int step = 0; step < W + H; ++step) {
            int cx = cur % W, cy = cur / W;
            if (IS_WATER_CAT(g->cat[cur]) && g->cat[cur] != TCAT_RIVER) {
                got_sea = 1;
                break;                     /* reached the sea */
            }
            /* carve this cell */
            if (g->cat[cur] != TCAT_RIVER) {
                g->cat[cur] = TCAT_RIVER;
                g->id[cur]  = TILE_RIVER;
                g->z[cur]   = (int8_t)clampi(g->z[cur] - 2, -128, 127);
                ++carved;
            }
            /* record a ford anchor every FORD_STEP cells along the path */
            if (step > 0 && step % FORD_STEP == 0) {
                if (nFord == capFord) {
                    int nc2 = capFord ? capFord * 2 : 256;
                    int *tmp = (int *)realloc(fordAnchors, (size_t)nc2 * sizeof(int));
                    if (tmp) { fordAnchors = tmp; capFord = nc2; }
                }
                if (nFord < capFord) fordAnchors[nFord++] = cur;
            }
            /* find the lowest neighbor (8-dir) by routing height: this gives the
             * natural flow direction and detects pits. */
            int best = -1; float bh = 1e30f;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    int nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int ni = nx + ny * W;
                    if (g->cat[ni] == TCAT_MOUNTAIN) continue; /* never flow into a range */
                    if (hf[ni] < bh) { bh = hf[ni]; best = ni; }
                }
            if (best < 0) break;
            /* If the lowest reachable neighbor is not lower (beyond a small
             * tolerance for coastline-warp dips), the river ends in a sink: it
             * stops here and (if lakes are on) this spot becomes a pond. */
            if (bh > hf[cur] + 0.01f) {
                if (cfg->lakes) {
                    if (nLake == capLake) {
                        int nc2 = capLake ? capLake * 2 : 64;
                        int *tmp = (int *)realloc(lakeAnchors, (size_t)nc2 * sizeof(int));
                        if (tmp) { lakeAnchors = tmp; capLake = nc2; }
                    }
                    if (nLake < capLake) lakeAnchors[nLake++] = cur;
                }
                break;
            }

            /* Meander: rotate the steepest-descent direction by a smooth
             * noise-driven angle, then flow to the downhill neighbour best
             * aligned with that heading. Every step still descends (within
             * tolerance), so rivers keep reaching the sea while winding. */
            double baseAng = atan2((double)(best / W - cy), (double)(best % W - cx));
            double ang = baseAng + MEANDER_MAX * noise_layer_sample(mnd, cx, cy);
            double dxw = cos(ang), dyw = sin(ang);
            int chosen = best; double bestScore = -1e30;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    int nx2 = cx + dx, ny2 = cy + dy;
                    if (nx2 < 0 || ny2 < 0 || nx2 >= W || ny2 >= H) continue;
                    int ni = nx2 + ny2 * W;
                    if (g->cat[ni] == TCAT_MOUNTAIN) continue;
                    if (hf[ni] > hf[cur] + 0.01f) continue;   /* keep descending */
                    double len = (dx && dy) ? 0.70710678 : 1.0;
                    double score = ((double)dx * dxw + (double)dy * dyw) * len;
                    if (score > bestScore) { bestScore = score; chosen = ni; }
                }

            if (g->cat[chosen] == TCAT_RIVER) break;  /* merged into another river */
            /* widen: mark the two cells perpendicular to flow as river banks */
            int nx = chosen % W, ny = chosen / W;
            int ddx = nx - cx, ddy = ny - cy;
            int px = -ddy, py = ddx;       /* perpendicular */
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                int wx = cx + px * sgn, wy = cy + py * sgn;
                if (wx < 0 || wy < 0 || wx >= W || wy >= H) continue;
                int wi = wx + wy * W;
                if (!IS_WATER_CAT(g->cat[wi]) && g->cat[wi] != TCAT_MOUNTAIN) {
                    g->cat[wi] = TCAT_RIVER;
                    g->id[wi]  = TILE_RIVER;
                    g->z[wi]   = (int8_t)clampi(g->z[wi] - 2, -128, 127);
                }
            }
            prevx = cx; prevy = cy; (void)prevx; (void)prevy;
            cur = chosen;
        }
        if (got_sea) ++reached;
    }

    /* Widen rivers by one cell (4-neighbour dilation) so channels read clearly
     * and look like real rivers. Uses a sentinel so it stays a single ring and
     * does not cascade within the pass. */
    const uint8_t PEND = 250;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int i = x + y * W;
            if (IS_WATER_CAT(g->cat[i]) || g->cat[i] == TCAT_MOUNTAIN) continue;
            int near = 0;
            if (x > 0     && g->cat[i - 1] == TCAT_RIVER) near = 1;
            else if (x < W-1 && g->cat[i + 1] == TCAT_RIVER) near = 1;
            else if (y > 0     && g->cat[i - W] == TCAT_RIVER) near = 1;
            else if (y < H-1 && g->cat[i + W] == TCAT_RIVER) near = 1;
            if (near) g->cat[i] = PEND;
        }
    for (size_t i = 0; i < (size_t)W * H; ++i)
        if (g->cat[i] == PEND) {
            g->cat[i] = TCAT_RIVER;
            g->id[i]  = TILE_RIVER;
            g->z[i]   = (int8_t)clampi(g->z[i] - 1, -128, 127);
        }

    /* Fords: a compact crossing at each anchor so no area is landlocked by a
     * river, without long sandbars or bridges to nowhere. Each ford floods the
     * connected river cells within a small radius back to passable land (so it
     * spans the channel bank-to-bank and nothing more). Anchors within a few
     * cells of the open sea are skipped, so there are no sandbars at river
     * mouths. */
    const int FORD_RADIUS = 3;
    long fords = 0, fordsPlaced = 0;
    int q[512], qd[512];
    for (int a = 0; a < nFord; ++a) {
        int start = fordAnchors[a];
        if (g->cat[start] != TCAT_RIVER) continue;   /* already forded/merged */

        /* skip anchors near open ocean (would make a sandbar at the mouth) */
        int sx = start % W, sy = start / W, nearSea = 0;
        for (int dy = -FORD_RADIUS - 1; dy <= FORD_RADIUS + 1 && !nearSea; ++dy)
            for (int dx = -FORD_RADIUS - 1; dx <= FORD_RADIUS + 1; ++dx) {
                int nx = sx + dx, ny = sy + dy;
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                int c = g->cat[nx + ny * W];
                if (c == TCAT_WATER_DEEP || c == TCAT_WATER_SHALLOW) { nearSea = 1; break; }
            }
        if (nearSea) continue;

        int qh = 0, qt = 0;
        q[qt] = start; qd[qt] = 0; qt++;
        while (qh < qt) {
            int c = q[qh]; int d = qd[qh]; qh++;
            if (g->cat[c] != TCAT_RIVER) continue;
            int cxc = c % W, cyc = c / W;
            int bz = g->z[c] + 2; int found = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    int nx = cxc + dx, ny = cyc + dy;
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int ni = nx + ny * W;
                    if (!IS_WATER_CAT(g->cat[ni]) && g->cat[ni] != TCAT_MOUNTAIN) {
                        if (!found || g->z[ni] > bz) { bz = g->z[ni]; found = 1; }
                    }
                }
            g->cat[c] = TCAT_SAND;
            g->id[c]  = tile_for_cat(TCAT_SAND);
            g->z[c]   = (int8_t)clampi(bz, -128, 127);
            ++fords;
            if (d < FORD_RADIUS) {
                int nb[4] = { c - 1, c + 1, c - W, c + W };
                for (int k = 0; k < 4; ++k) {
                    int n = nb[k];
                    if (n < 0 || n >= W * H) continue;
                    if (g->cat[n] == TCAT_RIVER && qt < 512) { q[qt] = n; qd[qt] = d + 1; qt++; }
                }
            }
        }
        ++fordsPlaced;
    }
    free(fordAnchors);

    /* Lakes: flood a small pond at each inland river sink (not near ocean), so
     * rivers that don't reach the sea feed a lake instead of just stopping. */
    long lakesPlaced = 0;
    for (int a = 0; a < nLake; ++a) {
        int start = lakeAnchors[a];
        if (IS_OCEAN_CAT(g->cat[start])) continue;
        int sx = start % W, sy = start / W, nearSea = 0;
        for (int dy = -4; dy <= 4 && !nearSea; ++dy)
            for (int dx = -4; dx <= 4; ++dx) {
                int nx = sx + dx, ny = sy + dy;
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                if (IS_OCEAN_CAT(g->cat[nx + ny * W])) { nearSea = 1; break; }
            }
        if (nearSea) continue;
        int lz = g->z[start];
        int qh = 0, qt = 0;
        q[qt] = start; qd[qt] = 0; qt++;
        while (qh < qt) {
            int c = q[qh]; int d = qd[qh]; qh++;
            if (g->cat[c] == TCAT_MOUNTAIN || IS_OCEAN_CAT(g->cat[c])) continue;
            g->cat[c] = TCAT_LAKE;
            g->id[c]  = biome_tile(TCAT_LAKE, cell_hash(cfg->seed, c % W, c / W, NOISE_LAYER_BIOME));
            g->z[c]   = (int8_t)clampi(lz, -128, 127);
            ++lakesPlaced;
            if (d < 2) {
                int nb[4] = { c - 1, c + 1, c - W, c + W };
                for (int k = 0; k < 4; ++k) {
                    int nn = nb[k];
                    if (nn < 0 || nn >= W * H) continue;
                    int nc3 = g->cat[nn];
                    if (nc3 != TCAT_MOUNTAIN && !IS_OCEAN_CAT(nc3) && nc3 != TCAT_LAKE
                        && qt < 512) { q[qt] = nn; qd[qt] = d + 1; qt++; }
                }
            }
        }
    }
    free(lakeAnchors);

    fprintf(stderr, "rivers: %d sources, %ld cells carved, %ld reached the sea, %ld fords (%ld cells), %ld lakes\n",
            cap, carved, reached, fordsPlaced, fords, lakesPlaced);
    noise_layer_free(mnd);
    free(cs);
}

/* Sloped sand beaches around every ocean coast: land within beach_width of the
 * sea becomes sand, with z ramped from the shore (water_z) up to the inland
 * height, so coasts slope instead of cliffing. Rivers/lakes/mountains excluded. */
static void beach_pass(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const size_t n = (size_t)W * (size_t)H;
    const int bw = cfg->beach_width;
    if (bw < 1) return;
    uint8_t *dist = (uint8_t *)malloc(n);
    if (!dist) return;
    for (size_t i = 0; i < n; ++i) dist[i] = IS_OCEAN_CAT(g->cat[i]) ? 0 : 255;

    for (int d = 1; d <= bw; ++d)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int i = x + y * W;
                if (dist[i] != 255) continue;
                int c = g->cat[i];
                if (c == TCAT_MOUNTAIN || IS_WATER_CAT(c)) continue;
                int adj = (x > 0 && dist[i-1] == d-1) || (x < W-1 && dist[i+1] == d-1)
                       || (y > 0 && dist[i-W] == d-1) || (y < H-1 && dist[i+W] == d-1);
                if (!adj) continue;
                dist[i] = (uint8_t)d;
                double f = (double)d / (double)bw;      /* shore .. inland */
                int zl = g->z[i];
                int zb = cfg->water_z + (int)lround((zl - cfg->water_z) * f);
                g->cat[i] = TCAT_SAND;
                g->id[i]  = biome_tile(TCAT_SAND, cell_hash(cfg->seed, x, y, NOISE_LAYER_BIOME));
                g->z[i]   = (int8_t)clampi(zb, -128, 127);
            }
    free(dist);
}

static int is_walkable_land(const terrain_grid *g, int i) {
    int c = g->cat[i];
    return !(IS_WATER_CAT(c) || c == TCAT_MOUNTAIN);
}

/* Mountain passes: carve ~3-wide walkable corridors (dirt) through mountain
 * bands up to MAXTHICK thick at regular intervals along both axes, so interior
 * valleys aren't sealed off. Heuristic but bounded and deterministic. */
static void carve_passes(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const int SPACING = 160, MAXTHICK = 48;

    for (int y = SPACING / 2; y < H; y += SPACING) {
        int x = 0;
        while (x < W) {
            int i = x + y * W;
            if (g->cat[i] == TCAT_MOUNTAIN && x > 0 && is_walkable_land(g, i - 1)) {
                int zL = g->z[i - 1], e = x;
                while (e < W && g->cat[e + y * W] == TCAT_MOUNTAIN && (e - x) < MAXTHICK) ++e;
                if (e < W && e > x && is_walkable_land(g, e + y * W)) {
                    int zR = g->z[e + y * W], len = e - x;
                    for (int p = x; p < e; ++p) {
                        double f = (double)(p - x + 1) / (double)(len + 1);
                        int zz = zL + (int)lround((zR - zL) * f);
                        for (int yy = y - 1; yy <= y + 1; ++yy) {
                            if (yy < 0 || yy >= H) continue;
                            int j = p + yy * W;
                            if (g->cat[j] == TCAT_MOUNTAIN) {
                                g->cat[j] = TCAT_HILL;
                                g->id[j]  = biome_tile(TCAT_HILL, cell_hash(cfg->seed, p, yy, NOISE_LAYER_BIOME));
                                g->z[j]   = (int8_t)clampi(zz, -128, 127);
                            }
                        }
                    }
                }
                x = e > x ? e : x + 1;
                continue;
            }
            ++x;
        }
    }
    for (int x = SPACING / 2; x < W; x += SPACING) {
        int y = 0;
        while (y < H) {
            int i = x + y * W;
            if (g->cat[i] == TCAT_MOUNTAIN && y > 0 && is_walkable_land(g, i - W)) {
                int zT = g->z[i - W], e = y;
                while (e < H && g->cat[x + e * W] == TCAT_MOUNTAIN && (e - y) < MAXTHICK) ++e;
                if (e < H && e > y && is_walkable_land(g, x + e * W)) {
                    int zB = g->z[x + e * W], len = e - y;
                    for (int p = y; p < e; ++p) {
                        double f = (double)(p - y + 1) / (double)(len + 1);
                        int zz = zT + (int)lround((zB - zT) * f);
                        for (int xx = x - 1; xx <= x + 1; ++xx) {
                            if (xx < 0 || xx >= W) continue;
                            int j = xx + p * W;
                            if (g->cat[j] == TCAT_MOUNTAIN) {
                                g->cat[j] = TCAT_HILL;
                                g->id[j]  = biome_tile(TCAT_HILL, cell_hash(cfg->seed, xx, p, NOISE_LAYER_BIOME));
                                g->z[j]   = (int8_t)clampi(zz, -128, 127);
                            }
                        }
                    }
                }
                y = e > y ? e : y + 1;
                continue;
            }
            ++y;
        }
    }
}

/* Climate-biome categories eligible for territorial reassignment (elevation
 * features — hills/mountains/sand/water — are left alone). */
static int is_climate_cat(int c) {
    return c == TCAT_GRASS || c == TCAT_FOREST || c == TCAT_DESERT ||
           c == TCAT_JUNGLE || c == TCAT_SWAMP || c == TCAT_SNOW;
}

/*
 * Regions pass (Voronoi territories + marching-squares borders + MST graph).
 * Partitions the map into organic territories, gives each a climate (so biomes
 * become coherent patches with Voronoi borders instead of latitude bands),
 * marks region borders into g->flags, and builds the MST connectivity graph
 * over the territories (reported now; roads will route it in Phase 5).
 * g->region and g->flags persist on the grid for later passes.
 */
static void regions_pass(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const size_t n = (size_t)W * (size_t)H;

    g->region = (int32_t *)malloc(n * sizeof(int32_t));
    g->flags  = (uint8_t *)malloc(n);
    if (!g->region || !g->flags) {
        free(g->region); g->region = NULL;
        free(g->flags);  g->flags  = NULL;
        fprintf(stderr, "warning: regions skipped (out of memory)\n");
        return;
    }

    voronoi_diagram vd;
    if (voronoi_build(&vd, g->region, W, H, cfg->seed,
                      cfg->region_spacing, cfg->region_jitter) != 0) {
        free(g->region); g->region = NULL;
        free(g->flags);  g->flags  = NULL;
        fprintf(stderr, "warning: regions skipped (voronoi alloc failed)\n");
        return;
    }
    const int ns = vd.n;

    /* Per-region climate: latitude temperature at the site + a per-region jitter
     * and moisture draw (deterministic in the region id). */
    double *rtemp = (double *)malloc((size_t)ns * sizeof(double));
    double *rmoist = (double *)malloc((size_t)ns * sizeof(double));
    if (rtemp && rmoist) {
        for (int id = 0; id < ns; ++id) {
            uint64_t s = cfg->seed
                       ^ (0xC2B2AE3D27D4EB4FULL * (uint64_t)(id + 1))
                       ^ ((uint64_t)NOISE_LAYER_VORONOI << 40);
            double mo = (double)(noise_splitmix64(&s) >> 11) / 9007199254740992.0;
            double tj = (double)(noise_splitmix64(&s) >> 11) / 9007199254740992.0;
            double ny2 = (H > 1) ? vd.sites[id].y / (double)(H - 1) : 0.5;
            double lat = 1.0 - 2.0 * fabs(ny2 - 0.5);          /* 0 poles .. 1 centre */
            rtemp[id]  = (lat * 2.0 - 1.0) * 0.65 + (tj * 2.0 - 1.0) * 0.25
                       + cfg->temperature_bias;
            rmoist[id] = mo * 2.0 - 1.0;                        /* match noise scale */
        }

        const double zmax = cfg->land_z_max > 0 ? (double)cfg->land_z_max : 1.0;
        long reclassified = 0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                size_t i = (size_t)x + (size_t)y * W;
                int c = g->cat[i];
                if (!is_climate_cat(c)) continue;
                int id = g->region[i];
                double hh = (double)g->z[i] / zmax;
                if (hh < 0.0) hh = 0.0;
                if (hh > 1.0) hh = 1.0;
                int nc = biome_classify(hh, rtemp[id], rmoist[id]);
                if (nc != c) {
                    g->cat[i] = (uint8_t)nc;
                    g->id[i]  = biome_tile(nc, cell_hash(cfg->seed, x, y, NOISE_LAYER_BIOME));
                    ++reclassified;
                }
            }

        /* Region borders -> g->flags (for later tile-transition passes). */
        long borders = marching_squares_borders(g->region, g->flags, W, H);

        /* MST over the territory graph (jittered-grid adjacency). */
        const int gx = vd.gx, gy = vd.gy;
        int cap = 4 * ns + 8;
        mst_edge *edges = (mst_edge *)malloc((size_t)cap * sizeof(mst_edge));
        mst_edge *tree  = (mst_edge *)malloc((size_t)(ns > 0 ? ns : 1) * sizeof(mst_edge));
        long treeN = 0; double treeLen = 0.0;
        if (edges && tree) {
            int ne = 0;
            for (int b = 0; b < gy; ++b)
                for (int a = 0; a < gx; ++a) {
                    int id = b * gx + a;
                    int nb[4][2] = { {a + 1, b}, {a, b + 1}, {a + 1, b + 1}, {a - 1, b + 1} };
                    for (int k = 0; k < 4; ++k) {
                        int na = nb[k][0], mb = nb[k][1];
                        if (na < 0 || mb < 0 || na >= gx || mb >= gy) continue;
                        int jd = mb * gx + na;
                        if (ne >= cap) continue;
                        double dx = vd.sites[id].x - vd.sites[jd].x;
                        double dy = vd.sites[id].y - vd.sites[jd].y;
                        edges[ne].a = id; edges[ne].b = jd;
                        edges[ne].w = sqrt(dx * dx + dy * dy);
                        ++ne;
                    }
                }
            int k = mst_build(edges, ne, ns, tree);
            if (k > 0) {
                treeN = k;
                for (int e = 0; e < k; ++e) treeLen += tree[e].w;
            }
        }
        free(edges); free(tree);

        fprintf(stderr,
            "regions: %d territories (spacing %d), %ld cells reclassified, "
            "%ld border cells, MST %ld edges (%.0f tiles)\n",
            ns, vd.spacing, reclassified, borders, treeN, treeLen);
    }
    free(rtemp); free(rmoist);
    voronoi_free(&vd);
}

/* --- Phase 4: WFC biome transitions + cellular forest clumps -------------- */

/* WFC biome classes and their TCAT mapping. */
enum { WC_GRASS = 0, WC_FOREST, WC_DESERT, WC_JUNGLE, WC_SWAMP, WC_SNOW, WC_N };
static const int WC_TCAT[WC_N] = {
    TCAT_GRASS, TCAT_FOREST, TCAT_DESERT, TCAT_JUNGLE, TCAT_SWAMP, TCAT_SNOW
};
static int tcat_to_class(int c) {
    switch (c) {
        case TCAT_FOREST: return WC_FOREST;
        case TCAT_DESERT: return WC_DESERT;
        case TCAT_JUNGLE: return WC_JUNGLE;
        case TCAT_SWAMP:  return WC_SWAMP;
        case TCAT_SNOW:   return WC_SNOW;
        default:          return WC_GRASS;
    }
}

/*
 * WFC pass: assign each Voronoi territory a biome so neighbouring territories
 * only meet along legal transitions (GRASS is the universal glue; e.g. desert
 * never directly touches snow). Each region prefers its climate biome (high
 * weight) but WFC inserts transition biomes where adjacency forbids a direct
 * meeting. Requires the region map from the regions pass; on a WFC contradiction
 * the climate biomes are kept unchanged.
 */
static void wfc_pass(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const size_t n = (size_t)W * (size_t)H;
    if (!g->region) {
        fprintf(stderr, "warning: wfc skipped (regions not enabled)\n");
        return;
    }

    /* Rebuild the Voronoi sites (deterministic, identical to the regions pass)
     * to recover site positions and the coarse-grid topology. */
    int32_t *scratch = (int32_t *)malloc(n * sizeof(int32_t));
    voronoi_diagram vd;
    if (!scratch ||
        voronoi_build(&vd, scratch, W, H, cfg->seed,
                      cfg->region_spacing, cfg->region_jitter) != 0) {
        free(scratch);
        fprintf(stderr, "warning: wfc skipped (voronoi alloc failed)\n");
        return;
    }
    free(scratch);
    const int ns = vd.n, gx = vd.gx, gy = vd.gy;

    /* Biome-class compatibility (symmetric). GRASS is adjacent to everything. */
    static const int compat[WC_N][WC_N] = {
        /* GRASS  */ {1,1,1,1,1,1},
        /* FOREST */ {1,1,0,1,1,1},
        /* DESERT */ {1,0,1,0,0,0},
        /* JUNGLE */ {1,1,0,1,1,0},
        /* SWAMP  */ {1,1,0,1,1,0},
        /* SNOW   */ {1,1,0,0,0,1},
    };
    uint8_t allowed[WC_N * WC_N];
    for (int a = 0; a < WC_N; ++a)
        for (int b = 0; b < WC_N; ++b)
            allowed[a * WC_N + b] = (uint8_t)compat[a][b];

    /* Per-region climate preference -> per-node weights. */
    double *weight = (double *)malloc((size_t)ns * WC_N * sizeof(double));
    int    *outc   = (int *)malloc((size_t)ns * sizeof(int));
    int    *adjStart = (int *)malloc((size_t)(ns + 1) * sizeof(int));
    int    *deg      = (int *)calloc((size_t)ns, sizeof(int));
    if (!weight || !outc || !adjStart || !deg) {
        free(weight); free(outc); free(adjStart); free(deg); voronoi_free(&vd);
        fprintf(stderr, "warning: wfc skipped (out of memory)\n");
        return;
    }
    const double BONUS = 8.0;
    for (int id = 0; id < ns; ++id) {
        uint64_t s = cfg->seed
                   ^ (0xC2B2AE3D27D4EB4FULL * (uint64_t)(id + 1))
                   ^ ((uint64_t)NOISE_LAYER_VORONOI << 40);
        double mo = (double)(noise_splitmix64(&s) >> 11) / 9007199254740992.0;
        double tj = (double)(noise_splitmix64(&s) >> 11) / 9007199254740992.0;
        double ny2 = (H > 1) ? vd.sites[id].y / (double)(H - 1) : 0.5;
        double lat = 1.0 - 2.0 * fabs(ny2 - 0.5);
        double temp = (lat * 2.0 - 1.0) * 0.65 + (tj * 2.0 - 1.0) * 0.25
                    + cfg->temperature_bias;
        int pref = tcat_to_class(biome_classify(0.4, temp, mo * 2.0 - 1.0));
        for (int t = 0; t < WC_N; ++t)
            weight[id * WC_N + t] = 1.0 + (t == pref ? BONUS : 0.0);
    }

    /* CSR adjacency over the coarse grid (4-neighbour, undirected). */
    for (int b = 0; b < gy; ++b)
        for (int a = 0; a < gx; ++a) {
            int id = b * gx + a;
            if (a > 0)      ++deg[id];
            if (a < gx - 1) ++deg[id];
            if (b > 0)      ++deg[id];
            if (b < gy - 1) ++deg[id];
        }
    adjStart[0] = 0;
    for (int i = 0; i < ns; ++i) adjStart[i + 1] = adjStart[i] + deg[i];
    int total = adjStart[ns];
    int *adjList = (int *)malloc((size_t)(total > 0 ? total : 1) * sizeof(int));
    int *cur = (int *)malloc((size_t)ns * sizeof(int));
    if (!adjList || !cur) {
        free(weight); free(outc); free(adjStart); free(deg);
        free(adjList); free(cur); voronoi_free(&vd);
        fprintf(stderr, "warning: wfc skipped (out of memory)\n");
        return;
    }
    for (int i = 0; i < ns; ++i) cur[i] = adjStart[i];
    for (int b = 0; b < gy; ++b)
        for (int a = 0; a < gx; ++a) {
            int id = b * gx + a;
            if (a > 0)      adjList[cur[id]++] = id - 1;
            if (a < gx - 1) adjList[cur[id]++] = id + 1;
            if (b > 0)      adjList[cur[id]++] = id - gx;
            if (b < gy - 1) adjList[cur[id]++] = id + gx;
        }

    int rc = wfc_solve_graph(ns, adjStart, adjList, WC_N, allowed, weight,
                             NULL, cfg->seed, outc);

    long reclassified = 0;
    if (rc == 0) {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                size_t i = (size_t)x + (size_t)y * W;
                if (!is_climate_cat(g->cat[i])) continue;
                int cls = outc[g->region[i]];
                int nc = WC_TCAT[cls];
                if (nc != g->cat[i]) {
                    g->cat[i] = (uint8_t)nc;
                    g->id[i]  = biome_tile(nc, cell_hash(cfg->seed, x, y, NOISE_LAYER_BIOME));
                    ++reclassified;
                }
            }
        fprintf(stderr, "wfc: %d territories solved, %ld cells retinted (legal transitions)\n",
                ns, reclassified);
    } else {
        fprintf(stderr, "wfc: contradiction -> kept climate biomes\n");
    }

    free(weight); free(outc); free(adjStart); free(deg);
    free(adjList); free(cur); voronoi_free(&vd);
}

/*
 * Cellular-automata pass: grow organic forest clumps. A random fill over the
 * grass/forest area (plus the existing forest as seeds) is smoothed with a
 * birth/survival automaton, clustering scattered forest into coherent woods and
 * clearing lone trees. Only grass<->forest cells are toggled.
 */
static void cellular_pass(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const size_t n = (size_t)W * (size_t)H;
    uint8_t *domain = (uint8_t *)malloc(n);
    uint8_t *mask   = (uint8_t *)malloc(n);
    if (!domain || !mask) {
        free(domain); free(mask);
        fprintf(stderr, "warning: cellular skipped (out of memory)\n");
        return;
    }
    for (size_t i = 0; i < n; ++i)
        domain[i] = (g->cat[i] == TCAT_GRASS || g->cat[i] == TCAT_FOREST) ? 1 : 0;

    cellular_fill(mask, domain, W, H, cfg->cellular_fill, cfg->seed, NOISE_LAYER_CELLULAR);
    for (size_t i = 0; i < n; ++i)
        if (domain[i] && g->cat[i] == TCAT_FOREST) mask[i] = 1;   /* seed existing woods */

    cellular_step(mask, domain, W, H, 5, 4, 0, cfg->cellular_iterations);

    long toForest = 0, toGrass = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)x + (size_t)y * W;
            if (!domain[i]) continue;
            int nc = mask[i] ? TCAT_FOREST : TCAT_GRASS;
            if (nc != g->cat[i]) {
                g->cat[i] = (uint8_t)nc;
                g->id[i]  = biome_tile(nc, cell_hash(cfg->seed, x, y, NOISE_LAYER_BIOME));
                if (nc == TCAT_FOREST) ++toForest; else ++toGrass;
            }
        }
    fprintf(stderr, "cellular: forest clumps (+%ld forest, -%ld forest cells)\n",
            toForest, toGrass);
    free(domain); free(mask);
}

/* --- Phase 5: towns, roads, bridges, trails, buildings -------------------- */

/* Civilization tiles (validated classic land tile ids reused as crude town
 * terrain; real statics walls are a follow-up on the statics pipeline). */
#define TILE_TOWN_ROAD   0x0071  /* dirt road / trail */
#define TILE_TOWN_BRIDGE 0x0016  /* river crossing (packed earth) */
#define TILE_TOWN_FLOOR  0x0016  /* building floor */
#define TILE_TOWN_WALL   0x00E4  /* building wall (rock) */

#define TOWN_MAX 1024

/* Walkable land a road or building may occupy (not open sea or mountain, and
 * rivers/lakes only as bridges). */
static int town_buildable(int c) {
    return !(c == TCAT_WATER_DEEP || c == TCAT_WATER_SHALLOW ||
             c == TCAT_RIVER || c == TCAT_LAKE || c == TCAT_MOUNTAIN);
}

static void paint_road_cell(terrain_grid *g, int i) {
    int c = g->cat[i];
    if (c == TCAT_WATER_DEEP || c == TCAT_WATER_SHALLOW || c == TCAT_MOUNTAIN)
        return;                               /* A* should never step here */
    if (c == TCAT_RIVER || c == TCAT_LAKE) {
        g->cat[i] = (uint8_t)TCAT_BRIDGE;
        g->id[i]  = TILE_TOWN_BRIDGE;
    } else if (c != TCAT_FLOOR && c != TCAT_WALL) {
        g->cat[i] = (uint8_t)TCAT_ROAD;
        g->id[i]  = TILE_TOWN_ROAD;
    }
}

/*
 * Towns pass (runs last, after slope-limit): Poisson-disc town sites on
 * buildable land, an MST over them, A* roads (bridging rivers) along the tree,
 * BSP building footprints + street grid at each town, and L-system side-trails.
 * Deterministic (Poisson/BSP/L-system each keyed by their salts). Terrain-level
 * (cat/id) only; byte-safe behind the `towns` toggle.
 */
static void towns_pass(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const size_t n = (size_t)W * (size_t)H;
    const int margin = 8;

    /* Eligibility: buildable, not too high (avoid clifftops), off the edge. */
    uint8_t *allow = (uint8_t *)malloc(n);
    float   *cost  = (float *)malloc(n * sizeof(float));
    int     *path  = (int *)malloc(n * sizeof(int));
    int     *tx = (int *)malloc(TOWN_MAX * sizeof(int));
    int     *ty = (int *)malloc(TOWN_MAX * sizeof(int));
    if (!allow || !cost || !path || !tx || !ty) {
        free(allow); free(cost); free(path); free(tx); free(ty);
        fprintf(stderr, "warning: towns skipped (out of memory)\n");
        return;
    }

    const int zhi = (cfg->land_z_max > 0) ? (cfg->land_z_max * 3) / 5 : 127;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)x + (size_t)y * W;
            int c = g->cat[i];
            int ok = town_buildable(c) && c != TCAT_RIVER && c != TCAT_LAKE &&
                     g->z[i] <= zhi &&
                     x >= margin && y >= margin && x < W - margin && y < H - margin;
            allow[i] = (uint8_t)(ok ? 1 : 0);

            /* A* cost: impassable sea/mountain; bridge rivers/lakes; prefer flat
             * open ground, penalise forest/swamp/hills and elevation. */
            if (c == TCAT_WATER_DEEP || c == TCAT_WATER_SHALLOW || c == TCAT_MOUNTAIN)
                cost[i] = -1.0f;
            else if (c == TCAT_RIVER || c == TCAT_LAKE)
                cost[i] = 40.0f;                        /* bridge premium */
            else {
                float cc = 1.0f + 0.06f * (float)g->z[i];
                if (c == TCAT_HILL)   cc += 3.0f;
                if (c == TCAT_FOREST || c == TCAT_JUNGLE) cc += 1.5f;
                if (c == TCAT_SWAMP)  cc += 4.0f;
                cost[i] = cc;
            }
        }

    int spacing = cfg->town_spacing > 8 ? cfg->town_spacing : 8;
    int nt = poisson_sample(W, H, (double)spacing, allow, 30,
                            cfg->seed, NOISE_LAYER_POISSON, tx, ty, TOWN_MAX);

    /* MST over the towns (complete Euclidean graph -> tree). */
    long roads = 0, bridges = 0;
    if (nt >= 2) {
        int cap = nt * (nt - 1) / 2;
        mst_edge *edges = (mst_edge *)malloc((size_t)cap * sizeof(mst_edge));
        mst_edge *tree  = (mst_edge *)malloc((size_t)nt * sizeof(mst_edge));
        if (edges && tree) {
            int ne = 0;
            for (int a = 0; a < nt; ++a)
                for (int b = a + 1; b < nt; ++b) {
                    double dx = tx[a] - tx[b], dy = ty[a] - ty[b];
                    edges[ne].a = a; edges[ne].b = b;
                    edges[ne].w = sqrt(dx * dx + dy * dy);
                    ++ne;
                }
            int k = mst_build(edges, ne, nt, tree);
            for (int e = 0; e < k; ++e) {
                int a = tree[e].a, b = tree[e].b;
                int start = tx[a] + ty[a] * W, goal = tx[b] + ty[b] * W;
                int len = astar_path(W, H, cost, start, goal, path, (int)n);
                if (len <= 0) continue;
                for (int p = 0; p < len; ++p) {
                    int ci = path[p];
                    if (g->cat[ci] == TCAT_RIVER || g->cat[ci] == TCAT_LAKE) ++bridges;
                    else ++roads;
                    paint_road_cell(g, ci);
                    cost[ci] = 0.3f;              /* reuse roads in later edges */
                }
            }
        }
        free(edges); free(tree);
    }

    /* Town layout: plaza + BSP buildings + radiating L-system trails. */
    const int tsize = cfg->town_size > 12 ? cfg->town_size : 12;
    bsp_rect leaves[256];
    long buildings = 0, trailCells = 0;
    int *trail = path;                              /* reuse the path buffer */
    for (int t = 0; t < nt; ++t) {
        int cx = tx[t], cy = ty[t];
        int rx = cx - tsize / 2, ry = cy - tsize / 2, rw = tsize, rh = tsize;
        if (rx < margin) rx = margin;
        if (ry < margin) ry = margin;
        if (rx + rw > W - margin) rw = W - margin - rx;
        if (ry + rh > H - margin) rh = H - margin - ry;
        if (rw < 12 || rh < 12) continue;

        /* Plaza: pave buildable cells in the town rect. */
        for (int y = ry; y < ry + rh; ++y)
            for (int x = rx; x < rx + rw; ++x) {
                size_t i = (size_t)x + (size_t)y * W;
                if (town_buildable(g->cat[i])) paint_road_cell(g, i);
            }

        /* Buildings: BSP footprints, inset by 1 so streets remain between them. */
        int nl = bsp_partition(rx, ry, rw, rh, 8, 5,
                               cfg->seed, NOISE_LAYER_BSP + (uint32_t)t,
                               leaves, 256);
        for (int l = 0; l < nl; ++l) {
            int bx = leaves[l].x + 1, by = leaves[l].y + 1;
            int bw = leaves[l].w - 2, bh = leaves[l].h - 2;
            if (bw < 3 || bh < 3) continue;
            int anyFloor = 0;
            for (int y = by; y < by + bh; ++y)
                for (int x = bx; x < bx + bw; ++x) {
                    size_t i = (size_t)x + (size_t)y * W;
                    int c = g->cat[i];
                    if (!(c == TCAT_ROAD || c == TCAT_FLOOR || c == TCAT_WALL))
                        continue;                 /* build only on the paved plaza */
                    int border = (x == bx || x == bx + bw - 1 ||
                                  y == by || y == by + bh - 1);
                    g->cat[i] = (uint8_t)(border ? TCAT_WALL : TCAT_FLOOR);
                    g->id[i]  = border ? TILE_TOWN_WALL : TILE_TOWN_FLOOR;
                    if (!border) anyFloor = 1;
                }
            /* Punch a door in the south wall. */
            int doorx = bx + bw / 2, doory = by + bh - 1;
            if (anyFloor && doorx >= 0 && doory >= 0 && doorx < W && doory < H) {
                size_t di = (size_t)doorx + (size_t)doory * W;
                g->cat[di] = (uint8_t)TCAT_FLOOR;
                g->id[di]  = TILE_TOWN_FLOOR;
            }
            ++buildings;
        }

        /* L-system trails radiating from the town centre. */
        if (cfg->trails) {
            for (int b = 0; b < 3; ++b) {
                double ang = (6.2831853 / 3.0) * b
                           + (double)((cfg->seed >> (b * 4)) & 7) * 0.1;
                int m = lsystem_trail(W, H, cx, cy, ang, (double)spacing * 0.25,
                                      0.5, 3, cfg->seed,
                                      NOISE_LAYER_LSYSTEM + (uint32_t)t,
                                      trail, (int)n);
                for (int p = 0; p < m; ++p) {
                    int ci = trail[p];
                    if (town_buildable(g->cat[ci]) &&
                        g->cat[ci] != TCAT_FLOOR && g->cat[ci] != TCAT_WALL) {
                        paint_road_cell(g, ci);
                        ++trailCells;
                    }
                }
            }
        }
    }

    fprintf(stderr,
        "towns: %d towns, %ld road + %ld bridge cells, %ld buildings, %ld trail cells\n",
        nt, roads, bridges, buildings, trailCells);

    free(allow); free(cost); free(path); free(tx); free(ty);
}

int terrain_generate(terrain_grid *g, const mapgen_config *cfg,
                     const tiledata_land *td, struct preview_ctx *pv) {
    const int W = cfg->width, H = cfg->height;
    const size_t n = (size_t)W * (size_t)H;

    g->width = W;
    g->height = H;
    g->id  = (uint16_t *)malloc(n * sizeof(uint16_t));
    g->z   = (int8_t  *)malloc(n * sizeof(int8_t));
    g->cat = (uint8_t *)malloc(n * sizeof(uint8_t));
    /* The height field now lives on the grid so later passes can reuse it; it
     * is released by terrain_free(). Reserved layers stay NULL until used. */
    g->hfield      = (float *)malloc(n * sizeof(float));
    g->moisture    = NULL;
    g->temperature = NULL;
    g->region      = NULL;
    g->flags       = NULL;
    float *hf = g->hfield;
    if (!g->id || !g->z || !g->cat || !g->hfield) {
        terrain_free(g); return -1;
    }

    /* Continent centers + base radius first, so the continent-shape elevation
     * noise can be scaled to the continent size (coherent landmasses rather
     * than circles or archipelagos). */
    int ncen = cfg->continent_count;
    vec2 centers[64];
    double R = 1.0;
    if (cfg->continents) {
        if (ncen > 64) ncen = 64;
        place_centers(cfg, centers, ncen);
        double minpair = (double)(W < H ? W : H);
        for (int a = 0; a < ncen; ++a)
            for (int b = a + 1; b < ncen; ++b) {
                double dx = centers[a].x - centers[b].x;
                double dy = centers[a].y - centers[b].y;
                double d = sqrt(dx * dx + dy * dy);
                if (d < minpair) minpair = d;
            }
        R = 0.40 * minpair;
    }

    noise_layer *elev = noise_layer_create(cfg->seed, NOISE_LAYER_ELEVATION,
                                            cfg->frequency, cfg->octaves);
    noise_layer *moist = noise_layer_create(cfg->seed, NOISE_LAYER_MOISTURE,
                                             cfg->frequency * 2.0,
                                             cfg->octaves > 2 ? cfg->octaves - 1 : cfg->octaves);
    noise_layer *cont = NULL, *mtn = NULL, *temp = NULL;
    if (cfg->biomes)
        temp = noise_layer_create(cfg->seed, NOISE_LAYER_TEMPERATURE,
                                  cfg->frequency * 0.6, 3);
    if (cfg->continents)
        /* Fractal shape noise scaled to the continent radius: ~1.3 wavelengths
         * across R gives one coherent landmass per center with organic,
         * fractal coasts (not a circle, not an archipelago). */
        cont = noise_layer_create(cfg->seed, NOISE_LAYER_CONTINENT,
                                  1.0 / R, cfg->octaves);
    if (cfg->mountains) {
        /* Lower frequency + fewer octaves => a few broad, distinct ranges
         * rather than a fine web of ridges across the whole interior.
         * --mountain-scale overrides the frequency (smaller = bigger ranges). */
        double mfreq = cfg->mountain_scale > 0.0 ? cfg->mountain_scale
                                                 : cfg->frequency * 0.5;
        mtn = noise_layer_create_ridged(cfg->seed, NOISE_LAYER_DETAIL, mfreq,
                                        cfg->octaves > 4 ? 4 : cfg->octaves);
    }
    if (!elev || !moist || (cfg->continents && !cont) || (cfg->mountains && !mtn)
        || (cfg->biomes && !temp)) {
        noise_layer_free(elev); noise_layer_free(moist);
        noise_layer_free(cont); noise_layer_free(mtn); noise_layer_free(temp);
        terrain_free(g); return -1;   /* terrain_free releases g->hfield */
    }

    validate_palette(td);

    const double sea = cfg->sea_level;
    const int land_zmax = cfg->land_z_max;
    const double inv_halfw = (W > 1) ? 2.0 / (double)(W - 1) : 0.0;
    const double inv_halfh = (H > 1) ? 2.0 / (double)(H - 1) : 0.0;

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)x + (size_t)y * (size_t)W;
            double e_detail = noise_layer_sample(elev, x, y);  /* ~[-1,1] */
            double e;       /* landness: >sea => land */
            double hbase;   /* [0,1] macro land height (interior high, coast low) */

            if (cfg->continents) {
                /* Land = elevation NOISE minus a radial falloff around the
                 * nearest placed center (same idea as --continent, per center).
                 * The fractal shape noise defines the coastline; the falloff
                 * only pushes faraway areas to ocean and keeps continents
                 * separated. A little fine detail roughens the edge further. */
                double shape = noise_layer_sample(cont, x, y);     /* [-1,1] */
                double dmin = 1e30;
                for (int k = 0; k < ncen; ++k) {
                    double dx = (double)x - centers[k].x;
                    double dy = (double)y - centers[k].y;
                    double dist = sqrt(dx * dx + dy * dy);
                    if (dist < dmin) dmin = dist;
                }
                double t = dmin / R - 0.40;         /* solid-ish core out to 0.40R */
                if (t < 0.0) t = 0.0;
                /* Gentle interior lift so each continent reads as one coherent
                 * landmass (fewer interior seas), while the noise still shapes
                 * the coastline organically. */
                double core = 0.25 * (1.0 - dmin / R);
                if (core < 0.0) core = 0.0;
                e = (shape + 0.18 * e_detail + core) - 1.7 * pow(t, 2.0);
                /* Ocean margin: force the outer ring of the map to sea so no
                 * continent runs off the edge and ends abruptly. */
                double nxe = (double)x * inv_halfw - 1.0;
                double nye = (double)y * inv_halfh - 1.0;
                double em = fabs(nxe) > fabs(nye) ? fabs(nxe) : fabs(nye);
                double et = (em - 0.80) / (1.0 - 0.80);
                if (et < 0.0) et = 0.0;
                e -= 3.0 * et * et;
                /* Smooth macro height (cone) for land height + river routing. */
                double hc = 1.0 - dmin / R;
                if (hc < 0.0) hc = 0.0;
                if (hc > 1.0) hc = 1.0;
                hbase = hc;
            } else if (cfg->continent) {
                e = e_detail;
                double nx = (double)x * inv_halfw - 1.0;
                double ny = (double)y * inv_halfh - 1.0;
                double d = sqrt(nx * nx + ny * ny);
                if (d > 1.0) d = 1.0;
                double r0 = cfg->continent_radius;
                double t = (d - r0) / (1.0 - r0);
                if (t < 0.0) t = 0.0;
                e -= cfg->continent_strength * pow(t, cfg->continent_power);
                hbase = (e - sea);
            } else {
                e = e_detail;
                hbase = (e - sea);
            }

            int cat; int z; double height;

            if (e < sea) {
                cat = (e < sea - 0.15) ? TCAT_WATER_DEEP : TCAT_WATER_SHALLOW;
                z = cfg->water_z;
                height = -1.0e9f;            /* sea = strong sink for river routing */
            } else {
                double h = hbase;             /* roughly [0,1] inland */
                if (h < 0.0) h = 0.0;
                if (h > 1.0) h = 1.0;
                /* z carries fine detail for micro-relief... */
                double hh = 0.65 * h + 0.35 * (e_detail * 0.5 + 0.5);
                if (hh < 0.0) hh = 0.0;
                if (hh > 1.0) hh = 1.0;
                z = (int)lround(hh * (double)land_zmax);

                /* ...but the river ROUTING height is the SMOOTH macro slope
                 * (monotonic toward the coast) so downhill tracing is not
                 * trapped by detail-noise pits. */
                double route = h;

                /* Mountains: ridged ridges on sufficiently inland/high ground. */
                int isMountain = 0;
                if (cfg->mountains && h > 0.30) {
                    double m = noise_layer_sample(mtn, x, y);  /* ridged, ~[0,1] peaks high */
                    double mn = (m + 1.0) * 0.5;               /* -> [0,1] */
                    if (mn > cfg->mountain_level) {
                        double f = (mn - cfg->mountain_level) / (1.0 - cfg->mountain_level);
                        /* Mountains rise above the ground in both modes: from
                         * flat_z in flat mode, or from the base height otherwise. */
                        int base = cfg->flat ? cfg->flat_z : z;
                        z = clampi(base + (int)lround(f * (double)cfg->mountain_z), -128, 127);
                        route += f;           /* ranges are river sources/high ground */
                        isMountain = 1;
                    }
                }

                /* Flat mode: non-mountain land sits at one level, while mountain
                 * ranges keep their elevation (set above). */
                if (cfg->flat && !isMountain)
                    z = cfg->flat_z;

                height = (float)route;

                double mo = noise_layer_sample(moist, x, y);
                if (isMountain) {
                    cat = TCAT_MOUNTAIN;
                } else if (hh > 0.72) {
                    cat = TCAT_HILL;
                } else if (cfg->biomes) {
                    /* Climate: temperature by latitude (poles cold, centre hot),
                     * modulated by noise and cooled with elevation. */
                    double ny2 = (H > 1) ? (double)y / (double)(H - 1) : 0.5;
                    double lat = 1.0 - 2.0 * fabs(ny2 - 0.5);       /* 0 poles .. 1 centre */
                    double temperature = (lat * 2.0 - 1.0) * 0.65
                                       + noise_layer_sample(temp, x, y) * 0.25
                                       - hh * 0.5 + cfg->temperature_bias;
                    cat = biome_classify(hh, temperature, mo);
                } else {
                    cat = (mo > 0.1) ? TCAT_FOREST : TCAT_GRASS;
                }
            }

            uint64_t hash = cell_hash(cfg->seed, x, y, NOISE_LAYER_BIOME);
            g->cat[i] = (uint8_t)cat;
            g->id[i]  = biome_tile(cat, hash);
            g->z[i]   = (int8_t)clampi(z, -128, 127);
            hf[i]     = height;
        }
    }

    preview_pass(pv, g, "base_terrain_noise-landmass-biomes");

    /* Hydraulic erosion carves valleys/drainage into the height field (and
     * relief) before rivers, so rivers follow the eroded drainage. hf aliases
     * g->hfield, which erosion updates in place. */
    if (cfg->erosion) {
        erosion_apply(g, cfg);
        preview_pass(pv, g, "phase2_erosion_carved-valleys");
    }

    /* Voronoi territories: organic climate biomes + territory graph. */
    if (cfg->regions) {
        regions_pass(g, cfg);
        preview_pass(pv, g, "phase3_regions_voronoi-biomes");
    }

    /* WFC biome transitions over the territory graph. */
    if (cfg->wfc) {
        wfc_pass(g, cfg);
        preview_pass(pv, g, "phase4_wfc_legal-biome-transitions");
    }

    /* Cellular-automata organic forest clumps. */
    if (cfg->cellular) {
        cellular_pass(g, cfg);
        preview_pass(pv, g, "phase4_cellular_forest-clumps");
    }

    if (cfg->rivers) {
        carve_rivers(g, cfg, hf);
        preview_pass(pv, g, "base_rivers_meander-fords-lakes");
    }

    if (cfg->beaches) {
        beach_pass(g, cfg);
        preview_pass(pv, g, "base_beaches_sloped-coast");
    }

    if (cfg->passes && cfg->mountains) {
        carve_passes(g, cfg);
        preview_pass(pv, g, "base_mountain-passes");
    }

    /* Flat mode is already level; slope-limiting would only pull coastal land
     * down toward the ocean, so skip it. */
    if (!cfg->flat) {
        limit_slope(g, cfg);
        preview_pass(pv, g, "base_slope-limit");
    }

    /* Civilization: towns, roads/bridges, buildings, trails (runs last so the
     * slope-limiter doesn't flatten roads). */
    if (cfg->towns) {
        towns_pass(g, cfg);
        preview_pass(pv, g, "phase5_towns_roads-bridges-buildings");
    }

    noise_layer_free(elev); noise_layer_free(moist);
    noise_layer_free(cont); noise_layer_free(mtn); noise_layer_free(temp);
    /* g->hfield is kept for later passes and freed by terrain_free(). */
    return 0;
}

void terrain_free(terrain_grid *g) {
    if (!g)
        return;
    free(g->id);          g->id = NULL;
    free(g->z);           g->z = NULL;
    free(g->cat);         g->cat = NULL;
    free(g->hfield);      g->hfield = NULL;
    free(g->moisture);    g->moisture = NULL;
    free(g->temperature); g->temperature = NULL;
    free(g->region);      g->region = NULL;
    free(g->flags);       g->flags = NULL;
    g->width = g->height = 0;
}
