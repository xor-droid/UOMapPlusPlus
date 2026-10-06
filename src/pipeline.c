#include "uomappp/pipeline.h"
#include "uomappp/io.h"
#include "uomappp/tiledata.h"
#include "uomappp/terrain.h"
#include "uomappp/mapwriter.h"
#include "uomappp/statics.h"
#include "uomappp/preview.h"
#include "uomappp/mapdef.h"
#include "uomappp/install.h"

#include <stdio.h>

int pipeline_run(const mapgen_config *cfg) {
    const int BW = cfg->width >> 3, BH = cfg->height >> 3;

    /* Optional tile-flag reference (never fails hard). */
    tiledata_land td;
    tiledata_load(&td, cfg->tiledata_path);
    if (!td.loaded)
        fprintf(stderr, "note: tiledata not loaded from '%s' (palette flags unchecked)\n",
                cfg->tiledata_path);

    /* Per-pass preview context: snapshots land in pass_preview_dir (or out_dir).
     * Make sure that directory exists before any pass tries to write into it. */
    preview_ctx pv = { cfg, 0 };
    if (cfg->emit_pass_previews) {
        const char *pdir = cfg->pass_preview_dir[0] ? cfg->pass_preview_dir
                                                     : cfg->out_dir;
        if (io_ensure_dir(pdir) != 0)
            fprintf(stderr, "warning: cannot create pass-preview dir '%s'\n", pdir);
    }

    /* Terrain passes (noise/mountains/biomes -> rivers -> beaches -> passes ->
     * slope). Each emits its own PNG via pv when previews are enabled. */
    terrain_grid grid;
    if (terrain_generate(&grid, cfg, &td, &pv) != 0) {
        fprintf(stderr, "error: terrain generation failed (out of memory?)\n");
        return 1;
    }

    int rc = 0;
    if (mapwriter_write(&grid, cfg->out_dir, cfg->map_index) != 0) rc = 1;

    if (rc == 0 && !cfg->terrain_only)
        if (statics_write(&grid, cfg, cfg->out_dir, cfg->map_index) != 0) rc = 1;

    /* Final preview: a numbered pass snapshot (when pass previews are on) and/or
     * the explicit --preview path for backward compatibility. */
    if (rc == 0) {
        preview_pass(&pv, &grid, "final_complete-map");
        if (cfg->preview_path[0])
            if (preview_write_png(&grid, cfg->preview_path) != 0) rc = 1;
    }

    if (rc == 0 && cfg->emit_mapdef)
        if (mapdef_write_snippet(cfg->out_dir, cfg->map_index,
                                 cfg->width, cfg->height) != 0) rc = 1;

    /* Swap the resource files into the UOFiddler/ModernUO data dir (no-op when
     * install_dir is empty). */
    if (rc == 0)
        if (install_resources(cfg) != 0) rc = 1;

    terrain_free(&grid);

    if (rc == 0) {
        long long map_bytes = (long long)BW * BH * 196;
        printf("done: map%d.mul = %lld bytes%s%s%s\n",
               cfg->map_index, map_bytes,
               cfg->terrain_only ? "" : ", staidx/statics written",
               cfg->preview_path[0] ? ", preview written" : "",
               cfg->install_dir[0] ? ", installed" : "");
    }
    return rc;
}
