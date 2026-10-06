/*
 * erosion.h - hydraulic (droplet) erosion pass.
 *
 * Simulates rainfall droplets flowing downhill over the grid's continuous
 * height field, eroding steep slopes and depositing sediment in flats, which
 * carves dendritic valleys and drainage lines. This runs BEFORE the river pass
 * so rivers follow the eroded drainage.
 *
 * Determinism: droplet start positions come from a splitmix64 stream keyed by
 * noise_derive_seed-style mixing of (seed, NOISE_LAYER_EROSION); droplets are
 * processed in index order. Same build + seed + config => identical result.
 *
 * Modifies g->hfield on all land (river-routing height) and g->z on
 * non-mountain land (relief). Mountains keep their height. On allocation
 * failure the grid is left unchanged. Returns 0 on success, -1 on error.
 */
#ifndef UOMAPGEN_EROSION_H
#define UOMAPGEN_EROSION_H

#include "uomappp/terrain.h"
#include "uomappp/config.h"

int erosion_apply(terrain_grid *g, const mapgen_config *cfg);

#endif /* UOMAPGEN_EROSION_H */
