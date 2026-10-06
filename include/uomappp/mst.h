/*
 * mst.h - minimum spanning tree over a weighted graph (Kruskal + union-find).
 *
 * Used to decide which territories/towns to connect: nodes are Voronoi sites,
 * candidate edges are adjacent regions weighted by centroid distance, and the
 * MST is the cheapest set of links that keeps everything connected. The road
 * pass (Phase 5) will route an A* path along each MST edge.
 *
 * Determinism: edges are sorted with a fixed tie-break (weight, then a, then b),
 * so the tree is identical for identical input.
 */
#ifndef UOMAPGEN_MST_H
#define UOMAPGEN_MST_H

typedef struct {
    int    a, b;   /* node indices */
    double w;      /* edge weight */
} mst_edge;

/* Build an MST over nsites nodes from the given candidate edges. Writes up to
 * nsites-1 tree edges into out (caller-allocated, >= nsites-1). Returns the
 * number of tree edges, or -1 on allocation failure. */
int mst_build(const mst_edge *edges, int nedges, int nsites, mst_edge *out);

#endif /* UOMAPGEN_MST_H */
