/*
 * install.h - swap the generated .mul resource files into the directory that
 * UOFiddler / a ModernUO shard loads from.
 *
 * This is the final pipeline step. When cfg->install_dir is empty it is a no-op
 * success. Otherwise the generated map<N>.mul (and, unless terrain_only,
 * staidx<N>.mul and statics<N>.mul) are copied from cfg->out_dir into
 * cfg->install_dir, overwriting any existing files there.
 */
#ifndef UOMAPGEN_INSTALL_H
#define UOMAPGEN_INSTALL_H

#include "uomappp/config.h"

/* Copy the final resource triplet from out_dir to install_dir. Returns 0 on
 * success (including the no-op case), -1 on a copy failure. */
int install_resources(const mapgen_config *cfg);

#endif /* UOMAPGEN_INSTALL_H */
