# UOMapPlusPlus (uomappp) - multi-library procedural UO map generator for ModernUO
#
# Build (ALWAYS single-core, as required for this project):
#   cmake -S . -B build
#   cmake --build build -j1
#
# Run:
#   ./build/uomappp --help
#
cmake_minimum_required(VERSION 3.16)
project(UOMapPlusPlus C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)   # we use getopt_long / strcasecmp (POSIX/GNU)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(UOMG_VERSION "0.1.0")

add_executable(uomappp
  src/main.c
  src/config.c
  src/io.c
  src/noise.c
  src/tiledata.c
  src/biome.c
  src/terrain.c
  src/vegetation.c
  src/mapwriter.c
  src/statics.c
  src/preview.c
  src/mapdef.c
  src/pipeline.c
  src/install.c
  src/field.c
  src/erosion.c
  src/voronoi.c
  src/marching.c
  src/mst.c
  src/cellular.c
  src/wfc.c
  src/poisson.c
  src/astar.c
  src/bsp.c
  src/lsystem.c
)

target_include_directories(uomappp PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/third_party/FastNoiseLite
  ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_compile_definitions(uomappp PRIVATE UOMG_VERSION="${UOMG_VERSION}")

# Warnings on. Determinism: disable FP contraction (FMA) so results don't vary
# with -O level / FMA availability within a given build target.
target_compile_options(uomappp PRIVATE
  -Wall -Wextra -Wshadow
  -ffp-contract=off
)

# FastNoiseLite's C implementation uses math functions.
target_link_libraries(uomappp PRIVATE m)

# noise.c instantiates the FastNoiseLite implementation; its unused 3D-cellular
# path trips -Waggressive-loop-optimizations. We only use 2D OpenSimplex2, so
# silence that upstream warning for this translation unit only.
set_source_files_properties(src/noise.c PROPERTIES
  COMPILE_OPTIONS "-Wno-aggressive-loop-optimizations")
