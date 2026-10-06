/*
 * marching.h - marching-squares boundary extraction on a labelled grid.
 *
 * On a tile raster the marching-squares case test (is the 2x2 neighbourhood
 * uniform?) reduces to "does this cell differ from a 4-neighbour?". We use it to
 * trace the borders between regions (or biomes, or land/water): the cells where
 * a contour line passes. Deterministic, no RNG. The border mask feeds the
 * region adjacency graph now and tile-transition/dithering passes later.
 */
#ifndef UOMAPGEN_MARCHING_H
#define UOMAPGEN_MARCHING_H

#include <stdint.h>

/* Mark cells on a label boundary (any 4-neighbour has a different label) as 1,
 * others 0, into out (W*H). Returns the number of border cells. */
long marching_squares_borders(const int32_t *label, uint8_t *out, int W, int H);

#endif /* UOMAPGEN_MARCHING_H */
