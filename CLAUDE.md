# CLAUDE.md — UOMapPlusPlus (uomappp)

Guidance for Claude Code (and humans) working in this repo.

## What this is

**UOMapPlusPlus** (binary `uomappp`) is a procedural **Ultima Online** map
generator that writes classic `.mul` map data (terrain `mapN.mul` +
`staidxN.mul` + `staticsN.mul`) for a **ModernUO** shard. It is a fork of
`uomapgen` that keeps the original POSIX-C / CMake / FastNoiseLite stack and its
hard determinism contract, and expands the single terrain pass set into a
**toolbox of generation passes** — each backed by the algorithm best suited to
its job (noise, hydraulic erosion, Voronoi/Delaunay, Wave Function Collapse,
A\*, minimum spanning trees, cellular automata, BSP, Poisson-disc sampling,
marching squares, L-systems, libtcod).

It uses [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) (OpenSimplex2)
for its base fields and already generates **organic continents** (noise-shaped
coastlines) with **climate biomes** (snow/desert/jungle/swamp/forest/grass),
**rivers** (meandering, fords, lakes), **mountains** (ridged, with walkable
passes), **beaches**, and **vegetation statics** (trees/rocks/plants).

The architecture is **one binary with internal passes** run in a fixed order by
a pipeline orchestrator; each pass emits a `passNN_<name>.png` so the map can be
watched as it evolves, and the final `.mul` triplet is written once at the end
and swapped into the directory UOFiddler/ModernUO loads from. The expansion
roadmap (erosion, Voronoi biomes, WFC transitions, cellular caves/swamps, towns
via Poisson+MST, A\*/L-system roads, BSP dungeons/interiors, Poisson resources)
is in the **Roadmap** section below.

- **Language:** POSIX C (C11), Linux only.
- **Build system:** CMake.
- **Determinism:** output is **byte-identical for a given build + seed/config**.
- **No environment variables** are ever read — configuration is CLI flags and
  an optional config file only.

## Build & run

Always build single-core (`-j1`) — this is a hard project requirement:

```sh
cmake -S . -B build
cmake --build build -j1
./build/uomappp --help
```

Example run (small test map + preview + ModernUO snippet):

```sh
./build/uomappp --seed 42 --preset test --out ./out --preview ./out/preview.png --emit-mapdef
```

## Repository layout

```
CMakeLists.txt            top-level build (pins -j1 in docs, determinism flags)
include/uomappp/*.h       public headers, one per module
src/*.c                   implementation (see modules below)
third_party/
  FastNoiseLite/          vendored C single-header noise lib
  stb/                    vendored stb_image_write.h (PNG preview)
config/example.cfg        annotated sample config
docs/FORMAT.md            exact .mul byte layouts
docs/DETERMINISM.md       seeding + reproducibility rules
ref/UONewDawn/            reference UO client assets (NOT committed; see below)
out/                      generated output (NOT committed)
```

### Modules (`src/` + matching `include/uomappp/*.h`)

- `pipeline` — the generation **harness**: `pipeline_run(cfg)` drives the fixed
               pass order (tiledata → `terrain_generate` → `mapwriter` →
               `statics` → final preview → `mapdef` → `install`). `main.c` only
               parses config and calls this. New library passes slot in here.
- `config`   — defaults, config-file parse, presets, validation. No env vars.
               Pipeline keys: `pass_previews`, `pass_preview_dir`, `install_dir`.
               Presets: `test`, `felucca`, and **`britannia`** (felucca size with
               values calibrated to the real Felucca `map0` — ~50% water, flat
               terraced plains + mountain ranges + Voronoi biome regions, so it
               looks like Britannia not noise). The `terrace` pass (flatten land
               into plateaus; keys `terrace`/`terrace_step`/`plain_z`) is the key
               calibration lever.
- `io`       — explicit little-endian byte writers + path/dir helpers + a
               byte-for-byte `io_copy_file`. **Never `fwrite` raw structs**
               (would leak host endianness/padding).
- `noise`    — FastNoiseLite wrapper; the ONE unit that defines `FNL_IMPL`.
               splitmix64-derived per-layer seeds from the master seed (salts in
               `noise.h`, incl. elevation/moisture/continent/temperature/meander/
               biome/veg/**erosion**). Also `noise_layer_create_ridged` (mountains).
- `field`    — float-grid utilities (the SciPy/NumPy algorithms reimplemented in
               C, no Python): chamfer **distance transform**, separable **blur**,
               normalize. Pure/deterministic; shared by erosion and later passes.
- `erosion`  — **hydraulic (droplet) erosion** pass (off by default). Carves
               valleys/drainage into the relief (`z`) and routing height
               (`hfield`) BEFORE rivers, so rivers follow the eroded drainage.
               Droplets spawn from a splitmix64 stream keyed by
               `NOISE_LAYER_EROSION`; mountains keep their peaks.
- `voronoi`  — raster (jittered-grid) **Voronoi territories**: sites + per-cell
               nearest-site `region` label, with an optional OpenSimplex
               **boundary domain-warp** (`region_warp`) so borders read organic,
               not polygonal. Salts `NOISE_LAYER_VORONOI` / `NOISE_LAYER_WARP`.
               (We compute the partition directly on the raster instead of
               vendoring a polygon Voronoi lib.)
- `marching` — **marching-squares** boundary extraction: marks region/biome
               border cells (the raster 2×2 crossing test) into `g->flags`.
- `mst`      — **minimum spanning tree** (Kruskal + union-find) over the
               territory graph: the town-connectivity graph roads will route.
               The `regions` terrain pass (off by default) ties these together:
               fills `g->region`, gives each territory a climate so biomes become
               organic Voronoi patches, flags borders, and builds the MST.
- `cellular`  — **cellular-automata** primitives (random fill + birth/survival
               smoothing). The `cellular` pass grows organic **forest clumps**;
               reusable for swamps/caves/ruins. Salt `NOISE_LAYER_CELLULAR`.
- `wfc`      — **Wave Function Collapse** over a graph (min-entropy + weighted
               collapse + constraint propagation, no backtracking). The `wfc`
               pass (implies `regions`) assigns each territory a biome so
               neighbours meet only along **legal transitions** (grass is the
               universal glue). Salt `NOISE_LAYER_WFC`; falls back to climate
               biomes on contradiction.
- `poisson`  — **Poisson-disc sampling** (Bridson): blue-noise points with a
               minimum spacing, for town sites. Salt `NOISE_LAYER_POISSON`.
- `astar`    — **A\*** on an 8-connected cost grid (binary heap, `(f,seq)`
               tie-break), for routing roads. No RNG.
- `bsp`      — **BSP** recursive rectangle partition, for town building
               footprints / dungeon rooms. Salt `NOISE_LAYER_BSP`.
- `lsystem`  — **L-system** branching turtle (F→F[+F][-F]-style), for organic
               side-trails. Salt `NOISE_LAYER_LSYSTEM`.
               The `towns` terrain pass (off by default, runs last) ties these
               together: Poisson town sites on buildable land → MST → A* roads
               (bridging rivers) → BSP buildings + street grid → L-system trails.
               Roads/floors are terrain tiles (TCAT_ROAD/BRIDGE/FLOOR); building
               **walls and doors are real statics** (authentic UO stone-wall
               `0x0057`-`0x0059` + door `0x06A5`) appended to `g->statics`.
               `cliffs` pass: mountains get varied authentic rock land tiles +
               loose rock statics at sharp faces.
- `tiledata` — reads the land section of `tiledata.mul` (High Seas format) to
               sanity-check palette tile flags (Wet / Impassable). Optional.
- `biome`    — climate-band biome classification (temperature-by-latitude +
               moisture + elevation) and the per-biome **land-tile palette**
               (varied by a per-cell hash). `biome.c` is where land tiles live.
- `terrain`  — the terrain passes: elevation/continent fields → land/water →
               biomes, then passes for mountains, **rivers** (meander, fords,
               lakes), **beaches** (sloped coasts), **mountain passes**, and
               slope-limit. Continent shaping = elevation noise − radial falloff.
               Also the **connect** pass (`--connect`: multi-source Dijkstra + MST
               link disconnected within-continent land, carving meandering grass
               passes `--pass-width`/`--pass-slope` through mountains or sand
               causeways over inland water; salt `0x17`), the **foothills** pass
               (`--mountain-slope`: BFS-grade every mountain perimeter into sloped
               foothills, no RNG), and the **clearings** pass (`--clearings`:
               reserve flat vegetation-free building plots; salt `0x16`). All off
               by default (byte-identical when off).
               Owns `terrain_grid`, the shared pass state: final `id/z/cat` plus
               reusable float layers (`hfield` populated now; `moisture`,
               `temperature`, `region`, `flags` reserved/NULL until a later pass
               allocates them). Emits a per-pass PNG via `preview_pass` after
               each stage.
- `vegetation`— deterministic per-cell static placement by biome (trees, cacti,
               reeds, boulders, plants); the curated static-ID sets live here.
- `mapwriter`— writes `mapN.mul` in the client's column-major block layout.
- `statics`  — writes `staidxN.mul` + `staticsN.mul`: per block it merges the
               per-cell vegetation records with the grid's world-space statics
               (town **building walls/doors** + **cliff rocks**, in `g->statics`),
               sorts canonically, and writes with correct index offsets. Empty
               blocks → `(-1,-1,-1)`; lengths stay multiples of 7.
- `preview`  — top-down PNG via stb (per-biome colors). `preview_write_png` for
               a single image; `preview_pass` writes numbered `pass<NN>_<name>.png`
               snapshots (diagnostic only — never affects `.mul` bytes).
- `mapdef`   — emits `map-definitions.snippet.json` for ModernUO.
- `install`  — final step: copies the `.mul` triplet from `out_dir` into
               `install_dir` (the dir UOFiddler/ModernUO loads). No-op if unset.
- `housing`  — post-generation **buildability summary** printed to stdout (never
               affects `.mul` bytes): counts water / impassable rock / static-
               blocked / buildable land, and how many houses of each UO footprint
               (small 7×7 … castle 31×31) fit on flat, clear, non-overlapping
               ground. Uses `vegetation_blocks()` so only true obstacles
               (trees/rocks) count, not passable ground cover.

## UO `.mul` byte format (what the writers must honor)

Full detail in `docs/FORMAT.md`. The essentials, confirmed against the
reference client and ModernUO's `Projects/Server/TileMatrix/TileMatrix.cs`:

- **`mapN.mul`** — blocks of 8×8 cells, **196 bytes** each:
  - 4-byte block header (server ignores it; we write `0`).
  - 64 cells, each `int16 tileID` (LE) + `int8 z`.
  - **Block order is column-major:** `blockIndex = blockX * BlockHeight + blockY`
    where `BlockWidth = width>>3`, `BlockHeight = height>>3`.
  - **Cell order in a block:** `cellIndex = ((y & 7) << 3) + (x & 7)`.
- **`staidxN.mul`** — one 12-byte record per block (same column-major order):
  `int32 lookup, int32 length, int32 extra`. Empty block = `(-1,-1,-1)`;
  `length` must be a multiple of 7.
- **`staticsN.mul`** — 7-byte records grouped per block:
  `uint16 id, uint8 x(0..7), uint8 y(0..7), int8 z, int16 hue` (all LE).
- **`tiledata.mul`** here is **High Seas format** (3,188,736 bytes): land entry
  = 8-byte flags + `int16 texID` + 20-byte name = 30 bytes; 512 groups of 32.
- ModernUO prefers `.mul`; it only falls back to `mapNLegacyMUL.uop` if
  `mapN.mul` is absent. **We always write `.mul`.** `radarcol.mul` is unused by
  the server and not generated.

## ModernUO integration

- Server source lives at
  `/mnt/c/Users/brian/bin/uoMaps/ModernUO-Main/ModernUO-main`.
- Map dimensions come from `Distribution/Data/map-definitions.json`. The
  generated map's `width`/`height` **must match** the entry for that map, or the
  server computes wrong block offsets. Use `--emit-mapdef` to get a matching
  snippet, or `--preset felucca` (7168×4096) for a drop-in `map0` replacement.
- Drop the generated `mapN.mul` / `staidxN.mul` / `staticsN.mul` into a
  directory listed in the server's `dataDirectories` config.

## Reference assets

`ref/UONewDawn/` is a full UO client (Stygian Abyss era, ClassicUO 7.0.15.1,
HS-format tiledata). It is **large and copyrighted — never commit it** (it is
git-ignored). Treat `ref/UONewDawn/map0.mul` as the layout oracle and
`tiledata.mul` as the tile-flag source.

## Conventions / gotchas

- Keep all on-disk writes going through `io_write_*le` helpers.
- Keep `FNL_IMPL` defined in exactly one `.c` file (`noise.c`).
- Anything affecting output bytes for a given seed (noise params, the per-biome
  palettes in `biome.c`, the vegetation sets/densities in `vegetation.c`,
  iteration order, the per-layer salts in `noise.h`) is part of the determinism
  contract — changing it changes everyone's maps. Call it out in commits.
- Determinism now covers `map0.mul`, `staidx0.mul` **and** `statics0.mul`
  (vegetation is hashed from `(seed,x,y)`); verify all three with `sha256sum`.
- Build must stay warning-clean with `-Wall -Wextra -Wshadow` (the FastNoiseLite
  3D-cellular warning is suppressed only for `noise.c`).

## Roadmap

Inherited from `uomapgen`:

1. ✅ Terrain: organic continents, oceans, navigable land → valid `.mul` triplet.
2. ✅ Enrichment: climate biomes, rivers (fords/lakes), mountains (passes),
   beaches, vegetation statics.

UOMapPlusPlus expansion — each library is an internal **pass** (uniform
`pass_<name>(terrain_grid *, const mapgen_config *)` signature), guarded by a
`cfg` toggle, drawing any RNG from `noise_derive_seed(cfg->seed,
NOISE_LAYER_<NAME>)` (new salts **appended** to `noise.h`, never reordered), run
in a fixed order by the pipeline orchestrator, each emitting a per-pass PNG:

0. ✅ Fork hygiene: rename `uomapgen` → `uomappp`, `include/uomappp/`,
   `project(UOMapPlusPlus)`; public repo `xor-droid/UOMapPlusPlus`.
3. ✅ **Phase 1 — Framework:** shared float layers on `terrain_grid`
   (`hfield` live; `moisture`/`temperature`/`region`/`flags` reserved);
   `pipeline_run` harness; per-pass PNG (`preview_pass`); config/CLI for
   previews + `install` resource-swap into the UOFiddler data dir. Existing
   terrain refactored behind the pipeline with **byte-identical output
   preserved** (sha256 gate, default + continents/mountains/rivers configs).
4. ✅ **Phase 2 — Terrain realism:** hydraulic erosion (droplet, `erosion`
   module, salt `0x09`) + distance-transform/blur utilities (`field` module,
   SciPy/NumPy reimplemented in C); erosion runs before rivers so drainage feeds
   them. Gated off by default → existing maps byte-identical; deterministic when
   on (sha256 gate, two runs).
5. ✅ **Phase 3 — Regions:** Voronoi territories (`voronoi`, salt `0x0A`) →
   organic climate biomes; marching-squares borders (`marching`) → `g->flags`;
   MST territory graph (`mst`). Gated by `regions` (off by default →
   byte-identical; deterministic when on). Allocates `g->region` + `g->flags`.
6. ✅ **Phase 4 — Transitions & organic regions:** WFC biome transitions
   (`wfc` module, salt `0x0C`; implies regions, legal biome adjacency) + cellular
   automata forest clumps (`cellular` module, salt `0x0B`). Both off by default
   → byte-identical; deterministic when on (full-stack sha256 gate). (Swamps/
   caves/ruins reuse the same `cellular`/`wfc` modules in later work.)
7. ✅ **Phase 5 — Civilization:** towns (`poisson` sites, salt `0x0D`) + MST +
   `astar` roads/bridges + `bsp` building footprints (salt `0x0F`) + `lsystem`
   trails (salt `0x0E`), via the `towns` pass. Off by default → byte-identical;
   deterministic when on. Rendered as terrain tiles; **follow-up:** render
   building walls as real statics (extend `statics.c`/`vegetation.c` record
   pipeline).
8. ✅ **Phase 6 — Detail:** Voronoi **boundary noise-warp** (`region_warp`, salt
   `0x11`, on by default when regions is used); **biome-border dithering**
   (`dither` pass, salt `0x13`); **Poisson-disc resource nodes** (`resources`
   pass, salt `0x12`); verified full pipeline + final PNG + install swap.
9. ✅ **Statics follow-ups:** town building **walls/doors emitted as real UO
   statics** (grid world-statics merged into `staticsN.mul`); **cliff-face
   mountains** (`cliffs` pass, salt `0x14`) — varied authentic rock land tiles +
   rock statics at sharp faces. Static item ids verified by name+flags against
   the reference `tiledata.mul`.

### Library → pass → seed map

| Technology | UO application | New salt in `noise.h` |
|---|---|---|
| FastNoiseLite / OpenSimplex2 | continents, elevation, moisture, temperature | `0x01..0x08` (frozen) |
| SciPy/NumPy (reimpl. in C) | heightmaps, masks, distance transforms, erosion | — / erosion salt |
| Hydraulic erosion | mountains/valleys/drainage | `NOISE_LAYER_EROSION` |
| Voronoi/Delaunay | biomes, territories, town placement | `NOISE_LAYER_VORONOI` |
| Wave Function Collapse | tile transitions, buildings, ruins, towns | `NOISE_LAYER_WFC` |
| Cellular automata | caves, swamps, forests, ruins | `NOISE_LAYER_CA` |
| Poisson-disc sampling | trees, rocks, ruins, resource placement | `NOISE_LAYER_POISSON` |
| BSP | dungeons / building interiors | `NOISE_LAYER_BSP` |
| L-systems | roads, vegetation, settlements | `NOISE_LAYER_LSYSTEM` |
| A\* / MST / marching squares | roads/trails/bridges; connecting towns; coastlines | none (fixed tie-break) |
| libtcod | BSP / A\* / FOV helpers | per-use (seed its RNG from a derived seed) |

**Non-C libraries** (SciPy/NumPy = Python; WFC/OpenSimplex2/Voronoi reference
impls = Java/C#) are handled by reimplementing the specific algorithms in POSIX C
— **no Python/Java at runtime** — vendoring existing C ports where they exist
(libtcod, a C WFC, a C Voronoi such as `jc_voronoi`). Each vendored library keeps
its source in its own `third_party/<lib>/` subdir; compiled artifacts land in
`build/` (normal CMake). Note FastNoiseLite already *is* OpenSimplex2, so that row
is "keep/confirm," not a new dependency.
