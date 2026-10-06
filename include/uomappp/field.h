/*
 * field.h - float-grid utilities (the SciPy/NumPy-style algorithms we need,
 * reimplemented in POSIX C: no Python at runtime).
 *
 * All grids are row-major, index = x + y*W. These are pure, deterministic
 * transforms (no RNG) shared by the erosion pass now and by later passes
 * (coast distance for moisture, mask smoothing, etc.).
 */
#ifndef UOMAPGEN_FIELD_H
#define UOMAPGEN_FIELD_H

#include <stdint.h>

/* Two-pass chamfer (3,4-style, using 1 and sqrt2 weights) approximate Euclidean
 * distance transform. `mask` is W*H: nonzero cells are seeds (distance 0), zero
 * cells get the distance (in tile units) to the nearest seed written into `out`.
 * If there are no seeds, every cell gets a large finite value. Returns 0/-1. */
int field_distance_transform(const uint8_t *mask, float *out, int W, int H);

/* Separable box blur (running-sum, O(W*H) per pass), `passes` iterations of the
 * given radius -> approaches a Gaussian. In place on buf (W*H floats), using an
 * internal scratch buffer. radius<1 or passes<1 is a no-op. Returns 0/-1. */
int field_blur(float *buf, int W, int H, int radius, int passes);

/* Rescale buf to [0,1] in place using its min/max. No-op when the field is
 * flat (max==min). */
void field_normalize(float *buf, int W, int H);

#endif /* UOMAPGEN_FIELD_H */
