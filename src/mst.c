#include "uomappp/mst.h"

#include <stdlib.h>
#include <string.h>

static int uf_find(int *p, int x) {
    while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; }   /* path halving */
    return x;
}

/* Deterministic ordering: ascending weight, then (a,b) to break ties. */
static int edge_cmp(const void *A, const void *B) {
    const mst_edge *e = (const mst_edge *)A;
    const mst_edge *f = (const mst_edge *)B;
    if (e->w < f->w) return -1;
    if (e->w > f->w) return 1;
    if (e->a != f->a) return e->a < f->a ? -1 : 1;
    if (e->b != f->b) return e->b < f->b ? -1 : 1;
    return 0;
}

int mst_build(const mst_edge *edges, int nedges, int nsites, mst_edge *out) {
    if (nsites <= 0)
        return 0;
    mst_edge *sorted = NULL;
    if (nedges > 0) {
        sorted = (mst_edge *)malloc((size_t)nedges * sizeof(mst_edge));
        if (!sorted)
            return -1;
        memcpy(sorted, edges, (size_t)nedges * sizeof(mst_edge));
        qsort(sorted, (size_t)nedges, sizeof(mst_edge), edge_cmp);
    }
    int *parent = (int *)malloc((size_t)nsites * sizeof(int));
    if (!parent) { free(sorted); return -1; }
    for (int i = 0; i < nsites; ++i) parent[i] = i;

    int k = 0;
    for (int i = 0; i < nedges && k < nsites - 1; ++i) {
        int ra = uf_find(parent, sorted[i].a);
        int rb = uf_find(parent, sorted[i].b);
        if (ra != rb) {
            parent[ra] = rb;
            out[k++] = sorted[i];
        }
    }
    free(sorted);
    free(parent);
    return k;
}
