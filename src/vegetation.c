#include "uomappp/vegetation.h"
#include "uomappp/terrain.h"
#include "uomappp/noise.h"

/* Curated static sets (verified against tiledata.mul). Trees/rocks/boulders are
 * Impassable (they block movement); plants/flowers/ferns/grasses/reeds are
 * passable ground cover. */
static const uint16_t TREES_TEMPERATE[] = { 0x0CCA, 0x0CCB, 0x0CCC, 0x0CCD, 0x0CD0, 0x0CD3, 0x0D42, 0x0D43 };
static const uint16_t TREES_JUNGLE[]    = { 0x0C95, 0x0CA8, 0x0CAA, 0x0CAB };
static const uint16_t CACTI[]           = { 0x0D25, 0x0D26, 0x0D27, 0x0D28 };
static const uint16_t ROCKS[]           = { 0x1363, 0x1364, 0x1365, 0x1366 };
static const uint16_t BOULDERS[]        = { 0x11B6, 0x1350, 0x1351, 0x1352 };
static const uint16_t FERNS[]           = { 0x0C9F, 0x0CA0, 0x0CA1, 0x0CA2 };
static const uint16_t FLOWERS[]         = { 0x0C37, 0x0C38, 0x0C45, 0x0C46 };
static const uint16_t GRASSES[]         = { 0x0CAC, 0x0CAD, 0x0CAE, 0x0CAF };
static const uint16_t REEDS[]           = { 0x0D05 };

#define PICK(arr, h) ((arr)[(h) % (sizeof(arr) / sizeof((arr)[0]))])

/* Shared source of truth for both placement and the housing-buildability query:
 * pick the per-cell blocking feature (tree/rock/cactus/boulder -> *out_id) and
 * passable ground cover (flower/fern/grass/reed -> *out_cover). The RNG draws
 * happen in a fixed order so output stays byte-identical. */
static void veg_pick(const mapgen_config *cfg, int cat, int gx, int gy,
                     uint16_t *out_id, uint16_t *out_cover) {
    *out_id = 0;
    *out_cover = 0;
    if (!cfg->vegetation)
        return;

    /* Deterministic per-cell RNG stream. */
    uint64_t s = cfg->seed
               ^ (0x2545F4914F6CDD1DULL * (uint64_t)(uint32_t)gx)
               ^ (0x9E3779B97F4A7C15ULL * (uint64_t)(uint32_t)gy)
               ^ ((uint64_t)NOISE_LAYER_VEG << 56);
    uint64_t r1 = noise_splitmix64(&s);
    uint64_t r2 = noise_splitmix64(&s);
    double u1 = (double)(r1 >> 11) / 9007199254740992.0;  /* [0,1) */
    double u2 = (double)(r2 >> 11) / 9007199254740992.0;

    const double td = cfg->tree_density;
    const double rd = cfg->rock_density;
    const double pd = cfg->plant_density;

    uint16_t id = 0;        /* primary feature (blocking) */
    uint16_t cover = 0;     /* optional ground cover (passable) */

    switch (cat) {
        case TCAT_FOREST:
            if (u1 < td)        id = PICK(TREES_TEMPERATE, r1);
            if (u2 < pd * 0.6)  cover = PICK(FERNS, r2);
            break;
        case TCAT_JUNGLE:
            if (u1 < td * 1.3)  id = PICK(TREES_JUNGLE, r1);
            if (u2 < pd)        cover = PICK(FERNS, r2);
            break;
        case TCAT_SNOW:
            if (u1 < td * 0.5)  id = PICK(TREES_TEMPERATE, r1);  /* sparse */
            break;
        case TCAT_DESERT:
            if (u1 < td * 0.35) id = PICK(CACTI, r1);
            break;
        case TCAT_SWAMP:
            if (u1 < pd * 1.5)  cover = PICK(REEDS, r1);
            if (u2 < td * 0.25) id = PICK(TREES_TEMPERATE, r2);  /* occasional dead-ish tree */
            break;
        case TCAT_GRASS:
            if (u1 < td * 0.12) id = PICK(TREES_TEMPERATE, r1);  /* lone trees */
            if (u2 < pd)        cover = (r2 & 1) ? PICK(FLOWERS, r2) : PICK(GRASSES, r2);
            break;
        case TCAT_HILL:
            if (u1 < rd * 1.5)  id = PICK(ROCKS, r1);
            break;
        case TCAT_MOUNTAIN:
            if (u1 < rd * 2.0)  id = PICK(BOULDERS, r1);
            break;
        default:
            return;  /* water, river, lake, beach -> no vegetation */
    }
    *out_id = id;
    *out_cover = cover;
}

int vegetation_place(const mapgen_config *cfg, int cat, int cx, int cy,
                     int z, int gx, int gy, static_rec *out) {
    uint16_t id, cover;
    veg_pick(cfg, cat, gx, gy, &id, &cover);

    int n = 0;
    if (cover) {
        out[n].id = cover; out[n].x = (uint8_t)cx; out[n].y = (uint8_t)cy;
        out[n].z = (int8_t)z; out[n].hue = 0; ++n;
    }
    if (id) {
        out[n].id = id; out[n].x = (uint8_t)cx; out[n].y = (uint8_t)cy;
        out[n].z = (int8_t)z; out[n].hue = 0; ++n;
    }
    return n;
}

int vegetation_blocks(const mapgen_config *cfg, int cat, int gx, int gy) {
    uint16_t id, cover;
    veg_pick(cfg, cat, gx, gy, &id, &cover);
    (void)cover;                 /* ground cover is passable; only features block */
    return id != 0;
}
