#ifndef PATCHWORK_PDF_H
#define PATCHWORK_PDF_H

#include "quilt.h"
#include <SDL3/SDL.h>

SDL_IOStream *make_pdf(const Quilt *q, Preferences prefs);
const void *pdf_data(SDL_IOStream *output);

#endif
