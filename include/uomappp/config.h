/*
 * config.h - central configuration for uomapgen.
 *
 * A single mapgen_config struct is populated from (in order of increasing
 * precedence): built-in defaults -> config file (--config) -> CLI flags.
 * There is deliberately NO environment-variable input anywhere.
 */
#ifndef UOMAPGEN_CONFIG_H
#define UOMAPGEN_CONFIG_H

#include <stdint.h>
#include <limits.h>

#ifndef UOMG_PATH_MAX
#define UOMG_PATH_MAX 4096
#endif

typedef struct {
    uint64_t seed;          /* master seed; byte-identical output per seed */
    int      map_index;     /* facet / fileIndex N -> mapN.mul etc. */
    int      width;         /* tiles, multiple of 8 */
    int      height;        /* tiles, multiple of 8 */
    double   sea_level;     /* elevation noise threshold in [-1,1] for water */
    double   frequency;     /* base noise frequency */
    int      octaves;       /* fbm octaves */
    int      max_slope;     /* max |z| step between adjacent land tiles */
    int      land_z_max;    /* highest land z produced from elevation */
    int      water_z;       /* flat z assigned to water cells */
    int      continent;     /* bool: apply radial falloff -> single central continent */
    double   continent_radius;   /* [0,1): solid-land core radius before falloff */
    double   continent_strength; /* how hard edges are pushed to ocean (>0) */
    double   continent_power;    /* falloff curvature (>0) */
    int      continents;    /* bool: multiple continents via placed centers */
    int      continent_count;    /* number of continents when continents=1 */
    double   continent_scale;    /* frequency of the coastline-warp noise */
    double   continent_fill;     /* continent size vs map (auto ~50% land at ~0.8) */
    int      flat;          /* bool: level ground everywhere (land z = flat_z) */
    int      flat_z;             /* the single z used for all land when flat */
    int      mountains;     /* bool: add ridged mountain ranges */
    double   mountain_level;     /* ridge threshold in [0,1]; higher = less rock */
    int      mountain_z;         /* extra z added at mountain peaks */
    double   mountain_scale;     /* mountain ridge frequency (0 = auto from frequency) */
    int      rivers;        /* bool: carve downhill rivers from high ground */
    int      river_density;      /* source count (0 = auto from map size) */
    int      biomes;        /* bool: climate-band biomes (snow/desert/jungle/swamp) */
    double   temperature_bias;   /* shift climate warmer(+)/colder(-) */
    int      vegetation;    /* bool: place tree/rock/plant statics */
    double   tree_density;       /* [0,1] fraction of eligible cells with a tree */
    double   rock_density;       /* [0,1] rocks/boulders on hills/mountains */
    double   plant_density;      /* [0,1] ground cover (plants/flowers/ferns) */
    int      beaches;       /* bool: sloped sand beaches around all coasts */
    int      beach_width;        /* beach band width in tiles */
    int      lakes;         /* bool: form lakes at inland river sinks */
    int      passes;        /* bool: carve walkable passes through mountains */
    int      erosion;       /* bool: hydraulic (droplet) erosion before rivers */
    double   erosion_density;    /* droplets = density * width * height */
    int      erosion_lifetime;   /* max steps per droplet */
    int      erosion_radius;     /* erosion brush radius in tiles */
    double   erosion_erode;      /* [0,1] fraction of free capacity eroded per step */
    double   erosion_deposition; /* [0,1] fraction of excess sediment deposited */
    double   erosion_z_scale;    /* how strongly eroded relief folds back into z (1.0=full) */
    int      regions;       /* bool: Voronoi climate territories (organic biomes) */
    int      region_spacing;     /* Voronoi site spacing in tiles (territory size) */
    double   region_jitter;      /* [0,1] site jitter within its grid cell */
    int      wfc;           /* bool: WFC biome transitions over territories (implies regions) */
    int      cellular;      /* bool: cellular-automata forest clumping */
    double   cellular_fill;      /* [0,1] forest random-fill probability */
    int      cellular_iterations;/* CA smoothing iterations */
    int      towns;         /* bool: towns + roads/bridges + buildings */
    int      town_spacing;       /* min distance between towns (Poisson radius) */
    int      town_size;          /* town footprint size in tiles */
    int      trails;             /* bool: L-system side-trails from towns */
    double   region_warp;        /* Voronoi boundary domain-warp amplitude (tiles; 0=crisp) */
    int      dither;        /* bool: stipple biome borders into soft transitions */
    double   dither_strength;    /* [0,1] fraction of border cells to swap */
    int      resources;     /* bool: Poisson-disc resource (ore) nodes */
    int      resource_spacing;   /* min distance between resource nodes (tiles) */
    int      cliffs;        /* bool: varied mountain rock tiles + cliff-face statics */
    int      terrace;       /* bool: flatten land into plateaus (Britannia-like) */
    int      terrace_step;       /* z quantization step for terraces */
    int      plain_z;            /* land at or below |plain_z| snaps flat to 0 */
    int      emit_mapdef;   /* bool: also write map-definitions.snippet.json */
    int      terrain_only;  /* bool: write only mapN.mul (skip statics) */
    int      emit_pass_previews; /* bool: write a PNG snapshot after each pass */
    char     out_dir[UOMG_PATH_MAX];
    char     tiledata_path[UOMG_PATH_MAX];
    char     preview_path[UOMG_PATH_MAX];      /* empty string = no final preview */
    char     pass_preview_dir[UOMG_PATH_MAX];  /* dir for pass PNGs (empty => out_dir) */
    char     install_dir[UOMG_PATH_MAX];       /* copy final .mul triplet here (empty => skip) */
    char     config_path[UOMG_PATH_MAX];       /* empty string = none */
    char     dump_config_path[UOMG_PATH_MAX];  /* write resolved config here (empty => skip) */
} mapgen_config;

/* Fill cfg with built-in defaults. */
void config_defaults(mapgen_config *cfg);

/* Parse an INI-like "key = value" file into cfg (keys mirror CLI long opts
 * with '-' replaced by '_'). Returns 0 on success, -1 on error (message to
 * stderr). Missing file when path is empty is a no-op success. */
int config_load_file(mapgen_config *cfg, const char *path);

/* Apply a named preset ("test", "felucca"). Returns 0 or -1 if unknown. */
int config_apply_preset(mapgen_config *cfg, const char *name);

/* Apply a single key/value pair (keys mirror CLI long options, '-' or '_').
 * Used by the config-file parser. Returns 0 on success, -1 on error. */
int config_set_kv(mapgen_config *cfg, const char *key, const char *val);

/* Validate ranges (dims multiple of 8 and > 0, octaves > 0, etc.).
 * Returns 0 on success, -1 on error (message to stderr). */
int config_validate(const mapgen_config *cfg);

/* Write the fully-resolved config (defaults + file + CLI, after a preset is
 * expanded) to path as INI "key = value" lines that config_load_file reads
 * back. Running `--config <path>` then reproduces this exact map. Returns 0 on
 * success (incl. empty path = no-op), -1 on write error. */
int config_dump(const mapgen_config *cfg, const char *path);

#endif /* UOMAPGEN_CONFIG_H */
