/*
 * lsystem.h - stochastic L-system branching trails.
 *
 * A bracketed turtle L-system (F -> F[+F][-F] style growth) grown recursively:
 * each segment draws forward, then spawns a couple of branches at +/- a turn
 * angle. Used for organic side-trails radiating from towns. Deterministic:
 * branch counts and angle jitter come from a splitmix64 stream keyed by
 * (seed, salt).
 */
#ifndef UOMAPGEN_LSYSTEM_H
#define UOMAPGEN_LSYSTEM_H

#include <stdint.h>

/* Grow a branching trail from (x0,y0) with initial heading angle0 (radians).
 * Each segment is `step` tiles long; branches turn by +/- `turn` radians (with
 * jitter) and recurse `depth` levels, shortening as they go. Visited cell
 * indices (x+y*W), clipped to the grid, are appended to out (up to maxOut).
 * Returns the number written. Deterministic from (seed, salt). */
int lsystem_trail(int W, int H, int x0, int y0, double angle0, double step,
                  double turn, int depth, uint64_t seed, uint32_t salt,
                  int *out, int maxOut);

#endif /* UOMAPGEN_LSYSTEM_H */
