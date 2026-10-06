#include "uomappp/wfc.h"
#include "uomappp/noise.h"

#include <stdlib.h>

/* Allowed domain for neighbour nb given node's domain: tile u survives iff some
 * tile t in nodeDom permits adjacency u<->t. */
static uint32_t support_filter(uint32_t nbDom, uint32_t nodeDom,
                               int nTiles, const uint8_t *allowed) {
    uint32_t keep = 0;
    for (int u = 0; u < nTiles; ++u) {
        if (!(nbDom & (1u << u))) continue;
        for (int t = 0; t < nTiles; ++t) {
            if (!(nodeDom & (1u << t))) continue;
            if (allowed[t * nTiles + u]) { keep |= (1u << u); break; }
        }
    }
    return keep;
}

/* Propagate constraints from a seed node to a fixed point. The worklist uses an
 * in-queue flag so it never exceeds nNodes entries. Propagation is monotone
 * (bits only ever removed), so the fixed point is independent of pop order ->
 * the result stays deterministic. Returns 0 on success, -1 on contradiction. */
static int propagate(int seedNode, uint32_t *dom, const int *adjStart,
                     const int *adjList, int nTiles, const uint8_t *allowed,
                     int *stack, uint8_t *inq) {
    int sp = 0;
    stack[sp++] = seedNode;
    inq[seedNode] = 1;
    while (sp > 0) {
        int node = stack[--sp];
        inq[node] = 0;
        for (int e = adjStart[node]; e < adjStart[node + 1]; ++e) {
            int nb = adjList[e];
            uint32_t nd = support_filter(dom[nb], dom[node], nTiles, allowed);
            if (nd == 0)
                return -1;                 /* contradiction */
            if (nd != dom[nb]) {
                dom[nb] = nd;
                if (!inq[nb]) { stack[sp++] = nb; inq[nb] = 1; }
            }
        }
    }
    return 0;
}

int wfc_solve_graph(int nNodes, const int *adjStart, const int *adjList,
                    int nTiles, const uint8_t *allowed, const double *weight,
                    const int *prior, uint64_t seed, int *out) {
    if (nNodes <= 0 || nTiles <= 0 || nTiles > 32)
        return -1;

    uint32_t full = (nTiles == 32) ? 0xFFFFFFFFu : ((1u << nTiles) - 1u);
    uint32_t *dom  = (uint32_t *)malloc((size_t)nNodes * sizeof(uint32_t));
    int     *stack = (int *)malloc((size_t)nNodes * sizeof(int));
    uint8_t *inq   = (uint8_t *)calloc((size_t)nNodes, 1);
    if (!dom || !stack || !inq) { free(dom); free(stack); free(inq); return -1; }

    for (int i = 0; i < nNodes; ++i)
        dom[i] = (prior && prior[i] >= 0 && prior[i] < nTiles)
                     ? (1u << prior[i]) : full;

    /* Propagate any pre-fixed priors. */
    for (int i = 0; i < nNodes; ++i)
        if (__builtin_popcount(dom[i]) == 1)
            if (propagate(i, dom, adjStart, adjList, nTiles, allowed, stack, inq) != 0) {
                free(dom); free(stack); free(inq); return -1;
            }

    uint64_t st = seed ^ (0xD1B54A32D192ED03ULL * (uint64_t)(NOISE_LAYER_WFC + 1));

    for (;;) {
        /* Min-entropy node: fewest remaining tiles (>1), lowest index on ties. */
        int best = -1, bestCount = nTiles + 1;
        for (int i = 0; i < nNodes; ++i) {
            int c = __builtin_popcount(dom[i]);
            if (c > 1 && c < bestCount) { bestCount = c; best = i; }
        }
        if (best < 0)
            break;   /* all nodes collapsed */

        /* Weighted collapse among the node's remaining tiles. Weights are
         * per-node: weight[node*nTiles + t] (NULL => uniform). */
        const double *w = weight ? weight + (size_t)best * nTiles : NULL;
        double total = 0.0;
        for (int t = 0; t < nTiles; ++t)
            if (dom[best] & (1u << t)) total += (w ? w[t] : 1.0);
        double r = (double)(noise_splitmix64(&st) >> 11)
                   * (1.0 / 9007199254740992.0) * total;
        int chosen = -1;
        for (int t = 0; t < nTiles; ++t) {
            if (!(dom[best] & (1u << t))) continue;
            r -= (w ? w[t] : 1.0);
            if (r <= 0.0) { chosen = t; break; }
        }
        if (chosen < 0)
            for (int t = nTiles - 1; t >= 0; --t)
                if (dom[best] & (1u << t)) { chosen = t; break; }

        dom[best] = (1u << chosen);
        if (propagate(best, dom, adjStart, adjList, nTiles, allowed, stack, inq) != 0) {
            free(dom); free(stack); free(inq); return -1;
        }
    }

    for (int i = 0; i < nNodes; ++i) {
        uint32_t d = dom[i];
        if (d == 0) { free(dom); free(stack); free(inq); return -1; }
        int t = 0;
        while (!(d & 1u)) { d >>= 1; ++t; }
        out[i] = t;
    }
    free(dom); free(stack); free(inq);
    return 0;
}
