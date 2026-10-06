/*
 * housing.h - post-generation buildability summary.
 *
 * Reports, from the finished grid, how much dry non-ocean land is actually
 * buildable (clear of water, impassable mountain rock, blocking vegetation
 * features and world statics) and how many houses of each standard UO footprint
 * would fit on perfectly flat, clear ground. Diagnostic only: prints to stdout
 * and never touches the output bytes.
 */
#ifndef UOMAPGEN_HOUSING_H
#define UOMAPGEN_HOUSING_H

#include "uomappp/config.h"
#include "uomappp/terrain.h"

/* Print the housing/buildability summary for the generated grid. */
void housing_summary_print(const terrain_grid *g, const mapgen_config *cfg);

#endif /* UOMAPGEN_HOUSING_H */
