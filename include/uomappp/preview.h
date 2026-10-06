/*
 * preview.h - optional top-down PNG render of the terrain for fast iteration.
 */
#ifndef UOMAPGEN_PREVIEW_H
#define UOMAPGEN_PREVIEW_H

#include "uomappp/terrain.h"
#include "uomappp/config.h"

/* Render the grid to a PNG at path (blue=water, green=land, grey=hills, with
 * land shaded by z). Returns 0/-1. */
int preview_write_png(const terrain_grid *g, const char *path);

/*
 * Per-pass preview context. The pipeline/terrain passes call preview_pass()
 * after each stage; when cfg->emit_pass_previews is set it writes a numbered
 * snapshot "<dir>/pass<NN>_<name>.png" (dir = cfg->pass_preview_dir, or
 * cfg->out_dir when that is empty) and advances `count`. Previews are purely
 * diagnostic: failures are warned about but never abort generation, and they
 * never affect the .mul output bytes.
 */
typedef struct preview_ctx {
    const mapgen_config *cfg;
    int                  count;
} preview_ctx;

/* Emit the next numbered pass PNG if pv is non-NULL and previews are enabled. */
void preview_pass(preview_ctx *pv, const terrain_grid *g, const char *name);

#endif /* UOMAPGEN_PREVIEW_H */
