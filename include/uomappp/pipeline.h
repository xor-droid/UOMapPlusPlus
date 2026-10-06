/*
 * pipeline.h - the generation harness.
 *
 * pipeline_run() is the ordered "wrapper" that drives one full map build:
 * load the optional tiledata reference, generate the terrain grid (whose
 * internal passes emit per-pass PNGs when enabled), write the .mul triplet,
 * render the final preview, emit the ModernUO mapdef snippet, and finally swap
 * the resource files into the install directory. Future library passes slot
 * into this fixed order.
 *
 * cfg must already be validated (config_validate) and out_dir must exist.
 * Returns a process exit code: 0 on success, non-zero on failure.
 */
#ifndef UOMAPGEN_PIPELINE_H
#define UOMAPGEN_PIPELINE_H

#include "uomappp/config.h"

int pipeline_run(const mapgen_config *cfg);

#endif /* UOMAPGEN_PIPELINE_H */
