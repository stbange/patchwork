#ifndef PATCHWORK_APP_H
#define PATCHWORK_APP_H

#include <SDL3/SDL.h>
#include "whole.h"
#include "palettes.h"

enum { WALL_WIDTH = 1080, WALL_HEIGHT = 720, CARDS_PER_ROW = 3, CARDS_PER_PAGE = 6, PALETTES_PER_PAGE = 10 };
typedef enum {
    DESIGN, SHEET, LIBRARY, COLORS, UNDO, REDO, LANGUAGE_MENU,
    SWATCH_A, SWATCH_B, SWATCH_C, SWATCH_D, SWATCH_E,
    COL_LESS, COL_MORE, ROW_LESS, ROW_MORE, PATCH_LESS, PATCH_MORE, SET_SIZE, UNIT_IN, UNIT_CM, SET_SEAMS,
    ROTATE_BLOCK, FINISH_PLACING, SHARE, EXPORT_PDF,
    FILTER_ALL, FILTER_TRADITIONAL, FILTER_MODERN, FILTER_WHOLE, PREV_PAGE, NEXT_PAGE, APPLY, BRUSH,
    ORIGINAL_COLORS, FABRIC_PREV, FABRIC_NEXT, ADD_SWATCH, EDIT_LAYER, REMOVE_OVERLAY, RESTORE_BLOCKS, LANGUAGE_EN, LANGUAGE_DE, LANGUAGE_PL, EDIT_APPLY, EDIT_CANCEL, CONTROL_COUNT
} Control;
typedef enum { EDIT_NONE, EDIT_SIZE_VALUE, EDIT_FABRIC } Editor;

typedef struct View View;
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    View *view;
    History history;
    char *path, *settings_path;
    Preferences prefs;
    Text status;
    Control screen;
    PatternFilter filter;
    QuiltSetting setting;
    int chosen, fabric, fabric_page, row, col, patch, brush, turn, palette_page, whole_piece;
    WholeDesign whole_choice;
    Layer selected_layer;
    bool whole_library, background_only, adding;
    Editor editor;
    uint32_t initial_color;
    char input[NUMBER_BYTES];
    bool cursor, protected_plan, dirty, language_menu, replace_input, invalid, pdf_pending;
} App;

extern const SDL_FRect control_box[CONTROL_COUNT];
SDL_FRect quilt_box(const Quilt *q, SDL_FRect mat);
SDL_FRect pattern_card(int slot);
SDL_FRect setting_box(int setting);
SDL_FRect palette_card(int slot);
SDL_FRect slider_box(int channel);
SDL_FRect fabric_nav(Control id, Control screen);
extern const SDL_FRect design_mat;
View *open_view(SDL_Renderer *renderer, const char *base);
void close_view(View *view);
void draw_app(App *app);
#endif
