/*
 * wfc.h - Wave Function Collapse over an undirected graph.
 *
 * General tiled WFC (constraint-based tile assignment): each node holds a domain
 * of still-possible tiles; we repeatedly collapse the lowest-entropy node to a
 * single tile and propagate the adjacency constraints to its neighbours. Here it
 * is used to assign biomes to Voronoi territories so neighbouring territories
 * only meet along legal transitions (no desert touching snow) -- i.e. tile
 * transitions at the territory level. The same solver will drive building/ruin
 * tiling later.
 *
 * Determinism: the min-entropy node is chosen by lowest remaining count then
 * lowest index; the collapse tile is a weighted draw from a splitmix64 stream
 * keyed by (seed, NOISE_LAYER_WFC); propagation uses a fixed-order queue. No
 * backtracking -- on a contradiction the solver returns -1 and the caller keeps
 * its prior assignment.
 *
 * Limit: nTiles <= 32 (domains are uint32 bitsets).
 */
#ifndef UOMAPGEN_WFC_H
#define UOMAPGEN_WFC_H

#include <stdint.h>

/* Solve over nNodes nodes. Adjacency is CSR: neighbours of node i are
 * adjList[adjStart[i] .. adjStart[i+1]). `allowed` is an nTiles*nTiles symmetric
 * 0/1 matrix (allowed[a*nTiles+b] = may a and b be adjacent). `weight[t]` (>0)
 * biases collapse toward tile t. `prior[i]` in [0,nTiles) pre-fixes a node, or
 * -1 to leave it free (prior may be NULL = all free). Writes the chosen tile per
 * node into out[nNodes]. Returns 0 on a full solve, -1 on contradiction/alloc. */
int wfc_solve_graph(int nNodes, const int *adjStart, const int *adjList,
                    int nTiles, const uint8_t *allowed, const double *weight,
                    const int *prior, uint64_t seed, int *out);

#endif /* UOMAPGEN_WFC_H */
