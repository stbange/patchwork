#ifndef PATCHWORK_APP_H
#define PATCHWORK_APP_H

#include <SDL3/SDL.h>
#include "quilt.h"

enum { WALL_WIDTH = 1080, WALL_HEIGHT = 720 };
typedef enum {
    DESIGN, SHEET, UNDO, REDO, SWATCH_A, SWATCH_B, SWATCH_C, SWATCH_D, SWATCH_E,
    COL_LESS, COL_MORE, ROW_LESS, ROW_MORE, PATCH_LESS, PATCH_MORE, SHARE, CONTROL_COUNT
} Control;

typedef struct View View;
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    View *view;
    History history;
    char *path;
    const char *status;
    int fabric, row, col;
    bool sheet, cursor, protected_plan, dirty;
} App;

extern const SDL_FRect control_box[CONTROL_COUNT];
SDL_FRect quilt_box(const Quilt *q, SDL_FRect mat);
extern const SDL_FRect design_mat;
View *open_view(SDL_Renderer *renderer, const char *base);
void close_view(View *view);
void draw_app(App *app);
#endif
