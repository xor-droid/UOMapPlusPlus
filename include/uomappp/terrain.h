/*
 * terrain.h - procedural terrain grid: noise -> (land tile id, z).
 *
 * The grid is stored row-major (index = x + y*width) in generation space.
 * The .mul writer is responsible for translating this into the client's
 * column-major block layout, so this module stays layout-agnostic.
 */
#ifndef UOMAPGEN_TERRAIN_H
#define UOMAPGEN_TERRAIN_H

#include <stdint.h>
#include "uomappp/config.h"
#include "uomappp/tiledata.h"

/* Coarse terrain categories, used by the preview renderer. */
enum {
    TCAT_WATER_DEEP = 0,
    TCAT_WATER_SHALLOW,
    TCAT_SAND,          /* beach sand (also fords) */
    TCAT_GRASS,
    TCAT_FOREST,
    TCAT_HILL,
    TCAT_MOUNTAIN,
    TCAT_RIVER,
    TCAT_DESERT,
    TCAT_JUNGLE,
    TCAT_SWAMP,
    TCAT_SNOW,
    TCAT_LAKE,
    /* --- appended for UOMapPlusPlus civilization pass (preview only) --- */
    TCAT_ROAD,          /* dirt/cobble road + trails */
    TCAT_BRIDGE,        /* road crossing a river */
    TCAT_FLOOR,         /* building floor */
    TCAT_WALL,          /* building wall */
    TCAT_ORE,           /* resource/mineral node */
    TCAT_COUNT
};

/*
 * The grid is the shared state every pass reads and writes. Beyond the final
 * (id, z, cat) outputs, it carries reusable float "layers" that passes exchange
 * so later stages don't recompute fields earlier ones already produced.
 *
 * Phase 1 allocates and populates `hfield` (the smooth macro routing-height,
 * formerly the transient `hf`). `moisture`, `temperature`, `region` and `flags`
 * are declared here as the pipeline contract but are NULL until the pass that
 * introduces them allocates them (erosion/Voronoi/etc. in later phases). Always
 * check for NULL before reading them. terrain_free() releases whatever is set.
 */
typedef struct {
    int       width;       /* map width in tiles */
    int       height;      /* map height in tiles */
    uint16_t *id;          /* land tile id per cell */
    int8_t   *z;           /* signed z per cell */
    uint8_t  *cat;         /* TCAT_* per cell (preview + pass logic) */
    float    *hfield;      /* continuous height field / river-routing height */
    float    *moisture;    /* [reserved] per-cell moisture (NULL until used) */
    float    *temperature; /* [reserved] per-cell temperature (NULL until used) */
    int32_t  *region;      /* [reserved] Voronoi/biome territory id (NULL until used) */
    uint8_t  *flags;       /* [reserved] per-cell feature mask (NULL until used) */
} terrain_grid;

/* Forward declaration: the per-pass preview context lives in preview.h. Passing
 * NULL disables intermediate previews. */
struct preview_ctx;

/* Generate the full grid from cfg. td may be unloaded (flags optional); when
 * loaded, palette choices are validated and a warning is printed on mismatch.
 * When pv is non-NULL and pass previews are enabled in cfg, a PNG is emitted
 * after each internal pass (terrain/rivers/beaches/passes/slope).
 * Returns 0 on success, -1 on allocation failure. Caller frees with
 * terrain_free(). */
int terrain_generate(terrain_grid *g, const mapgen_config *cfg,
                     const tiledata_land *td, struct preview_ctx *pv);

void terrain_free(terrain_grid *g);

#endif /* UOMAPGEN_TERRAIN_H */
