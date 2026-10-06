/*
 * UOMapPlusPlus (uomappp) - multi-library procedural Ultima Online map generator for ModernUO.
 *
 * Pipeline: config (defaults -> file -> CLI) -> noise -> terrain grid ->
 * mapN.mul (+ empty staidxN/staticsN) [+ preview PNG] [+ mapdef snippet].
 *
 * No environment variables are consulted anywhere by design.
 */
#include "uomappp/config.h"
#include "uomappp/io.h"
#include "uomappp/pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#ifndef UOMG_VERSION
#define UOMG_VERSION "0.1.0"
#endif

/* Long-only option codes (no short equivalents). */
enum {
    OPT_CONT_RADIUS = 1000,
    OPT_CONT_STRENGTH,
    OPT_CONT_POWER,
    OPT_CONT_SCALE,
    OPT_CONT_COUNT,
    OPT_CONT_FILL,
    OPT_MOUNTAINS,
    OPT_MTN_LEVEL,
    OPT_MTN_Z,
    OPT_MTN_SCALE,
    OPT_FLAT,
    OPT_FLAT_Z,
    OPT_RIVERS,
    OPT_RIVER_DENSITY,
    OPT_NO_BIOMES,
    OPT_TEMP_BIAS,
    OPT_NO_VEG,
    OPT_TREE_DEN,
    OPT_ROCK_DEN,
    OPT_PLANT_DEN,
    OPT_NO_BEACHES,
    OPT_BEACH_WIDTH,
    OPT_NO_LAKES,
    OPT_NO_PASSES,
    OPT_EROSION,
    OPT_EROSION_DENSITY,
    OPT_EROSION_LIFETIME,
    OPT_EROSION_RADIUS,
    OPT_EROSION_ZSCALE,
    OPT_REGIONS,
    OPT_REGION_SPACING,
    OPT_REGION_JITTER,
    OPT_WFC,
    OPT_CELLULAR,
    OPT_CELLULAR_FILL,
    OPT_CELLULAR_ITERS,
    OPT_TOWNS,
    OPT_TOWN_SPACING,
    OPT_TOWN_SIZE,
    OPT_NO_TRAILS,
    OPT_REGION_WARP,
    OPT_DITHER,
    OPT_DITHER_STRENGTH,
    OPT_RESOURCES,
    OPT_RESOURCE_SPACING,
    OPT_CLIFFS,
    OPT_TERRACE,
    OPT_TERRACE_STEP,
    OPT_PLAIN_Z,
    OPT_CONNECT,
    OPT_CONNECT_MAX,
    OPT_PASS_WIDTH,
    OPT_PASS_SLOPE,
    OPT_CLEARINGS,
    OPT_CLEARING_SPACING,
    OPT_CLEARING_SIZE,
    OPT_PASS_PREVIEWS,
    OPT_PASS_PREVIEW_DIR,
    OPT_INSTALL_DIR,
    OPT_DUMP_CONFIG
};

static void print_version(void) {
    printf("uomappp %s\n", UOMG_VERSION);
}

static void print_help(const char *argv0) {
    printf(
"uomappp %s - multi-library procedural UO map generator for ModernUO\n"
"\n"
"Usage: %s [options]\n"
"\n"
"Output is byte-identical for a given build + seed/config.\n"
"Writes map<N>.mul and (unless --terrain-only) empty staidx<N>.mul and\n"
"statics<N>.mul into the output directory.\n"
"\n"
"Options:\n"
"  --seed <uint64>       Master seed (default 0). Same seed => same bytes.\n"
"  --config <file>       Read an INI-like config file first; CLI flags override it.\n"
"  --map <N>             Facet / file index -> map<N>.mul triplet (default 0).\n"
"  --width <tiles>       Map width, multiple of 8 (default 1024).\n"
"  --height <tiles>      Map height, multiple of 8 (default 1024).\n"
"  --preset <name>       'test' (1024x1024), 'felucca' (7168x4096), or\n"
"                        'britannia' (felucca size, calibrated to the real map).\n"
"  --out <dir>           Output directory (default ./out).\n"
"  --sea-level <float>   Water threshold on elevation noise, [-1,1] (default 0.0).\n"
"  --frequency <float>   Base noise frequency (default 0.004).\n"
"  --octaves <int>       fBm octaves (default 5).\n"
"  --max-slope <int>     Max z step between adjacent land tiles (default 4).\n"
"  --land-z-max <int>    Highest land z from elevation, 0..127 (default 45).\n"
"  --water-z <int>       Flat z for water cells (default -5).\n"
"  --continent           Radial falloff: one large central landmass ringed by ocean.\n"
"  --continent-radius <f>    Solid-land core radius, [0,1) (default 0.55; implies --continent).\n"
"  --continent-strength <f>  How hard edges fall to ocean (default 2.0; implies --continent).\n"
"  --continent-power <f>     Falloff curvature (default 2.0; implies --continent).\n"
"  --continents          Multiple continents (placed centers), ocean between them.\n"
"  --continent-count <n>     Exact number of continents, positions by seed\n"
"                            (default 3; implies --continents).\n"
"  --continent-fill <f>      Continent size vs map; ~0.8 => ~50%% land at any\n"
"                            count (implies --continents).\n"
"  --continent-scale <f>     Coastline-warp frequency (default 0.00045).\n"
"  --flat                Level non-mountain ground to one z (mountains keep their height).\n"
"  --flat-z <int>            The z for flat ground; mountains rise above it (default 0; implies --flat).\n"
"  --mountains           Add ridged mountain ranges (impassable rock peaks).\n"
"  --mountain-level <f>      Ridge threshold [0,1); higher = fewer/sparser ranges (default 0.70).\n"
"  --mountain-z <int>        Extra z at peaks, 0..127 (default 70).\n"
"  --mountain-scale <f>      Ridge frequency; smaller = bigger/broader ranges\n"
"                            (default: auto = frequency*0.5).\n"
"  --rivers              Carve downhill rivers from high ground to the sea.\n"
"  --river-density <n>       How many rivers: number of sources, higher = more\n"
"                            (default: auto ~ (w+h)/400; e.g. 10 sparse, 120 dense).\n"
"  Hydraulic erosion (off by default; carves valleys/drainage before rivers):\n"
"  --erosion                 Enable droplet erosion on the height field.\n"
"  --erosion-density <f>     Droplets = f * width * height (default 0.20).\n"
"  --erosion-lifetime <n>    Max steps per droplet (default 30).\n"
"  --erosion-radius <n>      Erosion brush radius in tiles (default 3).\n"
"  --erosion-z-scale <f>     How strongly eroded relief folds back into z\n"
"                            (1.0 = full carve; default 1.0).\n"
"                            (erosion-erode / erosion-deposition: config file only.)\n"
"  Regions (off by default; Voronoi climate territories -> organic biomes):\n"
"  --regions                 Partition the map into Voronoi biome territories.\n"
"  --region-spacing <n>      Territory size: site spacing in tiles (default 64).\n"
"  --region-jitter <f>       Site jitter within its cell, [0,1] (default 0.6).\n"
"  --wfc                     WFC biome transitions over territories (legal\n"
"                            biome adjacency; implies --regions).\n"
"  --cellular                Cellular-automata organic forest clumps.\n"
"  --cellular-fill <f>       Forest random-fill probability, [0,1] (default 0.42).\n"
"  --cellular-iterations <n> CA smoothing iterations (default 4).\n"
"  Towns (off by default; Poisson sites, MST roads, A* paths, BSP buildings):\n"
"  --towns                   Place towns + roads/bridges + buildings + trails.\n"
"  --town-spacing <n>        Min distance between towns in tiles (default 160).\n"
"  --town-size <n>           Town footprint size in tiles (default 48).\n"
"  --no-trails               Skip the L-system side-trails from towns.\n"
"  Detail/polish:\n"
"  --region-warp <f>         Voronoi border domain-warp amplitude in tiles\n"
"                            (organic borders; default 12, 0 = crisp polygons).\n"
"  --dither                  Stipple biome borders into soft transitions.\n"
"  --dither-strength <f>     Fraction of border cells to swap, [0,1] (default 0.35).\n"
"  --resources               Poisson-disc resource (ore) nodes on hills/foothills.\n"
"  --resource-spacing <n>    Min distance between resource nodes (default 120).\n"
"  --cliffs                  Varied mountain rock tiles + cliff-face rock statics.\n"
"  --terrace                 Flatten land into Britannia-like plateaus (vs noise).\n"
"  --terrace-step <n>        Terrace z quantization step (default 5).\n"
"  --plain-z <n>             Land within +/-n of 0 snaps flat to 0 (default 3).\n"
"  --connect                 Carve passes/bridges so no land is cut off by\n"
"                            mountains or water.\n"
"  --connect-max <n>         Max barrier-crossing cost; higher bridges wider gaps\n"
"                            (default 6000; keeps the open ocean between continents).\n"
"  --pass-width <n>          Half-width of carved passes/causeways; bigger = wider\n"
"                            natural grassy valleys (default 6 = ~12 tiles wide).\n"
"  --pass-slope <n>          Foothill band width grading mountain rock down to\n"
"                            the valley (default 8; 0 = sharp cliff edges).\n"
"  --clearings               Reserve flat, vegetation-free building plots.\n"
"  --clearing-spacing <n>    Min distance between clearings (default 140).\n"
"  --clearing-size <n>       Clearing radius in tiles (default 10).\n"
"  Enrichment (all ON by default; use the --no-* flags to disable):\n"
"  --no-biomes               Disable climate biomes (snow/desert/jungle/swamp).\n"
"  --temperature-bias <f>    Shift climate warmer(+)/colder(-) (default 0).\n"
"  --no-vegetation           Skip tree/rock/plant statics (statics stay empty).\n"
"  --tree-density <f>        Tree fraction of eligible cells, 0..1 (default 0.08).\n"
"  --rock-density <f>        Rock/boulder fraction, 0..1 (default 0.02).\n"
"  --plant-density <f>       Ground-cover fraction, 0..1 (default 0.05).\n"
"  --no-beaches              Skip sloped sand beaches around coasts.\n"
"  --beach-width <int>       Beach band width in tiles (default 4).\n"
"  --no-lakes                Skip lakes at inland river sinks.\n"
"  --no-passes               Skip carving walkable passes through mountains.\n"
"  --tiledata <path>     tiledata.mul used to sanity-check tile flags\n"
"                        (default ./ref/UONewDawn/tiledata.mul; optional).\n"
"  --preview <file.png>  Also render a final top-down preview image.\n"
"  --pass-previews       Write a PNG snapshot after each generation pass.\n"
"  --pass-preview-dir <dir>  Directory for pass PNGs (default: the output dir;\n"
"                            implies --pass-previews).\n"
"  --install-dir <dir>   Copy the final .mul triplet into dir (the directory\n"
"                        UOFiddler/ModernUO loads), overwriting any files there.\n"
"  --dump-config <file>  Write the fully-resolved config (preset + file + flags)\n"
"                        to file; `--config file` then reproduces this map.\n"
"  --emit-mapdef         Also write map-definitions.snippet.json for ModernUO.\n"
"  --terrain-only        Write only map<N>.mul (skip staidx/statics).\n"
"  --help                Show this help and exit.\n"
"  --version             Show version and exit.\n"
"\n"
"Config file (no environment variables are ever used):\n"
"  INI-like 'key = value', '#' or ';' comments. Keys mirror the long options\n"
"  with '-' or '_' (e.g. sea_level = -0.1). 'preset = felucca' is allowed.\n"
"\n"
"Examples:\n"
"  %s --seed 42 --preset test --out ./out --preview out.png --emit-mapdef\n"
"  %s --config config/example.cfg --seed 7\n"
"  %s --seed 1 --preset felucca --out /mnt/c/.../client --map 0\n"
"\n"
"ModernUO note: the generated map's width/height MUST match the entry in\n"
"Data/map-definitions.json for that map. Use --emit-mapdef to get a matching\n"
"snippet, or --preset felucca for a drop-in map0 replacement.\n",
        UOMG_VERSION, argv0, argv0, argv0, argv0);
}

/* Pre-scan argv for --config so the file loads before CLI overrides apply. */
static const char *find_config_arg(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--config") && i + 1 < argc)
            return argv[i + 1];
        if (!strncmp(argv[i], "--config=", 9))
            return argv[i] + 9;
        if (!strcmp(argv[i], "-c") && i + 1 < argc)
            return argv[i + 1];
    }
    return NULL;
}

int main(int argc, char **argv) {
    mapgen_config cfg;
    config_defaults(&cfg);

    /* Phase 1: config file (if any) before CLI, so CLI always wins. */
    const char *cfg_path = find_config_arg(argc, argv);
    if (cfg_path) {
        snprintf(cfg.config_path, sizeof(cfg.config_path), "%s", cfg_path);
        if (config_load_file(&cfg, cfg_path) != 0)
            return 2;
    }

    /* Phase 2: CLI overrides. */
    static const struct option longopts[] = {
        { "seed",        required_argument, 0, 's' },
        { "config",      required_argument, 0, 'c' },
        { "map",         required_argument, 0, 'm' },
        { "width",       required_argument, 0, 'W' },
        { "height",      required_argument, 0, 'H' },
        { "preset",      required_argument, 0, 'p' },
        { "out",         required_argument, 0, 'o' },
        { "sea-level",   required_argument, 0, 'L' },
        { "frequency",   required_argument, 0, 'f' },
        { "octaves",     required_argument, 0, 'O' },
        { "max-slope",   required_argument, 0, 'S' },
        { "land-z-max",  required_argument, 0, 'Z' },
        { "water-z",     required_argument, 0, 'w' },
        { "continent",   no_argument,       0, 'C' },
        { "continent-radius",   required_argument, 0, OPT_CONT_RADIUS },
        { "continent-strength", required_argument, 0, OPT_CONT_STRENGTH },
        { "continent-power",    required_argument, 0, OPT_CONT_POWER },
        { "continents",  no_argument,       0, 'A' },
        { "continent-scale",    required_argument, 0, OPT_CONT_SCALE },
        { "continent-count",    required_argument, 0, OPT_CONT_COUNT },
        { "continent-fill",     required_argument, 0, OPT_CONT_FILL },
        { "mountains",   no_argument,       0, OPT_MOUNTAINS },
        { "mountain-level",     required_argument, 0, OPT_MTN_LEVEL },
        { "mountain-z",         required_argument, 0, OPT_MTN_Z },
        { "mountain-scale",     required_argument, 0, OPT_MTN_SCALE },
        { "flat",        no_argument,       0, OPT_FLAT },
        { "flat-z",      required_argument, 0, OPT_FLAT_Z },
        { "rivers",      no_argument,       0, OPT_RIVERS },
        { "river-density",      required_argument, 0, OPT_RIVER_DENSITY },
        { "no-biomes",   no_argument,       0, OPT_NO_BIOMES },
        { "temperature-bias",   required_argument, 0, OPT_TEMP_BIAS },
        { "no-vegetation", no_argument,     0, OPT_NO_VEG },
        { "tree-density",       required_argument, 0, OPT_TREE_DEN },
        { "rock-density",       required_argument, 0, OPT_ROCK_DEN },
        { "plant-density",      required_argument, 0, OPT_PLANT_DEN },
        { "no-beaches",  no_argument,       0, OPT_NO_BEACHES },
        { "beach-width",        required_argument, 0, OPT_BEACH_WIDTH },
        { "no-lakes",    no_argument,       0, OPT_NO_LAKES },
        { "no-passes",   no_argument,       0, OPT_NO_PASSES },
        { "erosion",           no_argument,       0, OPT_EROSION },
        { "erosion-density",   required_argument, 0, OPT_EROSION_DENSITY },
        { "erosion-lifetime",  required_argument, 0, OPT_EROSION_LIFETIME },
        { "erosion-radius",    required_argument, 0, OPT_EROSION_RADIUS },
        { "erosion-z-scale",   required_argument, 0, OPT_EROSION_ZSCALE },
        { "regions",           no_argument,       0, OPT_REGIONS },
        { "region-spacing",    required_argument, 0, OPT_REGION_SPACING },
        { "region-jitter",     required_argument, 0, OPT_REGION_JITTER },
        { "wfc",               no_argument,       0, OPT_WFC },
        { "cellular",          no_argument,       0, OPT_CELLULAR },
        { "cellular-fill",     required_argument, 0, OPT_CELLULAR_FILL },
        { "cellular-iterations", required_argument, 0, OPT_CELLULAR_ITERS },
        { "towns",             no_argument,       0, OPT_TOWNS },
        { "town-spacing",      required_argument, 0, OPT_TOWN_SPACING },
        { "town-size",         required_argument, 0, OPT_TOWN_SIZE },
        { "no-trails",         no_argument,       0, OPT_NO_TRAILS },
        { "region-warp",       required_argument, 0, OPT_REGION_WARP },
        { "dither",            no_argument,       0, OPT_DITHER },
        { "dither-strength",   required_argument, 0, OPT_DITHER_STRENGTH },
        { "resources",         no_argument,       0, OPT_RESOURCES },
        { "resource-spacing",  required_argument, 0, OPT_RESOURCE_SPACING },
        { "cliffs",            no_argument,       0, OPT_CLIFFS },
        { "terrace",           no_argument,       0, OPT_TERRACE },
        { "terrace-step",      required_argument, 0, OPT_TERRACE_STEP },
        { "plain-z",           required_argument, 0, OPT_PLAIN_Z },
        { "connect",           no_argument,       0, OPT_CONNECT },
        { "connect-max",       required_argument, 0, OPT_CONNECT_MAX },
        { "pass-width",        required_argument, 0, OPT_PASS_WIDTH },
        { "pass-slope",        required_argument, 0, OPT_PASS_SLOPE },
        { "clearings",         no_argument,       0, OPT_CLEARINGS },
        { "clearing-spacing",  required_argument, 0, OPT_CLEARING_SPACING },
        { "clearing-size",     required_argument, 0, OPT_CLEARING_SIZE },
        { "tiledata",    required_argument, 0, 'T' },
        { "preview",     required_argument, 0, 'P' },
        { "pass-previews",    no_argument,       0, OPT_PASS_PREVIEWS },
        { "pass-preview-dir", required_argument, 0, OPT_PASS_PREVIEW_DIR },
        { "install-dir",      required_argument, 0, OPT_INSTALL_DIR },
        { "dump-config",      required_argument, 0, OPT_DUMP_CONFIG },
        { "emit-mapdef", no_argument,       0, 'M' },
        { "terrain-only",no_argument,       0, 't' },
        { "help",        no_argument,       0, 'h' },
        { "version",     no_argument,       0, 'v' },
        { 0, 0, 0, 0 }
    };
    const char *optstr = "s:c:m:W:H:p:o:L:f:O:S:Z:w:CAT:P:Mthv";

    int c, idx;
    while ((c = getopt_long(argc, argv, optstr, longopts, &idx)) != -1) {
        switch (c) {
            case 's': cfg.seed = strtoull(optarg, NULL, 0); break;
            case 'c': break; /* already handled in phase 1 */
            case 'm': cfg.map_index = (int)strtol(optarg, NULL, 0); break;
            case 'W': cfg.width = (int)strtol(optarg, NULL, 0); break;
            case 'H': cfg.height = (int)strtol(optarg, NULL, 0); break;
            case 'p': if (config_apply_preset(&cfg, optarg) != 0) return 2; break;
            case 'o': snprintf(cfg.out_dir, sizeof(cfg.out_dir), "%s", optarg); break;
            case 'L': cfg.sea_level = strtod(optarg, NULL); break;
            case 'f': cfg.frequency = strtod(optarg, NULL); break;
            case 'O': cfg.octaves = (int)strtol(optarg, NULL, 0); break;
            case 'S': cfg.max_slope = (int)strtol(optarg, NULL, 0); break;
            case 'Z': cfg.land_z_max = (int)strtol(optarg, NULL, 0); break;
            case 'w': cfg.water_z = (int)strtol(optarg, NULL, 0); break;
            case 'C': cfg.continent = 1; break;
            case OPT_CONT_RADIUS:   cfg.continent = 1; cfg.continent_radius = strtod(optarg, NULL); break;
            case OPT_CONT_STRENGTH: cfg.continent = 1; cfg.continent_strength = strtod(optarg, NULL); break;
            case OPT_CONT_POWER:    cfg.continent = 1; cfg.continent_power = strtod(optarg, NULL); break;
            case 'A': cfg.continents = 1; break;
            case OPT_CONT_SCALE:    cfg.continents = 1; cfg.continent_scale = strtod(optarg, NULL); break;
            case OPT_CONT_COUNT:    cfg.continents = 1; cfg.continent_count = (int)strtol(optarg, NULL, 0); break;
            case OPT_CONT_FILL:     cfg.continents = 1; cfg.continent_fill = strtod(optarg, NULL); break;
            case OPT_MOUNTAINS:     cfg.mountains = 1; break;
            case OPT_MTN_LEVEL:     cfg.mountains = 1; cfg.mountain_level = strtod(optarg, NULL); break;
            case OPT_MTN_Z:         cfg.mountains = 1; cfg.mountain_z = (int)strtol(optarg, NULL, 0); break;
            case OPT_MTN_SCALE:     cfg.mountains = 1; cfg.mountain_scale = strtod(optarg, NULL); break;
            case OPT_FLAT:          cfg.flat = 1; break;
            case OPT_FLAT_Z:        cfg.flat = 1; cfg.flat_z = (int)strtol(optarg, NULL, 0); break;
            case OPT_RIVERS:        cfg.rivers = 1; break;
            case OPT_RIVER_DENSITY: cfg.rivers = 1; cfg.river_density = (int)strtol(optarg, NULL, 0); break;
            case OPT_NO_BIOMES:     cfg.biomes = 0; break;
            case OPT_TEMP_BIAS:     cfg.temperature_bias = strtod(optarg, NULL); break;
            case OPT_NO_VEG:        cfg.vegetation = 0; break;
            case OPT_TREE_DEN:      cfg.tree_density = strtod(optarg, NULL); break;
            case OPT_ROCK_DEN:      cfg.rock_density = strtod(optarg, NULL); break;
            case OPT_PLANT_DEN:     cfg.plant_density = strtod(optarg, NULL); break;
            case OPT_NO_BEACHES:    cfg.beaches = 0; break;
            case OPT_BEACH_WIDTH:   cfg.beach_width = (int)strtol(optarg, NULL, 0); break;
            case OPT_NO_LAKES:      cfg.lakes = 0; break;
            case OPT_NO_PASSES:     cfg.passes = 0; break;
            case OPT_EROSION:          cfg.erosion = 1; break;
            case OPT_EROSION_DENSITY:  cfg.erosion = 1; cfg.erosion_density = strtod(optarg, NULL); break;
            case OPT_EROSION_LIFETIME: cfg.erosion = 1; cfg.erosion_lifetime = (int)strtol(optarg, NULL, 0); break;
            case OPT_EROSION_RADIUS:   cfg.erosion = 1; cfg.erosion_radius = (int)strtol(optarg, NULL, 0); break;
            case OPT_EROSION_ZSCALE:   cfg.erosion = 1; cfg.erosion_z_scale = strtod(optarg, NULL); break;
            case OPT_REGIONS:        cfg.regions = 1; break;
            case OPT_REGION_SPACING: cfg.regions = 1; cfg.region_spacing = (int)strtol(optarg, NULL, 0); break;
            case OPT_REGION_JITTER:  cfg.regions = 1; cfg.region_jitter = strtod(optarg, NULL); break;
            case OPT_WFC:            cfg.wfc = 1; cfg.regions = 1; break;
            case OPT_CELLULAR:       cfg.cellular = 1; break;
            case OPT_CELLULAR_FILL:  cfg.cellular = 1; cfg.cellular_fill = strtod(optarg, NULL); break;
            case OPT_CELLULAR_ITERS: cfg.cellular = 1; cfg.cellular_iterations = (int)strtol(optarg, NULL, 0); break;
            case OPT_TOWNS:        cfg.towns = 1; break;
            case OPT_TOWN_SPACING: cfg.towns = 1; cfg.town_spacing = (int)strtol(optarg, NULL, 0); break;
            case OPT_TOWN_SIZE:    cfg.towns = 1; cfg.town_size = (int)strtol(optarg, NULL, 0); break;
            case OPT_NO_TRAILS:    cfg.trails = 0; break;
            case OPT_REGION_WARP:  cfg.region_warp = strtod(optarg, NULL); break;
            case OPT_DITHER:       cfg.dither = 1; break;
            case OPT_DITHER_STRENGTH: cfg.dither = 1; cfg.dither_strength = strtod(optarg, NULL); break;
            case OPT_RESOURCES:    cfg.resources = 1; break;
            case OPT_RESOURCE_SPACING: cfg.resources = 1; cfg.resource_spacing = (int)strtol(optarg, NULL, 0); break;
            case OPT_CLIFFS:       cfg.cliffs = 1; break;
            case OPT_TERRACE:      cfg.terrace = 1; break;
            case OPT_TERRACE_STEP: cfg.terrace = 1; cfg.terrace_step = (int)strtol(optarg, NULL, 0); break;
            case OPT_PLAIN_Z:      cfg.terrace = 1; cfg.plain_z = (int)strtol(optarg, NULL, 0); break;
            case OPT_CONNECT:      cfg.connect = 1; break;
            case OPT_CONNECT_MAX:  cfg.connect = 1; cfg.connect_max = (int)strtol(optarg, NULL, 0); break;
            case OPT_PASS_WIDTH:   cfg.connect = 1; cfg.pass_width = (int)strtol(optarg, NULL, 0); break;
            case OPT_PASS_SLOPE:   cfg.connect = 1; cfg.pass_slope = (int)strtol(optarg, NULL, 0); break;
            case OPT_CLEARINGS:    cfg.clearings = 1; break;
            case OPT_CLEARING_SPACING: cfg.clearings = 1; cfg.clearing_spacing = (int)strtol(optarg, NULL, 0); break;
            case OPT_CLEARING_SIZE:    cfg.clearings = 1; cfg.clearing_size = (int)strtol(optarg, NULL, 0); break;
            case 'T': snprintf(cfg.tiledata_path, sizeof(cfg.tiledata_path), "%s", optarg); break;
            case 'P': snprintf(cfg.preview_path, sizeof(cfg.preview_path), "%s", optarg); break;
            case OPT_PASS_PREVIEWS: cfg.emit_pass_previews = 1; break;
            case OPT_PASS_PREVIEW_DIR:
                cfg.emit_pass_previews = 1;
                snprintf(cfg.pass_preview_dir, sizeof(cfg.pass_preview_dir), "%s", optarg);
                break;
            case OPT_INSTALL_DIR:
                snprintf(cfg.install_dir, sizeof(cfg.install_dir), "%s", optarg);
                break;
            case OPT_DUMP_CONFIG:
                snprintf(cfg.dump_config_path, sizeof(cfg.dump_config_path), "%s", optarg);
                break;
            case 'M': cfg.emit_mapdef = 1; break;
            case 't': cfg.terrain_only = 1; break;
            case 'h': print_help(argv[0]); return 0;
            case 'v': print_version(); return 0;
            default:  fprintf(stderr, "Try '%s --help'.\n", argv[0]); return 2;
        }
    }

    if (config_validate(&cfg) != 0)
        return 2;

    if (io_ensure_dir(cfg.out_dir) != 0) {
        fprintf(stderr, "error: cannot create output directory '%s'\n", cfg.out_dir);
        return 1;
    }

    /* Write the fully-resolved config if requested (after the out dir exists, so
     * --dump-config ./out/world.cfg works; reflects preset + file + CLI). */
    if (config_dump(&cfg, cfg.dump_config_path) != 0)
        return 1;

    const int BW = cfg.width >> 3, BH = cfg.height >> 3;
    printf("uomappp %s: seed=%llu map=%d size=%dx%d (%dx%d blocks) out=%s\n",
           UOMG_VERSION, (unsigned long long)cfg.seed, cfg.map_index,
           cfg.width, cfg.height, BW, BH, cfg.out_dir);

    return pipeline_run(&cfg);
}
