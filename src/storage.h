#ifndef PATCHWORK_STORAGE_H
#define PATCHWORK_STORAGE_H
#include "quilt.h"
#include <SDL3/SDL.h>

typedef enum { STORE_MISSING, STORE_OK, STORE_BAD } LoadResult;
LoadResult load_quilt(const char *path, Quilt *q);
bool save_quilt(const char *path, const Quilt *q);
Preferences load_preferences(const char *path);
bool save_preferences(const char *path, Preferences prefs);
bool share_sheet(const Quilt *q, Preferences prefs);
bool share_pdf(const Quilt *q, Preferences prefs, SDL_Window *window);
#endif
