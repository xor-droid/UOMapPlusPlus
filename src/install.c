#include "uomappp/install.h"
#include "uomappp/io.h"

#include <stdio.h>

/* Copy out_dir/<name> -> install_dir/<name>. Returns 0/-1. */
static int install_one(const mapgen_config *cfg, const char *name) {
    char src[UOMG_PATH_MAX], dst[UOMG_PATH_MAX];
    if (io_join_path(src, sizeof(src), cfg->out_dir, name) != 0 ||
        io_join_path(dst, sizeof(dst), cfg->install_dir, name) != 0) {
        fprintf(stderr, "error: install path too long for '%s'\n", name);
        return -1;
    }
    if (io_copy_file(dst, src) != 0) {
        fprintf(stderr, "error: failed to install %s -> %s\n", src, dst);
        return -1;
    }
    fprintf(stderr, "installed: %s -> %s\n", src, dst);
    return 0;
}

int install_resources(const mapgen_config *cfg) {
    if (cfg->install_dir[0] == '\0')
        return 0;   /* nothing to do */

    if (io_ensure_dir(cfg->install_dir) != 0) {
        fprintf(stderr, "error: cannot create install directory '%s'\n",
                cfg->install_dir);
        return -1;
    }

    char name[32];
    snprintf(name, sizeof(name), "map%d.mul", cfg->map_index);
    if (install_one(cfg, name) != 0)
        return -1;

    if (!cfg->terrain_only) {
        snprintf(name, sizeof(name), "staidx%d.mul", cfg->map_index);
        if (install_one(cfg, name) != 0)
            return -1;
        snprintf(name, sizeof(name), "statics%d.mul", cfg->map_index);
        if (install_one(cfg, name) != 0)
            return -1;
    }
    return 0;
}
