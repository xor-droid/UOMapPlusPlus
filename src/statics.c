#include "uomappp/statics.h"
#include "uomappp/vegetation.h"
#include "uomappp/io.h"

#include <stdio.h>
#include <stdlib.h>

/* Deterministic ordering within a block: by y, then x, then z, then id. */
static int rec_less(const static_rec *a, const static_rec *b) {
    if (a->y != b->y) return a->y < b->y;
    if (a->x != b->x) return a->x < b->x;
    if (a->z != b->z) return a->z < b->z;
    return a->id < b->id;
}

/* A grid static bucketed by block index, for merging with vegetation. */
typedef struct { int bi; static_rec rec; } blk_static;

static int blk_cmp(const void *A, const void *B) {
    const blk_static *a = (const blk_static *)A, *b = (const blk_static *)B;
    if (a->bi != b->bi) return a->bi < b->bi ? -1 : 1;
    if (rec_less(&a->rec, &b->rec)) return -1;
    if (rec_less(&b->rec, &a->rec)) return 1;
    return 0;
}

#define STATICS_BLOCK_MAX 512

static int write_rec(FILE *f, const static_rec *r) {
    return (io_write_u16le(f, r->id) != 0 ||
            io_write_u8(f, r->x) != 0 ||
            io_write_u8(f, r->y) != 0 ||
            io_write_i8(f, r->z) != 0 ||
            io_write_i16le(f, (uint16_t)r->hue) != 0) ? -1 : 0;
}

int statics_write(const terrain_grid *g, const mapgen_config *cfg,
                  const char *out_dir, int map_index) {
    const int W = g->width, H = g->height;
    const int BW = W >> 3, BH = H >> 3;

    char name[32], path[UOMG_PATH_MAX];
    snprintf(name, sizeof(name), "staidx%d.mul", map_index);
    if (io_join_path(path, sizeof(path), out_dir, name) != 0) return -1;
    FILE *fi = fopen(path, "wb");
    if (!fi) { fprintf(stderr, "error: cannot open %s\n", path); return -1; }

    snprintf(name, sizeof(name), "statics%d.mul", map_index);
    if (io_join_path(path, sizeof(path), out_dir, name) != 0) { fclose(fi); return -1; }
    FILE *fs = fopen(path, "wb");
    if (!fs) { fprintf(stderr, "error: cannot open %s\n", path); fclose(fi); return -1; }

    /* Vegetation (<=64*VEG_MAX_PER_CELL) plus any merged grid statics per block. */
    static_rec buf[STATICS_BLOCK_MAX];
    int32_t offset = 0;   /* running byte offset into statics file */
    int rc = 0;
    long total = 0;

    /* Pre-bucket the grid's extra statics (building walls, cliff rocks) by block
     * index so we can merge-walk them alongside the column-major block loop. */
    blk_static *extra = NULL;
    long en = 0, ei = 0;
    if (g->statics_n > 0) {
        extra = (blk_static *)malloc((size_t)g->statics_n * sizeof(blk_static));
        if (!extra) { fclose(fi); fclose(fs); return -1; }
        for (int s = 0; s < g->statics_n; ++s) {
            const grid_static *gs = &g->statics[s];
            if (gs->x < 0 || gs->y < 0 || gs->x >= W || gs->y >= H) continue;
            extra[en].bi = (gs->x >> 3) * BH + (gs->y >> 3);
            extra[en].rec.id  = gs->id;
            extra[en].rec.x   = (uint8_t)(gs->x & 7);
            extra[en].rec.y   = (uint8_t)(gs->y & 7);
            extra[en].rec.z   = gs->z;
            extra[en].rec.hue = gs->hue;
            ++en;
        }
        qsort(extra, (size_t)en, sizeof(blk_static), blk_cmp);
    }

    /* Column-major block order: blockIndex = bx*BH + by (matches map/staidx). */
    for (int bx = 0; bx < BW && rc == 0; ++bx) {
        for (int by = 0; by < BH && rc == 0; ++by) {
            int nb = 0;
            for (int cy = 0; cy < 8; ++cy)
                for (int cx = 0; cx < 8; ++cx) {
                    int x = (bx << 3) + cx, y = (by << 3) + cy;
                    size_t idx = (size_t)x + (size_t)y * (size_t)W;
                    /* Reserved clearings get no vegetation (buildable ground). */
                    int cleared = g->flags && (g->flags[idx] & TGRID_FLAG_CLEARED);
                    int k = cleared ? 0
                          : vegetation_place(cfg, g->cat[idx], cx, cy, g->z[idx],
                                             x, y, &buf[nb]);
                    nb += k;
                }

            /* Merge extra statics for this block. */
            int bi = bx * BH + by;
            while (ei < en && extra[ei].bi == bi) {
                if (nb < STATICS_BLOCK_MAX) buf[nb++] = extra[ei].rec;
                ++ei;
            }

            if (nb == 0) {
                /* empty block */
                if (io_write_i32le(fi, -1) || io_write_i32le(fi, -1) || io_write_i32le(fi, -1))
                    rc = -1;
                continue;
            }

            /* deterministic insertion sort (nb is small) */
            for (int a = 1; a < nb; ++a) {
                static_rec key = buf[a]; int b = a - 1;
                while (b >= 0 && rec_less(&key, &buf[b])) { buf[b + 1] = buf[b]; --b; }
                buf[b + 1] = key;
            }

            for (int r = 0; r < nb; ++r)
                if (write_rec(fs, &buf[r]) != 0) { rc = -1; break; }

            int32_t len = (int32_t)(nb * 7);
            if (rc == 0 &&
                (io_write_i32le(fi, offset) || io_write_i32le(fi, len) || io_write_i32le(fi, 0)))
                rc = -1;
            offset += len;
            total += nb;
        }
    }

    if (fclose(fs) != 0) rc = -1;
    if (fclose(fi) != 0) rc = -1;
    free(extra);
    if (rc != 0) fprintf(stderr, "error: failed writing statics for map %d\n", map_index);
    else fprintf(stderr, "statics: %ld records, %d bytes\n", total, offset);
    return rc;
}
