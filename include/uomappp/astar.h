/*
 * astar.h - A* shortest path on an 8-connected cost grid.
 *
 * Used to route roads/trails between towns: the cost field makes paths prefer
 * flat ground, avoid water and mountains, and pay a premium (a bridge) to cross
 * a river. Deterministic: the open set is a binary heap ordered by (f, then a
 * monotonic insertion counter) so ties break the same way every run.
 */
#ifndef UOMAPGEN_ASTAR_H
#define UOMAPGEN_ASTAR_H

#include <stdint.h>

/* Find a least-cost path from start to goal (cell indices x+y*W) on a WxH grid.
 * cost[i] is the cost to ENTER cell i; cost[i] < 0 marks an impassable cell.
 * Diagonal moves cost sqrt(2) * entry cost. Writes the path (start..goal
 * inclusive) as cell indices into outPath (up to maxLen) and returns the path
 * length, or -1 if unreachable / on allocation failure / if it exceeds maxLen. */
int astar_path(int W, int H, const float *cost, int start, int goal,
               int *outPath, int maxLen);

#endif /* UOMAPGEN_ASTAR_H */
