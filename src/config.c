#include "uomappp/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  /* strcasecmp */
#include <ctype.h>
#include <errno.h>

void config_defaults(mapgen_config *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->seed        = 0;
    cfg->map_index   = 0;
    cfg->width       = 1024;
    cfg->height      = 1024;
    cfg->sea_level   = 0.0;
    cfg->frequency   = 0.004;
    cfg->octaves     = 5;
    cfg->max_slope   = 4;
    cfg->land_z_max  = 45;
    cfg->water_z     = -5;
    cfg->continent          = 0;
    cfg->continent_radius   = 0.55;
    cfg->continent_strength = 2.0;
    cfg->continent_power    = 2.0;
    cfg->continents         = 0;
    cfg->continent_count    = 3;
    cfg->continent_scale    = 0.00045;
    cfg->continent_fill     = 1.00;   /* continent size vs map (distinct landmasses) */
    cfg->flat               = 0;
    cfg->flat_z             = 0;
    cfg->mountains          = 0;
    cfg->mountain_level     = 0.70;
    cfg->mountain_z         = 70;
    cfg->mountain_scale     = 0.0;   /* 0 => auto (frequency * 0.5) */
    cfg->rivers             = 1;     /* on by default (disable with --no-rivers) */
    cfg->river_density      = 0;
    cfg->river_width        = 3;     /* dilation rings; bigger = wider/more visible rivers */
    cfg->biomes             = 1;
    cfg->temperature_bias   = 0.0;
    cfg->vegetation         = 1;
    cfg->tree_density        = 0.08;
    cfg->rock_density        = 0.02;
    cfg->plant_density       = 0.05;
    cfg->beaches            = 1;
    cfg->beach_width        = 4;
    cfg->lakes              = 1;
    cfg->passes             = 1;
    cfg->erosion            = 1;     /* on by default (disable with --no-erosion) */
    cfg->erosion_density    = 0.20;
    cfg->erosion_lifetime   = 30;
    cfg->erosion_radius     = 3;
    cfg->erosion_erode      = 0.30;
    cfg->erosion_deposition = 0.30;
    cfg->erosion_z_scale    = 1.0;   /* eroded relief (z units) folded back 1:1 */
    cfg->regions            = 1;     /* on by default (disable with --no-regions) */
    cfg->region_spacing     = 64;
    cfg->region_jitter      = 0.6;
    cfg->wfc                = 1;     /* on by default (disable with --no-wfc); implies regions */
    cfg->cellular           = 1;     /* on by default (disable with --no-cellular) */
    cfg->cellular_fill      = 0.42;
    cfg->cellular_iterations = 4;
    cfg->towns              = 0;
    cfg->town_spacing       = 160;
    cfg->town_size          = 48;
    cfg->trails             = 1;     /* active only when towns is on */
    cfg->region_warp        = 12.0;  /* organic Voronoi borders by default (0 = crisp) */
    cfg->dither             = 1;     /* on by default (disable with --no-dither) */
    cfg->dither_strength    = 0.35;
    cfg->resources          = 0;
    cfg->resource_spacing   = 120;
    cfg->cliffs             = 0;
    cfg->terrace            = 0;
    cfg->terrace_step       = 5;
    cfg->plain_z            = 3;
    cfg->connect            = 0;
    cfg->causeways          = 0;     /* default: keep islands (no sea causeways); --causeways to bridge the open sea */
    cfg->connect_max        = 6000;  /* cost budget: mountains/inland water bridge generously, open ocean does not */
    cfg->pass_width         = 10;    /* ~20-tile-wide grassy valleys, not 1-tile scars */
    cfg->pass_slope         = 8;     /* foothill band grading mountain rock down to the valley */
    cfg->mountain_slope     = 0;     /* grade EVERY mountain perimeter N tiles (0 = sharp walls) */
    cfg->clearings          = 0;
    cfg->clearing_spacing   = 140;
    cfg->clearing_size      = 10;
    cfg->emit_mapdef = 0;
    cfg->terrain_only = 0;
    cfg->emit_pass_previews = 0;
    snprintf(cfg->out_dir, sizeof(cfg->out_dir), "%s", "./out");
    snprintf(cfg->tiledata_path, sizeof(cfg->tiledata_path), "%s",
             "./ref/UONewDawn/tiledata.mul");
    cfg->preview_path[0]     = '\0';
    cfg->pass_preview_dir[0] = '\0';
    cfg->install_dir[0]      = '\0';
    cfg->config_path[0]      = '\0';
    cfg->dump_config_path[0] = '\0';
}

static int parse_bool(const char *v, int *out) {
    if (!strcmp(v, "1") || !strcasecmp(v, "true") || !strcasecmp(v, "yes") ||
        !strcasecmp(v, "on")) { *out = 1; return 0; }
    if (!strcmp(v, "0") || !strcasecmp(v, "false") || !strcasecmp(v, "no") ||
        !strcasecmp(v, "off")) { *out = 0; return 0; }
    return -1;
}

/* Normalize a key: lowercase and treat '-' and '_' the same (-> '_'). */
static void normalize_key(char *k) {
    for (; *k; ++k) {
        if (*k == '-') *k = '_';
        else *k = (char)tolower((unsigned char)*k);
    }
}

/* Apply one key/value pair (shared by config file and, if desired, CLI).
 * Returns 0 on success, -1 on unknown key or bad value. */
int config_set_kv(mapgen_config *cfg, const char *key_in, const char *val) {
    char key[64];
    snprintf(key, sizeof(key), "%s", key_in);
    normalize_key(key);

    errno = 0;
    if (!strcmp(key, "seed")) {
        cfg->seed = strtoull(val, NULL, 0);
    } else if (!strcmp(key, "map")) {
        cfg->map_index = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "width")) {
        cfg->width = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "height")) {
        cfg->height = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "sea_level")) {
        cfg->sea_level = strtod(val, NULL);
    } else if (!strcmp(key, "frequency")) {
        cfg->frequency = strtod(val, NULL);
    } else if (!strcmp(key, "octaves")) {
        cfg->octaves = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "max_slope")) {
        cfg->max_slope = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "land_z_max")) {
        cfg->land_z_max = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "water_z")) {
        cfg->water_z = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "continent")) {
        if (parse_bool(val, &cfg->continent) != 0) return -1;
    } else if (!strcmp(key, "continent_radius")) {
        cfg->continent_radius = strtod(val, NULL);
    } else if (!strcmp(key, "continent_strength")) {
        cfg->continent_strength = strtod(val, NULL);
    } else if (!strcmp(key, "continent_power")) {
        cfg->continent_power = strtod(val, NULL);
    } else if (!strcmp(key, "continents")) {
        if (parse_bool(val, &cfg->continents) != 0) return -1;
    } else if (!strcmp(key, "continent_count")) {
        cfg->continents = 1; cfg->continent_count = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "continent_scale")) {
        cfg->continents = 1; cfg->continent_scale = strtod(val, NULL);
    } else if (!strcmp(key, "continent_fill")) {
        cfg->continents = 1; cfg->continent_fill = strtod(val, NULL);
    } else if (!strcmp(key, "flat")) {
        if (parse_bool(val, &cfg->flat) != 0) return -1;
    } else if (!strcmp(key, "flat_z")) {
        cfg->flat_z = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "mountains")) {
        if (parse_bool(val, &cfg->mountains) != 0) return -1;
    } else if (!strcmp(key, "mountain_level")) {
        cfg->mountain_level = strtod(val, NULL);
    } else if (!strcmp(key, "mountain_z")) {
        cfg->mountain_z = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "mountain_scale")) {
        cfg->mountain_scale = strtod(val, NULL);
    } else if (!strcmp(key, "rivers")) {
        if (parse_bool(val, &cfg->rivers) != 0) return -1;
    } else if (!strcmp(key, "river_density")) {
        cfg->river_density = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "river_width")) {
        cfg->rivers = 1; cfg->river_width = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "biomes")) {
        if (parse_bool(val, &cfg->biomes) != 0) return -1;
    } else if (!strcmp(key, "temperature_bias")) {
        cfg->temperature_bias = strtod(val, NULL);
    } else if (!strcmp(key, "vegetation")) {
        if (parse_bool(val, &cfg->vegetation) != 0) return -1;
    } else if (!strcmp(key, "tree_density")) {
        cfg->tree_density = strtod(val, NULL);
    } else if (!strcmp(key, "rock_density")) {
        cfg->rock_density = strtod(val, NULL);
    } else if (!strcmp(key, "plant_density")) {
        cfg->plant_density = strtod(val, NULL);
    } else if (!strcmp(key, "beaches")) {
        if (parse_bool(val, &cfg->beaches) != 0) return -1;
    } else if (!strcmp(key, "beach_width")) {
        cfg->beach_width = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "lakes")) {
        if (parse_bool(val, &cfg->lakes) != 0) return -1;
    } else if (!strcmp(key, "passes")) {
        if (parse_bool(val, &cfg->passes) != 0) return -1;
    } else if (!strcmp(key, "erosion")) {
        if (parse_bool(val, &cfg->erosion) != 0) return -1;
    } else if (!strcmp(key, "erosion_density")) {
        cfg->erosion_density = strtod(val, NULL);
    } else if (!strcmp(key, "erosion_lifetime")) {
        cfg->erosion_lifetime = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "erosion_radius")) {
        cfg->erosion_radius = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "erosion_erode")) {
        cfg->erosion_erode = strtod(val, NULL);
    } else if (!strcmp(key, "erosion_deposition")) {
        cfg->erosion_deposition = strtod(val, NULL);
    } else if (!strcmp(key, "erosion_z_scale")) {
        cfg->erosion_z_scale = strtod(val, NULL);
    } else if (!strcmp(key, "regions")) {
        if (parse_bool(val, &cfg->regions) != 0) return -1;
    } else if (!strcmp(key, "region_spacing")) {
        cfg->region_spacing = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "region_jitter")) {
        cfg->region_jitter = strtod(val, NULL);
    } else if (!strcmp(key, "wfc")) {
        if (parse_bool(val, &cfg->wfc) != 0) return -1;
        if (cfg->wfc) cfg->regions = 1;   /* WFC needs the territory map */
    } else if (!strcmp(key, "cellular")) {
        if (parse_bool(val, &cfg->cellular) != 0) return -1;
    } else if (!strcmp(key, "cellular_fill")) {
        cfg->cellular_fill = strtod(val, NULL);
    } else if (!strcmp(key, "cellular_iterations")) {
        cfg->cellular_iterations = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "towns")) {
        if (parse_bool(val, &cfg->towns) != 0) return -1;
    } else if (!strcmp(key, "town_spacing")) {
        cfg->town_spacing = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "town_size")) {
        cfg->town_size = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "trails")) {
        if (parse_bool(val, &cfg->trails) != 0) return -1;
    } else if (!strcmp(key, "region_warp")) {
        cfg->region_warp = strtod(val, NULL);
    } else if (!strcmp(key, "dither")) {
        if (parse_bool(val, &cfg->dither) != 0) return -1;
    } else if (!strcmp(key, "dither_strength")) {
        cfg->dither_strength = strtod(val, NULL);
    } else if (!strcmp(key, "resources")) {
        if (parse_bool(val, &cfg->resources) != 0) return -1;
    } else if (!strcmp(key, "resource_spacing")) {
        cfg->resource_spacing = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "cliffs")) {
        if (parse_bool(val, &cfg->cliffs) != 0) return -1;
    } else if (!strcmp(key, "terrace")) {
        if (parse_bool(val, &cfg->terrace) != 0) return -1;
    } else if (!strcmp(key, "terrace_step")) {
        cfg->terrace_step = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "plain_z")) {
        cfg->plain_z = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "connect")) {
        if (parse_bool(val, &cfg->connect) != 0) return -1;
    } else if (!strcmp(key, "causeways")) {
        if (parse_bool(val, &cfg->causeways) != 0) return -1;
    } else if (!strcmp(key, "connect_max")) {
        cfg->connect_max = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "pass_width")) {
        cfg->connect = 1; cfg->pass_width = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "pass_slope")) {
        cfg->connect = 1; cfg->pass_slope = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "mountain_slope")) {
        cfg->mountains = 1; cfg->mountain_slope = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "clearings")) {
        if (parse_bool(val, &cfg->clearings) != 0) return -1;
    } else if (!strcmp(key, "clearing_spacing")) {
        cfg->clearing_spacing = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "clearing_size")) {
        cfg->clearing_size = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "out")) {
        snprintf(cfg->out_dir, sizeof(cfg->out_dir), "%s", val);
    } else if (!strcmp(key, "tiledata")) {
        snprintf(cfg->tiledata_path, sizeof(cfg->tiledata_path), "%s", val);
    } else if (!strcmp(key, "preview")) {
        snprintf(cfg->preview_path, sizeof(cfg->preview_path), "%s", val);
    } else if (!strcmp(key, "emit_mapdef")) {
        if (parse_bool(val, &cfg->emit_mapdef) != 0) return -1;
    } else if (!strcmp(key, "terrain_only")) {
        if (parse_bool(val, &cfg->terrain_only) != 0) return -1;
    } else if (!strcmp(key, "pass_previews")) {
        if (parse_bool(val, &cfg->emit_pass_previews) != 0) return -1;
    } else if (!strcmp(key, "pass_preview_dir")) {
        snprintf(cfg->pass_preview_dir, sizeof(cfg->pass_preview_dir), "%s", val);
    } else if (!strcmp(key, "install_dir")) {
        snprintf(cfg->install_dir, sizeof(cfg->install_dir), "%s", val);
    } else if (!strcmp(key, "preset")) {
        return config_apply_preset(cfg, val);
    } else {
        fprintf(stderr, "error: unknown config key '%s'\n", key_in);
        return -1;
    }
    return 0;
}

int config_load_file(mapgen_config *cfg, const char *path) {
    if (!path || path[0] == '\0')
        return 0;
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "error: cannot open config file '%s'\n", path);
        return -1;
    }

    char line[1024];
    int lineno = 0, rc = 0;
    while (fgets(line, sizeof(line), f)) {
        ++lineno;
        char *s = line;
        while (*s && isspace((unsigned char)*s)) ++s;       /* ltrim */
        if (*s == '\0' || *s == '#' || *s == ';')            /* comment/blank */
            continue;

        char *eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "error: %s:%d: expected key = value\n", path, lineno);
            rc = -1; break;
        }
        *eq = '\0';
        char *key = s;
        char *val = eq + 1;

        /* rtrim key */
        char *ke = key + strlen(key);
        while (ke > key && isspace((unsigned char)ke[-1])) *--ke = '\0';
        /* ltrim val */
        while (*val && isspace((unsigned char)*val)) ++val;
        /* strip an inline comment (whitespace-preceded # or ;), but not when
         * the value is quoted (a quoted path may legitimately contain them). */
        if (*val != '"' && *val != '\'') {
            for (char *p = val; *p; ++p) {
                if ((*p == '#' || *p == ';') &&
                    (p == val || isspace((unsigned char)p[-1]))) {
                    *p = '\0';
                    break;
                }
            }
        }
        /* rtrim val */
        char *ve = val + strlen(val);
        while (ve > val && isspace((unsigned char)ve[-1])) *--ve = '\0';
        /* strip surrounding quotes on value */
        size_t vlen = strlen(val);
        if (vlen >= 2 && ((val[0] == '"' && val[vlen-1] == '"') ||
                          (val[0] == '\'' && val[vlen-1] == '\''))) {
            val[vlen-1] = '\0';
            ++val;
        }

        if (config_set_kv(cfg, key, val) != 0) {
            fprintf(stderr, "       (in %s:%d)\n", path, lineno);
            rc = -1; break;
        }
    }
    fclose(f);
    return rc;
}

int config_apply_preset(mapgen_config *cfg, const char *name) {
    if (!strcasecmp(name, "test")) {
        cfg->width = 1024; cfg->height = 1024;
        return 0;
    }
    if (!strcasecmp(name, "felucca")) {
        cfg->width = 7168; cfg->height = 4096;
        return 0;
    }
    if (!strcasecmp(name, "britannia")) {
        /* Calibrated to the real Felucca map0 (measured): ~50% water, land
         * elevation overwhelmingly flat (z=0 dominant) in large equal-z
         * plateaus with sharp relief only at rare mountains -- i.e. terraced,
         * low-amplitude land rather than continuous noise. */
        cfg->width = 7168; cfg->height = 4096;
        cfg->continent = 1;              /* one main organic landmass */
        cfg->continent_radius   = 0.82;
        cfg->continent_strength = 1.8;
        cfg->continent_power    = 2.0;
        cfg->sea_level = -0.26;          /* ~50% water (ocean + inland lakes/rivers) */
        cfg->frequency = 0.0013;         /* large coherent landforms at felucca scale */
        cfg->mountains = 1;
        cfg->mountain_level = 0.60;
        cfg->mountain_z = 70;
        cfg->rivers = 1;
        cfg->biomes = 1;
        cfg->land_z_max = 8;             /* low plains; mountains carry the height */
        cfg->terrace = 1;                /* flat plateaus, not continuous noise */
        cfg->terrace_step = 6;
        cfg->plain_z = 4;
        cfg->regions = 1;                /* coherent biome territories, not per-cell confetti */
        cfg->region_spacing = 600;       /* ~Britannia-scale biome regions */
        cfg->region_warp = 50.0;         /* organic (non-polygonal) biome borders */
        cfg->dither = 1;                 /* soft biome transitions */
        /* Measured match vs the real Felucca map0: ~50% water, ~60% of land at
         * z=0, ~10% above z=20, ~85-89% of adjacent land at equal z (flat
         * plateaus) -- Britannia-like, not noise. */
        return 0;
    }
    fprintf(stderr, "error: unknown preset '%s' (use 'test', 'felucca' or 'britannia')\n", name);
    return -1;
}

int config_dump(const mapgen_config *cfg, const char *path) {
    if (!path || path[0] == '\0')
        return 0;
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "error: cannot write config dump '%s'\n", path);
        return -1;
    }
    fprintf(f, "# uomappp resolved config (defaults + config file + CLI, preset expanded).\n");
    fprintf(f, "# Reproduce this exact map (same build) with:  uomappp --config <this-file>\n\n");

    /* Scalars / params first. */
    fprintf(f, "seed = %llu\n", (unsigned long long)cfg->seed);
    fprintf(f, "map = %d\n", cfg->map_index);
    fprintf(f, "width = %d\n", cfg->width);
    fprintf(f, "height = %d\n", cfg->height);
    fprintf(f, "sea_level = %.9g\n", cfg->sea_level);
    fprintf(f, "frequency = %.9g\n", cfg->frequency);
    fprintf(f, "octaves = %d\n", cfg->octaves);
    fprintf(f, "max_slope = %d\n", cfg->max_slope);
    fprintf(f, "land_z_max = %d\n", cfg->land_z_max);
    fprintf(f, "water_z = %d\n", cfg->water_z);
    fprintf(f, "continent_radius = %.9g\n", cfg->continent_radius);
    fprintf(f, "continent_strength = %.9g\n", cfg->continent_strength);
    fprintf(f, "continent_power = %.9g\n", cfg->continent_power);
    fprintf(f, "continent_count = %d\n", cfg->continent_count);
    fprintf(f, "continent_scale = %.9g\n", cfg->continent_scale);
    fprintf(f, "continent_fill = %.9g\n", cfg->continent_fill);
    fprintf(f, "flat_z = %d\n", cfg->flat_z);
    fprintf(f, "mountain_level = %.9g\n", cfg->mountain_level);
    fprintf(f, "mountain_z = %d\n", cfg->mountain_z);
    fprintf(f, "mountain_scale = %.9g\n", cfg->mountain_scale);
    fprintf(f, "river_density = %d\n", cfg->river_density);
    fprintf(f, "river_width = %d\n", cfg->river_width);
    fprintf(f, "temperature_bias = %.9g\n", cfg->temperature_bias);
    fprintf(f, "tree_density = %.9g\n", cfg->tree_density);
    fprintf(f, "rock_density = %.9g\n", cfg->rock_density);
    fprintf(f, "plant_density = %.9g\n", cfg->plant_density);
    fprintf(f, "beach_width = %d\n", cfg->beach_width);
    fprintf(f, "erosion_density = %.9g\n", cfg->erosion_density);
    fprintf(f, "erosion_lifetime = %d\n", cfg->erosion_lifetime);
    fprintf(f, "erosion_radius = %d\n", cfg->erosion_radius);
    fprintf(f, "erosion_erode = %.9g\n", cfg->erosion_erode);
    fprintf(f, "erosion_deposition = %.9g\n", cfg->erosion_deposition);
    fprintf(f, "erosion_z_scale = %.9g\n", cfg->erosion_z_scale);
    fprintf(f, "region_spacing = %d\n", cfg->region_spacing);
    fprintf(f, "region_jitter = %.9g\n", cfg->region_jitter);
    fprintf(f, "region_warp = %.9g\n", cfg->region_warp);
    fprintf(f, "cellular_fill = %.9g\n", cfg->cellular_fill);
    fprintf(f, "cellular_iterations = %d\n", cfg->cellular_iterations);
    fprintf(f, "town_spacing = %d\n", cfg->town_spacing);
    fprintf(f, "town_size = %d\n", cfg->town_size);
    fprintf(f, "dither_strength = %.9g\n", cfg->dither_strength);
    fprintf(f, "resource_spacing = %d\n", cfg->resource_spacing);
    fprintf(f, "terrace_step = %d\n", cfg->terrace_step);
    fprintf(f, "plain_z = %d\n", cfg->plain_z);
    fprintf(f, "connect_max = %d\n", cfg->connect_max);
    fprintf(f, "pass_width = %d\n", cfg->pass_width);
    fprintf(f, "pass_slope = %d\n", cfg->pass_slope);
    fprintf(f, "mountain_slope = %d\n", cfg->mountain_slope);
    fprintf(f, "clearing_spacing = %d\n", cfg->clearing_spacing);
    fprintf(f, "clearing_size = %d\n", cfg->clearing_size);
    fprintf(f, "out = %s\n", cfg->out_dir);
    fprintf(f, "tiledata = %s\n", cfg->tiledata_path);
    if (cfg->preview_path[0])     fprintf(f, "preview = %s\n", cfg->preview_path);
    if (cfg->pass_preview_dir[0]) fprintf(f, "pass_preview_dir = %s\n", cfg->pass_preview_dir);
    /* install_dir intentionally omitted: it is an output side-action, not part of
     * the map; re-run with --install-dir if you want to swap files again. */

    /* Booleans LAST so that keys which imply a toggle on read (continent_count/
     * scale/fill => continents, wfc => regions) are overridden by the explicit
     * value here, giving a faithful round-trip. */
    fprintf(f, "\n");
    fprintf(f, "continent = %s\n",    cfg->continent ? "true" : "false");
    fprintf(f, "continents = %s\n",   cfg->continents ? "true" : "false");
    fprintf(f, "flat = %s\n",         cfg->flat ? "true" : "false");
    fprintf(f, "mountains = %s\n",    cfg->mountains ? "true" : "false");
    fprintf(f, "rivers = %s\n",       cfg->rivers ? "true" : "false");
    fprintf(f, "biomes = %s\n",       cfg->biomes ? "true" : "false");
    fprintf(f, "vegetation = %s\n",   cfg->vegetation ? "true" : "false");
    fprintf(f, "beaches = %s\n",      cfg->beaches ? "true" : "false");
    fprintf(f, "lakes = %s\n",        cfg->lakes ? "true" : "false");
    fprintf(f, "passes = %s\n",       cfg->passes ? "true" : "false");
    fprintf(f, "erosion = %s\n",      cfg->erosion ? "true" : "false");
    fprintf(f, "regions = %s\n",      cfg->regions ? "true" : "false");
    fprintf(f, "wfc = %s\n",          cfg->wfc ? "true" : "false");
    fprintf(f, "cellular = %s\n",     cfg->cellular ? "true" : "false");
    fprintf(f, "towns = %s\n",        cfg->towns ? "true" : "false");
    fprintf(f, "trails = %s\n",       cfg->trails ? "true" : "false");
    fprintf(f, "dither = %s\n",       cfg->dither ? "true" : "false");
    fprintf(f, "resources = %s\n",    cfg->resources ? "true" : "false");
    fprintf(f, "cliffs = %s\n",       cfg->cliffs ? "true" : "false");
    fprintf(f, "terrace = %s\n",      cfg->terrace ? "true" : "false");
    fprintf(f, "connect = %s\n",      cfg->connect ? "true" : "false");
    fprintf(f, "causeways = %s\n",    cfg->causeways ? "true" : "false");
    fprintf(f, "clearings = %s\n",    cfg->clearings ? "true" : "false");
    fprintf(f, "emit_mapdef = %s\n",  cfg->emit_mapdef ? "true" : "false");
    fprintf(f, "terrain_only = %s\n", cfg->terrain_only ? "true" : "false");
    fprintf(f, "pass_previews = %s\n", cfg->emit_pass_previews ? "true" : "false");

    if (fclose(f) != 0) {
        fprintf(stderr, "error: failed writing config dump '%s'\n", path);
        return -1;
    }
    return 0;
}

int config_validate(const mapgen_config *cfg) {
    int ok = 1;
    if (cfg->width <= 0 || cfg->width % 8 != 0) {
        fprintf(stderr, "error: width (%d) must be > 0 and a multiple of 8\n", cfg->width); ok = 0;
    }
    if (cfg->height <= 0 || cfg->height % 8 != 0) {
        fprintf(stderr, "error: height (%d) must be > 0 and a multiple of 8\n", cfg->height); ok = 0;
    }
    if (cfg->octaves <= 0) {
        fprintf(stderr, "error: octaves (%d) must be > 0\n", cfg->octaves); ok = 0;
    }
    if (cfg->map_index < 0 || cfg->map_index > 255) {
        fprintf(stderr, "error: map (%d) must be in 0..255\n", cfg->map_index); ok = 0;
    }
    if (cfg->sea_level < -1.0 || cfg->sea_level > 1.0) {
        fprintf(stderr, "error: sea-level (%g) must be in [-1,1]\n", cfg->sea_level); ok = 0;
    }
    if (cfg->land_z_max < 0 || cfg->land_z_max > 127) {
        fprintf(stderr, "error: land-z-max (%d) must be in 0..127\n", cfg->land_z_max); ok = 0;
    }
    if (cfg->water_z < -128 || cfg->water_z > 127) {
        fprintf(stderr, "error: water-z (%d) must be in -128..127\n", cfg->water_z); ok = 0;
    }
    if (cfg->continents && cfg->continent_scale <= 0.0) {
        fprintf(stderr, "error: continent-scale (%g) must be > 0\n", cfg->continent_scale); ok = 0;
    }
    if (cfg->continents && (cfg->continent_count < 1 || cfg->continent_count > 64)) {
        fprintf(stderr, "error: continent-count (%d) must be in 1..64\n", cfg->continent_count); ok = 0;
    }
    if (cfg->continents && (cfg->continent_fill <= 0.0 || cfg->continent_fill > 2.0)) {
        fprintf(stderr, "error: continent-fill (%g) must be in (0,2]\n", cfg->continent_fill); ok = 0;
    }
    if (cfg->mountains && (cfg->mountain_level < 0.0 || cfg->mountain_level >= 1.0)) {
        fprintf(stderr, "error: mountain-level (%g) must be in [0,1)\n", cfg->mountain_level); ok = 0;
    }
    if (cfg->mountains && (cfg->mountain_z < 0 || cfg->mountain_z > 127)) {
        fprintf(stderr, "error: mountain-z (%d) must be in 0..127\n", cfg->mountain_z); ok = 0;
    }
    if (cfg->mountains && cfg->mountain_scale < 0.0) {
        fprintf(stderr, "error: mountain-scale (%g) must be >= 0\n", cfg->mountain_scale); ok = 0;
    }
    if (cfg->flat && (cfg->flat_z < -128 || cfg->flat_z > 127)) {
        fprintf(stderr, "error: flat-z (%d) must be in -128..127\n", cfg->flat_z); ok = 0;
    }
    if (cfg->rivers && cfg->river_density < 0) {
        fprintf(stderr, "error: river-density (%d) must be >= 0\n", cfg->river_density); ok = 0;
    }
    if (cfg->rivers && (cfg->river_width < 1 || cfg->river_width > 32)) {
        fprintf(stderr, "error: river-width (%d) must be in 1..32\n", cfg->river_width); ok = 0;
    }
    if (cfg->tree_density < 0.0 || cfg->tree_density > 1.0 ||
        cfg->rock_density < 0.0 || cfg->rock_density > 1.0 ||
        cfg->plant_density < 0.0 || cfg->plant_density > 1.0) {
        fprintf(stderr, "error: densities (tree/rock/plant) must be in [0,1]\n"); ok = 0;
    }
    if (cfg->beaches && (cfg->beach_width < 0 || cfg->beach_width > 64)) {
        fprintf(stderr, "error: beach-width (%d) must be in 0..64\n", cfg->beach_width); ok = 0;
    }
    if (cfg->erosion) {
        if (cfg->erosion_density < 0.0) {
            fprintf(stderr, "error: erosion-density (%g) must be >= 0\n", cfg->erosion_density); ok = 0;
        }
        if (cfg->erosion_lifetime <= 0) {
            fprintf(stderr, "error: erosion-lifetime (%d) must be > 0\n", cfg->erosion_lifetime); ok = 0;
        }
        if (cfg->erosion_radius < 1 || cfg->erosion_radius > 16) {
            fprintf(stderr, "error: erosion-radius (%d) must be in 1..16\n", cfg->erosion_radius); ok = 0;
        }
        if (cfg->erosion_erode < 0.0 || cfg->erosion_erode > 1.0 ||
            cfg->erosion_deposition < 0.0 || cfg->erosion_deposition > 1.0) {
            fprintf(stderr, "error: erosion-erode/deposition must be in [0,1]\n"); ok = 0;
        }
        if (cfg->erosion_z_scale < 0.0) {
            fprintf(stderr, "error: erosion-z-scale (%g) must be >= 0\n", cfg->erosion_z_scale); ok = 0;
        }
    }
    if (cfg->regions) {
        if (cfg->region_spacing < 4) {
            fprintf(stderr, "error: region-spacing (%d) must be >= 4\n", cfg->region_spacing); ok = 0;
        }
        if (cfg->region_jitter < 0.0 || cfg->region_jitter > 1.0) {
            fprintf(stderr, "error: region-jitter (%g) must be in [0,1]\n", cfg->region_jitter); ok = 0;
        }
    }
    if (cfg->wfc && !cfg->regions) {
        fprintf(stderr, "error: --wfc requires --regions (the territory map)\n"); ok = 0;
    }
    if (cfg->cellular) {
        if (cfg->cellular_fill < 0.0 || cfg->cellular_fill > 1.0) {
            fprintf(stderr, "error: cellular-fill (%g) must be in [0,1]\n", cfg->cellular_fill); ok = 0;
        }
        if (cfg->cellular_iterations < 0) {
            fprintf(stderr, "error: cellular-iterations (%d) must be >= 0\n", cfg->cellular_iterations); ok = 0;
        }
    }
    if (cfg->towns) {
        if (cfg->town_spacing < 16) {
            fprintf(stderr, "error: town-spacing (%d) must be >= 16\n", cfg->town_spacing); ok = 0;
        }
        if (cfg->town_size < 12 || cfg->town_size > 512) {
            fprintf(stderr, "error: town-size (%d) must be in 12..512\n", cfg->town_size); ok = 0;
        }
    }
    if (cfg->region_warp < 0.0) {
        fprintf(stderr, "error: region-warp (%g) must be >= 0\n", cfg->region_warp); ok = 0;
    }
    if (cfg->dither && (cfg->dither_strength < 0.0 || cfg->dither_strength > 1.0)) {
        fprintf(stderr, "error: dither-strength (%g) must be in [0,1]\n", cfg->dither_strength); ok = 0;
    }
    if (cfg->resources && cfg->resource_spacing < 8) {
        fprintf(stderr, "error: resource-spacing (%d) must be >= 8\n", cfg->resource_spacing); ok = 0;
    }
    if (cfg->terrace) {
        if (cfg->terrace_step < 1 || cfg->terrace_step > 127) {
            fprintf(stderr, "error: terrace-step (%d) must be in 1..127\n", cfg->terrace_step); ok = 0;
        }
        if (cfg->plain_z < 0 || cfg->plain_z > 127) {
            fprintf(stderr, "error: plain-z (%d) must be in 0..127\n", cfg->plain_z); ok = 0;
        }
    }
    if (cfg->connect && cfg->connect_max < 1) {
        fprintf(stderr, "error: connect-max (%d) must be >= 1\n", cfg->connect_max); ok = 0;
    }
    if (cfg->connect && (cfg->pass_width < 1 || cfg->pass_width > 32)) {
        fprintf(stderr, "error: pass-width (%d) must be in 1..32\n", cfg->pass_width); ok = 0;
    }
    if (cfg->connect && (cfg->pass_slope < 0 || cfg->pass_slope > 64)) {
        fprintf(stderr, "error: pass-slope (%d) must be in 0..64\n", cfg->pass_slope); ok = 0;
    }
    if (cfg->mountain_slope < 0 || cfg->mountain_slope > 64) {
        fprintf(stderr, "error: mountain-slope (%d) must be in 0..64\n", cfg->mountain_slope); ok = 0;
    }
    if (cfg->clearings) {
        if (cfg->clearing_spacing < 16) {
            fprintf(stderr, "error: clearing-spacing (%d) must be >= 16\n", cfg->clearing_spacing); ok = 0;
        }
        if (cfg->clearing_size < 2 || cfg->clearing_size > 64) {
            fprintf(stderr, "error: clearing-size (%d) must be in 2..64\n", cfg->clearing_size); ok = 0;
        }
    }
    if (cfg->continent) {
        if (cfg->continent_radius < 0.0 || cfg->continent_radius >= 1.0) {
            fprintf(stderr, "error: continent-radius (%g) must be in [0,1)\n", cfg->continent_radius); ok = 0;
        }
        if (cfg->continent_strength <= 0.0) {
            fprintf(stderr, "error: continent-strength (%g) must be > 0\n", cfg->continent_strength); ok = 0;
        }
        if (cfg->continent_power <= 0.0) {
            fprintf(stderr, "error: continent-power (%g) must be > 0\n", cfg->continent_power); ok = 0;
        }
    }
    return ok ? 0 : -1;
}
