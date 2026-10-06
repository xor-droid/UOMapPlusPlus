/*
 * voronoi.h - raster (jittered-grid) Voronoi territories.
 *
 * Rather than vendoring a polygon Voronoi/Delaunay library and rasterizing it,
 * we compute the Voronoi partition directly on the tile grid: one jittered site
 * per cell of a coarse grid, and each map cell is labelled with its nearest
 * site. On a raster this is exact (within the search radius), cheap, and fits
 * the tile pipeline. The site list doubles as the Delaunay-style node set for
 * the territory graph (see mst.h); region adjacency comes from border cells.
 *
 * Determinism: site jitter is drawn from a splitmix64 stream keyed by
 * (seed, NOISE_LAYER_VORONOI, site index) — independent of iteration order.
 */
#ifndef UOMAPGEN_VORONOI_H
#define UOMAPGEN_VORONOI_H

#include <stdint.h>

typedef struct {
    double x, y;   /* site position in tile space */
    int    id;     /* == index into sites[] == region id */
} voronoi_site;

typedef struct {
    voronoi_site *sites;
    int           n;        /* site count = gx*gy */
    int           gx, gy;   /* coarse-grid dimensions */
    int           spacing;  /* coarse-grid cell size in tiles */
} voronoi_diagram;

/* Build the diagram and fill region[] (W*H) with each cell's nearest-site id in
 * [0,n). seed/spacing/jitter drive the layout (jitter in [0,1] = fraction of a
 * cell). When warp_amp > 0 the per-cell nearest-site query point is displaced by
 * an OpenSimplex domain-warp of that amplitude (tiles) at warp_freq, so the
 * territory borders wiggle organically instead of reading as straight polygon
 * edges. Site positions are unaffected by the warp. Returns 0 on success, -1 on
 * allocation failure. Free with voronoi_free(). */
int voronoi_build(voronoi_diagram *vd, int32_t *region, int W, int H,
                  uint64_t seed, int spacing, double jitter,
                  double warp_amp, double warp_freq);

void voronoi_free(voronoi_diagram *vd);

#endif /* UOMAPGEN_VORONOI_H */
