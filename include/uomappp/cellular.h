/*
 * cellular.h - cellular-automata primitives for organic blob regions.
 *
 * The classic "random fill + birth/survival smoothing" generator. Used now to
 * grow organic forest clumps; the same primitives serve swamps, caves and ruins
 * in later work. Deterministic: the random fill is drawn per cell from a
 * splitmix64 stream keyed by (seed, salt); smoothing is a fixed-order update
 * into a scratch buffer.
 */
#ifndef UOMAPGEN_CELLULAR_H
#define UOMAPGEN_CELLULAR_H

#include <stdint.h>

/* Random-fill `mask` (W*H): cell = 1 with probability p, else 0. If `domain` is
 * non-NULL, cells where domain[i]==0 are forced to 0 (outside the play area). */
void cellular_fill(uint8_t *mask, const uint8_t *domain, int W, int H,
                   double p, uint64_t seed, uint32_t salt);

/* Run `iterations` of an 8-neighbour (Moore) birth/survival automaton on mask:
 * next = alive ? (neighbours >= survive) : (neighbours >= birth). Out-of-bounds
 * neighbours count as `edge` (0 or 1). Cells outside `domain` (if non-NULL) stay
 * 0. Uses an internal scratch buffer. Returns 0/-1 (alloc). */
int cellular_step(uint8_t *mask, const uint8_t *domain, int W, int H,
                  int birth, int survive, int edge, int iterations);

#endif /* UOMAPGEN_CELLULAR_H */
