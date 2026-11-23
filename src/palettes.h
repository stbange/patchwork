#ifndef PATCHWORK_PALETTES_H
#define PATCHWORK_PALETTES_H

#include "quilt.h"

enum { PALETTE_COUNT = 100, PALETTE_NAME_BYTES = 80 };
void palette_name(int id, Language language, char out[PALETTE_NAME_BYTES]);
void palette_colors(int id, uint32_t out[FABRICS]);

#endif
