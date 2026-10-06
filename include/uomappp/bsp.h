/*
 * bsp.h - binary space partition into rectangular rooms.
 *
 * Recursively splits a rectangle along its longer axis (with a splitmix64-driven
 * split position) until leaves hit a minimum size or the depth limit. Used to
 * lay out town building footprints / dungeon rooms. Deterministic from
 * (seed, salt).
 */
#ifndef UOMAPGEN_BSP_H
#define UOMAPGEN_BSP_H

#include <stdint.h>

typedef struct { int x, y, w, h; } bsp_rect;

/* Partition rect (x,y,w,h) into leaf rooms, each at least minSize in both
 * dimensions, to at most maxDepth levels. Writes leaves into out (up to maxOut)
 * and returns the leaf count. Deterministic from (seed, salt). */
int bsp_partition(int x, int y, int w, int h, int minSize, int maxDepth,
                  uint64_t seed, uint32_t salt, bsp_rect *out, int maxOut);

#endif /* UOMAPGEN_BSP_H */
