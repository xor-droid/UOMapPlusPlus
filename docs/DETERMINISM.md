# Determinism & reproducibility

**Goal:** the generated `.mul` files are **byte-identical for a given build and
a given seed/config**. A map is fully described by its seed and configuration.

## How it's achieved

1. **Single master seed.** `--seed` (a `uint64`) is the only entropy source.
2. **Derived per-layer seeds.** Each noise layer (elevation, moisture, …) and
   each stochastic library pass gets a 32-bit seed via `splitmix64` with a fixed
   per-layer salt (`noise.h`, `NOISE_LAYER_*`). The salts and mixing are frozen
   as part of the contract, and **new salts are only ever appended** (never
   reordered or reused), so adding a pass cannot change existing maps. Salts so
   far: elevation/moisture/detail/continent/meander/temperature/biome/veg
   (`0x01`–`0x08`), `NOISE_LAYER_EROSION` (`0x09`, hydraulic-erosion droplet
   spawns), `NOISE_LAYER_VORONOI` (`0x0A`, Voronoi site jitter + per-region
   climate), `NOISE_LAYER_CELLULAR` (`0x0B`, cellular-automata random fill) and
   `NOISE_LAYER_WFC` (`0x0C`, WFC collapse choices), `NOISE_LAYER_POISSON`
   (`0x0D`, Poisson-disc town sites), `NOISE_LAYER_LSYSTEM` (`0x0E`, L-system
   trail branching), `NOISE_LAYER_BSP` (`0x0F`, BSP splits), `NOISE_LAYER_TOWN`
   (`0x10`), `NOISE_LAYER_WARP` (`0x11`, Voronoi boundary domain-warp; y uses
   `+0x1000`), `NOISE_LAYER_RESOURCE` (`0x12`, Poisson resource nodes) and
   `NOISE_LAYER_DITHER` (`0x13`, biome-border dithering) and `NOISE_LAYER_CLIFF`
   (`0x14`, mountain rock-tile variation + cliff faces). The erosion/regions/
   wfc/cellular/towns/dither/resources/cliffs passes are all off by default; with
   them off, output is byte-identical to the pre-fork builds. A* and MST use no
   RNG (fixed tie-breaks). Town building walls and cliff rocks are emitted as
   real statics, merged into `staticsN.mul` in canonical per-block order.
3. **No nondeterministic inputs.** No `rand()`, no `time()`, no threads, and
   **no environment variables**. Configuration is CLI flags + optional file only.
4. **Fixed iteration order.** Terrain generation and the slope-limiting passes
   run in a fixed raster/reverse-raster order; file writers emit blocks and
   cells in the exact on-disk order.
5. **Explicit little-endian writes.** All multi-byte fields go through
   `io_write_*le`. We never `fwrite` raw structs, so host endianness and struct
   padding cannot leak into the output.
6. **Pinned FP behavior.** The build disables FP contraction
   (`-ffp-contract=off`) so FMA availability / optimization level does not change
   results within a build target.

## Scope of the guarantee

Byte-identical output is guaranteed for the **same build target** — same
compiler, architecture, and flags. This is the intended "per-build" guarantee.

**Cross-architecture bit-exactness is out of scope.** FastNoiseLite produces
`float` results, and floating-point rounding can differ across CPUs/compilers.
If you need identical bytes across different machines, build with one pinned
toolchain (e.g. ship a container/toolchain file) and treat that as *the* build.

## Verifying

```sh
./build/uomappp --seed 42 --preset test --out ./a
./build/uomappp --seed 42 --preset test --out ./b
sha256sum a/map0.mul b/map0.mul      # hashes must match

./build/uomappp --seed 99 --preset test --out ./c
sha256sum c/map0.mul                 # must differ from seed 42
```

## What changes output

Anything below alters the bytes for a given seed; changing it is a
"breaking" change for reproducibility and should be called out in commits:

- noise parameters (`frequency`, `octaves`, lacunarity/gain in `noise.c`),
- the per-layer salts in `noise.h`,
- the tile palette or category thresholds in `terrain.c`,
- slope-limiting logic / order,
- the FastNoiseLite version under `third_party/`.
