/*
 * poisson.h - Poisson-disc sampling (Bridson's algorithm).
 *
 * Blue-noise points with a guaranteed minimum separation: natural-looking
 * scatter without clustering. Used to place town sites (and, later, resource
 * nodes) across eligible land. Deterministic: candidate angles/radii come from
 * a splitmix64 stream keyed by (seed, salt); the active list is processed in a
 * fixed order.
 */
#ifndef UOMAPGEN_POISSON_H
#define UOMAPGEN_POISSON_H

#include <stdint.h>

/* Sample points at least `radius` tiles apart in [0,W) x [0,H). If `allow` is
 * non-NULL, a sample is accepted only where allow[x+y*W] != 0. `k` is the
 * attempts-per-active-point (Bridson; 30 is typical). Writes accepted points
 * into outX/outY (up to maxPts) and returns the count. Deterministic from
 * (seed, salt). */
int poisson_sample(int W, int H, double radius, const uint8_t *allow, int k,
                   uint64_t seed, uint32_t salt,
                   int *outX, int *outY, int maxPts);

#endif /* UOMAPGEN_POISSON_H */
