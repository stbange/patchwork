#ifndef PATCHWORK_STORAGE_H
#define PATCHWORK_STORAGE_H
#include "quilt.h"

typedef enum { STORE_MISSING, STORE_OK, STORE_BAD } LoadResult;
LoadResult load_quilt(const char *path, Quilt *q);
bool save_quilt(const char *path, const Quilt *q);
bool share_sheet(const Quilt *q);
#endif
