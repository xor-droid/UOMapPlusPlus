#include "uomappp/astar.h"

#include <stdlib.h>
#include <math.h>

typedef struct { double f; long seq; int node; } heap_item;

/* Min-heap on (f, seq) for a deterministic tie-break. */
static void heap_push(heap_item *h, int *n, double f, long seq, int node) {
    int i = (*n)++;
    h[i].f = f; h[i].seq = seq; h[i].node = node;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h[p].f < h[i].f || (h[p].f == h[i].f && h[p].seq <= h[i].seq)) break;
        heap_item t = h[p]; h[p] = h[i]; h[i] = t; i = p;
    }
}

static heap_item heap_pop(heap_item *h, int *n) {
    heap_item top = h[0];
    h[0] = h[--(*n)];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, s = i;
        if (l < *n && (h[l].f < h[s].f || (h[l].f == h[s].f && h[l].seq < h[s].seq))) s = l;
        if (r < *n && (h[r].f < h[s].f || (h[r].f == h[s].f && h[r].seq < h[s].seq))) s = r;
        if (s == i) break;
        heap_item t = h[s]; h[s] = h[i]; h[i] = t; i = s;
    }
    return top;
}

int astar_path(int W, int H, const float *cost, int start, int goal,
               int *outPath, int maxLen) {
    if (W <= 0 || H <= 0 || start < 0 || goal < 0 ||
        start >= W * H || goal >= W * H)
        return -1;
    if (cost[start] < 0 || cost[goal] < 0)
        return -1;

    const size_t n = (size_t)W * (size_t)H;
    double *g = (double *)malloc(n * sizeof(double));
    int *came = (int *)malloc(n * sizeof(int));
    uint8_t *closed = (uint8_t *)calloc(n, 1);
    heap_item *heap = (heap_item *)malloc(n * sizeof(heap_item));
    if (!g || !came || !closed || !heap) {
        free(g); free(came); free(closed); free(heap); return -1;
    }
    for (size_t i = 0; i < n; ++i) { g[i] = 1e300; came[i] = -1; }

    const int gx = goal % W, gy = goal / W;
    long seq = 0;
    int hn = 0;
    g[start] = 0.0;
    int sxp = start % W, syp = start / W;
    double h0 = sqrt((double)((sxp - gx) * (sxp - gx) + (syp - gy) * (syp - gy)));
    heap_push(heap, &hn, h0, seq++, start);

    int rc = -1;
    while (hn > 0) {
        heap_item cur = heap_pop(heap, &hn);
        int c = cur.node;
        if (closed[c]) continue;
        closed[c] = 1;
        if (c == goal) { rc = 0; break; }
        int cx = c % W, cy = c / W;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dy) continue;
                int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                int nb = nx + ny * W;
                if (cost[nb] < 0 || closed[nb]) continue;
                double step = (dx && dy) ? 1.4142135624 : 1.0;
                double ng = g[c] + step * (double)cost[nb];
                if (ng < g[nb]) {
                    g[nb] = ng;
                    came[nb] = c;
                    double hx = sqrt((double)((nx - gx) * (nx - gx) + (ny - gy) * (ny - gy)));
                    heap_push(heap, &hn, ng + hx, seq++, nb);
                }
            }
    }

    int len = -1;
    if (rc == 0) {
        /* Count path length first. */
        int cnt = 0;
        for (int c = goal; c != -1; c = came[c]) ++cnt;
        if (cnt <= maxLen) {
            /* Fill outPath from start..goal. */
            int idx = cnt - 1;
            for (int c = goal; c != -1; c = came[c]) outPath[idx--] = c;
            len = cnt;
        }
    }
    free(g); free(came); free(closed); free(heap);
    return len;
}
