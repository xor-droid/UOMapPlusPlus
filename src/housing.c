#include "uomappp/housing.h"
#include "uomappp/terrain.h"
#include "uomappp/vegetation.h"

#include <stdio.h>
#include <stdlib.h>

/* Water categories (ocean/shallows/river/lake). Mountain is impassable rock.
 * Everything else is land a house could sit on if it is flat and clear. */
static int cat_is_water(int c) {
    return c == TCAT_WATER_DEEP || c == TCAT_WATER_SHALLOW ||
           c == TCAT_RIVER || c == TCAT_LAKE;
}

/* Standard UO house footprints (square), smallest to largest. */
static const struct { const char *name; int n; } HOUSE_SIZES[] = {
    { "small",  7 }, { "medium", 12 }, { "large",  18 },
    { "keep",  24 }, { "castle", 31 },
};

void housing_summary_print(const terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const size_t NC = (size_t)W * (size_t)H;

    /* buildable[i] = dry, passable land with no blocking static on the cell. */
    uint8_t *build = (uint8_t *)malloc(NC);
    if (!build) {
        fprintf(stderr, "housing: out of memory, skipping summary\n");
        return;
    }

    long water = 0, rock = 0, blocked = 0, buildable = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)x + (size_t)y * W;
            int c = g->cat[i];
            if (cat_is_water(c)) { build[i] = 0; ++water; continue; }
            if (c == TCAT_MOUNTAIN) { build[i] = 0; ++rock; continue; }
            int cleared = g->flags && (g->flags[i] & TGRID_FLAG_CLEARED);
            int blk = !cleared && vegetation_blocks(cfg, c, x, y);
            if (blk) { build[i] = 0; ++blocked; }
            else     { build[i] = 1; ++buildable; }
        }

    /* World statics (town walls/doors, cliff rocks) also block their cell. */
    for (int s = 0; s < g->statics_n; ++s) {
        const grid_static *gs = &g->statics[s];
        if (gs->x < 0 || gs->x >= W || gs->y < 0 || gs->y >= H) continue;
        size_t i = (size_t)gs->x + (size_t)gs->y * W;
        if (build[i]) { build[i] = 0; ++blocked; --buildable; }
    }

    printf("\nhousing summary (buildable = dry non-ocean land, no rock, clear of "
           "trees/rocks/statics):\n");
    printf("  map cells            : %12ld\n", (long)NC);
    printf("  water / ocean        : %12ld\n", water);
    printf("  impassable mountain  : %12ld\n", rock);
    printf("  blocked by statics   : %12ld\n", blocked);
    printf("  buildable land       : %12ld  (%.1f%% of map)\n",
           buildable, 100.0 * (double)buildable / (double)NC);

    printf("  house spots (flat, clear, non-overlapping footprints):\n");
    const int NS = (int)(sizeof(HOUSE_SIZES) / sizeof(HOUSE_SIZES[0]));
    for (int k = 0; k < NS; ++k) {
        int N = HOUSE_SIZES[k].n;
        long fit = 0;
        for (int by = 0; by + N <= H; by += N)
            for (int bx = 0; bx + N <= W; bx += N) {
                int ok = 1, zmin = 127, zmax = -128;
                for (int dy = 0; dy < N && ok; ++dy)
                    for (int dx = 0; dx < N; ++dx) {
                        size_t i = (size_t)(bx + dx) + (size_t)(by + dy) * W;
                        if (!build[i]) { ok = 0; break; }
                        int zz = g->z[i];
                        if (zz < zmin) zmin = zz;
                        if (zz > zmax) zmax = zz;
                        if (zmax - zmin > 0) { ok = 0; break; } /* strict flat */
                    }
                if (ok) ++fit;
            }
        printf("    %-7s %2dx%-2d : %10ld\n", HOUSE_SIZES[k].name, N, N, fit);
    }

    free(build);
}
