#include "app.h"
#include "storage.h"
#define SDL_MAIN_USE_CALLBACKS 1
#define SDL_MAIN_NOIMPL
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
static const char render_drivers[] = "opengles2,software";
#else
static const char render_drivers[] = "opengl,opengles2,software";
#endif

static const Quilt *current_quilt(const App *a)
{
    return &a->history.steps[a->history.at];
}

static void persist(App *a)
{
    a->status = a->protected_plan ? PROTECTED_PLAN : save_quilt(a->path, current_quilt(a)) ? SAVED : SAVE_FAILED;
}

static void revise(App *a, const Quilt *q)
{
    if (record(&a->history, q)) persist(a);
    a->dirty = true;
}

static void store_preferences(App *a)
{
    if (!save_preferences(a->settings_path, a->prefs)) a->status = SETTINGS_FAILED;
    else if (a->status == SETTINGS_FAILED) a->status = a->protected_plan ? PROTECTED_PLAN : EDITS_SAVE;
    a->dirty = true;
}

static void edit(App *a, Editor editor, int fabric)
{
    a->editor = editor;
    a->adding = editor == EDIT_FABRIC && fabric == current_quilt(a)->fabrics;
    if (!a->adding) a->fabric = fabric;
    if (editor == EDIT_FABRIC) {
        a->initial_color = a->adding ? extra_fabric_color(fabric) : current_quilt(a)->color[fabric];
        snprintf(a->input, sizeof a->input, "%06X", (unsigned)a->initial_color);
    } else
        decimal(a->input, a->prefs.language, current_quilt(a)->size / (double)unit_scale(a->prefs.unit), 4, true);
    a->replace_input = true;
    a->invalid = false;
    SDL_StartTextInput(a->window);
}

static void close_editor(App *a)
{
    a->editor = EDIT_NONE;
    a->adding = false;
    SDL_StopTextInput(a->window);
    a->dirty = true;
}

static void apply_editor(App *a)
{
    Quilt q = *current_quilt(a);
    int value = a->editor == EDIT_FABRIC ? parse_color(a->input) : parse_size(a->input, a->prefs.unit);
    if (value < 0) {
        a->invalid = true;
        a->dirty = true;
        return;
    }
    if (a->adding) {
        a->fabric = q.fabrics;
        add_fabric(&q, (uint32_t)value);
        a->fabric_page = a->fabric / STARTER_FABRICS;
        a->brush = 0;
    } else if (a->editor == EDIT_FABRIC)
        q.color[a->fabric] = (uint32_t)value;
    else resize_quilt(&q, q.rows, q.cols, value);
    revise(a, &q);
    close_editor(a);
}

static void type_value(App *a, const char *value)
{
    const char *allowed = a->editor == EDIT_FABRIC ? "#0123456789ABCDEFabcdef" : "0123456789.,";
    size_t length = strlen(value), used = a->replace_input ? 0 : strlen(a->input);
    if (strspn(value, allowed) != length || used + length >= sizeof a->input) return;
    memcpy(a->input + used, value, length + 1);
    a->replace_input = false;
    a->invalid = false;
    a->dirty = true;
}

static void editor_key(App *a, SDL_Keycode key, SDL_Keymod mod)
{
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) { apply_editor(a); return; }
    if (key == SDLK_ESCAPE) { close_editor(a); return; }
    if (key == SDLK_A && (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
        a->replace_input = true;
    } else if (key == SDLK_BACKSPACE || key == SDLK_DELETE) {
        size_t length = strlen(a->input);
        a->input[key == SDLK_DELETE || a->replace_input || !length ? 0 : length - 1] = 0;
        a->replace_input = false;
        a->invalid = false;
    }
    a->dirty = true;
}

static void change_size(App *a, Control id)
{
    Quilt q = *current_quilt(a);
    int change = (id - COL_LESS) % 2 ? 1 : -1;
    int cols = q.cols + (id <= COL_MORE ? change : 0);
    int rows = q.rows + (id == ROW_LESS || id == ROW_MORE ? change : 0);
    int size = SDL_clamp(q.size + (id >= PATCH_LESS ? change * unit_scale(a->prefs.unit) : 0), MIN_SIZE, MAX_SIZE);
    if (resize_quilt(&q, rows, cols, size)) {
        a->whole_piece = -1;
        revise(a, &q);
    }
}

static void go_page(App *a, int delta)
{
    if (a->screen == COLORS) {
        a->palette_page = SDL_clamp(a->palette_page + delta, 0, (PALETTE_COUNT - 1) / PALETTES_PER_PAGE);
    } else if (!a->whole_library) {
        int ids[PATTERN_COUNT];
        int count = catalog_ids(a->filter, ids);
        int next = (a->chosen / CARDS_PER_PAGE + delta) * CARDS_PER_PAGE;
        if (next >= 0 && next < count) a->chosen = next;
    }
}

static void use_pattern(App *a, bool fill)
{
    if (a->whole_library) {
        Quilt q = *current_quilt(a);
        Layer layer = a->whole_choice >= VINE ? APPLIQUE : FOUNDATION;
        if (layer == APPLIQUE && !fill) set_whole_design(&q, FOUNDATION, WHOLECLOTH);
        set_whole_design(&q, layer, a->whole_choice);
        a->selected_layer = layer;
        a->background_only = false;
        a->whole_piece = -1;
        a->screen = DESIGN;
        a->brush = 0;
        a->cursor = false;
        revise(a, &q);
        return;
    }
    int ids[PATTERN_COUNT];
    catalog_ids(a->filter, ids);
    int id = ids[a->chosen];
    a->screen = DESIGN;
    a->patch = 0;
    a->cursor = false;
    a->brush = fill ? 0 : id;
    a->whole_piece = -1;
    a->selected_layer = FOUNDATION;
    a->turn = 1;
    if (fill) {
        Quilt q = *current_quilt(a);
        if (arrange(&q, id, a->setting)) revise(a, &q);
    }
}

static void use_palette(App *a, int number)
{
    Quilt q = *current_quilt(a);
    if (number >= 0) palette_colors(number, q.color);
    else memcpy(q.color, original_palette, sizeof original_palette);
    revise(a, &q);
}

static void click_control(App *a, Control id)
{
    if (id <= COLORS) {
        a->screen = id;
    } else if (id == UNDO || id == REDO) {
        if (travel(&a->history, id == UNDO ? -1 : 1)) {
            persist(a);
            a->whole_piece = -1;
            a->cursor = false;
        }
    } else if (id == LANGUAGE_MENU) {
        a->language_menu = !a->language_menu;
    } else if (id >= LANGUAGE_EN && id <= LANGUAGE_PL) {
        a->prefs.language = (Language)(id - LANGUAGE_EN);
        a->language_menu = false;
        store_preferences(a);
    } else if (id >= SWATCH_A && id <= SWATCH_E) {
        int f = a->fabric_page * STARTER_FABRICS + id - SWATCH_A;
        if (f >= current_quilt(a)->fabrics) return;
        a->fabric = f;
        if (a->screen == COLORS) edit(a, EDIT_FABRIC, a->fabric);
        else a->brush = 0;
    } else if (id == ADD_SWATCH) {
        if (current_quilt(a)->fabrics < MAX_FABRICS) edit(a, EDIT_FABRIC, current_quilt(a)->fabrics);
    } else if (id == FABRIC_PREV || id == FABRIC_NEXT) {
        int last = (current_quilt(a)->fabrics - 1) / STARTER_FABRICS;
        a->fabric_page = SDL_clamp(a->fabric_page + (id == FABRIC_NEXT ? 1 : -1), 0, last);
    } else if (id == EDIT_LAYER) {
        a->background_only = !a->background_only;
        a->selected_layer = a->background_only ? FOUNDATION : APPLIQUE;
        a->whole_piece = -1;
        a->cursor = false;
    } else if (id == RESTORE_BLOCKS) {
        Quilt q = *current_quilt(a);
        set_whole_design(&q, FOUNDATION, BLOCK_TOP);
        a->whole_piece = -1;
        revise(a, &q);
    } else if (id == REMOVE_OVERLAY) {
        Quilt q = *current_quilt(a);
        set_whole_design(&q, APPLIQUE, BLOCK_TOP);
        a->selected_layer = FOUNDATION;
        a->whole_piece = -1;
        a->background_only = false;
        revise(a, &q);
    } else if (id == SET_SIZE) {
        edit(a, EDIT_SIZE_VALUE, a->fabric);
    } else if (id == UNIT_IN || id == UNIT_CM) {
        a->prefs.unit = id == UNIT_IN ? INCHES : CENTIMETRES;
        store_preferences(a);
    } else if (id == SET_SEAMS) {
        Quilt q = *current_quilt(a);
        if (!q.foundation) q.seams = (uint8_t)((q.seams + 1) % SEAM_COUNT);
        revise(a, &q);
    } else if (id == ROTATE_BLOCK) {
        a->turn = a->turn % 4 + 1;
    } else if (id == FINISH_PLACING) {
        a->brush = 0;
    } else if (id == SHARE) {
#ifdef __EMSCRIPTEN__
        Text status = SHEET_DOWNLOADED;
#else
        Text status = SHEET_COPIED;
#endif
        a->status = share_sheet(current_quilt(a), a->prefs) ? status : EXPORT_FAILED;
    } else if (id == EXPORT_PDF) {
        if (a->pdf_pending) return;
        if (!share_pdf(current_quilt(a), a->prefs, a->window)) a->status = EXPORT_FAILED;
#ifdef __EMSCRIPTEN__
        else a->status = PDF_EXPORTED;
#else
        else a->pdf_pending = true;
#endif
    } else if (id >= FILTER_ALL && id <= FILTER_MODERN) {
        a->whole_library = false;
        PatternFilter filter = (PatternFilter)(id - FILTER_ALL);
        if (a->filter != filter) a->chosen = 0;
        a->filter = filter;
    } else if (id == FILTER_WHOLE) {
        a->whole_library = true;
    } else if (id == PREV_PAGE || id == NEXT_PAGE) {
        go_page(a, id == PREV_PAGE ? -1 : 1);
    } else if (id == APPLY || id == BRUSH) {
        use_pattern(a, id == APPLY);
    } else if (id == ORIGINAL_COLORS) {
        use_palette(a, -1);
    } else {
        change_size(a, id);
    }
    const Quilt *q = current_quilt(a);
    a->fabric = SDL_min(a->fabric, q->fabrics - 1);
    a->fabric_page = SDL_min(a->fabric_page, (q->fabrics - 1) / STARTER_FABRICS);
    if (!q->applique) a->selected_layer = FOUNDATION;
    a->row = SDL_min(a->row, q->rows - 1);
    a->col = SDL_min(a->col, q->cols - 1);
    a->patch = SDL_min(a->patch, block_pattern(&q->cell[a->row][a->col])->count - 1);
    a->dirty = true;
}

static void place(App *a)
{
    Quilt q = *current_quilt(a);
    if (a->brush) {
        set_whole_design(&q, FOUNDATION, BLOCK_TOP);
        place_block(&q.cell[a->row][a->col], a->brush, a->turn);
        a->patch = 0;
    } else if (a->selected_layer == APPLIQUE ? q.applique : q.foundation) {
        if (a->whole_piece < 0) a->whole_piece = next_whole(&q, a->selected_layer, -1, 1);
        paint_whole(&q, a->selected_layer, a->whole_piece, a->fabric);
    } else {
        paint(&q, a->row, a->col, a->patch, a->fabric);
    }
    revise(a, &q);
}

static bool adjust_slider(App *a, SDL_FPoint point)
{
    if (a->editor != EDIT_FABRIC) return false;
    for (int i = 0; i < 3; ++i) {
        SDL_FRect slider = slider_box(i);
        if (!SDL_PointInRectFloat(&point, &slider)) continue;
        int parsed = parse_color(a->input);
        unsigned color = parsed < 0 ? a->initial_color : (unsigned)parsed;
        int shift = 16 - 8 * i;
        unsigned value = (unsigned)SDL_clamp(SDL_roundf((point.x - slider.x) * 255 / slider.w), 0, 255);
        color = (color & ~(255u << shift)) | (value << shift);
        snprintf(a->input, sizeof a->input, "%06X", color);
        a->replace_input = true;
        a->invalid = false;
        a->dirty = true;
        return true;
    }
    return false;
}

static void library_pointer(App *a, SDL_FPoint point)
{
    if (a->whole_library) {
        for (int design = HEX_FIELD; design < WHOLE_COUNT; ++design) {
            SDL_FRect card = pattern_card(design - HEX_FIELD);
            if (SDL_PointInRectFloat(&point, &card)) a->whole_choice = (WholeDesign)design;
        }
        a->dirty = true;
        return;
    }
    int ids[PATTERN_COUNT];
    int count = catalog_ids(a->filter, ids);
    int start = a->chosen / CARDS_PER_PAGE * CARDS_PER_PAGE;
    for (int i = 0; i < CARDS_PER_PAGE && start + i < count; ++i) {
        SDL_FRect card = pattern_card(i);
        if (SDL_PointInRectFloat(&point, &card)) a->chosen = start + i;
    }
    for (int i = 0; i < SETTING_COUNT; ++i) {
        SDL_FRect setting = setting_box(i);
        if (SDL_PointInRectFloat(&point, &setting)) a->setting = (QuiltSetting)i;
    }
    a->dirty = true;
}

static void board_pointer(App *a, SDL_FPoint point)
{
    const Quilt *q = current_quilt(a);
    SDL_FRect board = quilt_box(q, design_mat);
    if (point.x < board.x || point.x >= board.x + board.w ||
        point.y < board.y || point.y >= board.y + board.h) return;
    double side = board.w / q->cols, x = (point.x - board.x) / side, y = (point.y - board.y) / side;
    if (!a->brush) {
        for (int layer = a->background_only ? FOUNDATION : APPLIQUE; layer >= FOUNDATION; --layer) {
            int id = hit_whole(q, (Layer)layer, (XY){x, y});
            if (id < 0) continue;
            a->selected_layer = (Layer)layer;
            a->whole_piece = id;
            a->cursor = true;
            place(a);
            return;
        }
    }
    a->selected_layer = FOUNDATION;
    a->whole_piece = -1;
    for (int r = 0; r < q->rows; ++r)
        for (int c = 0; c < q->cols; ++c) {
            BlockBounds b = block_bounds(q, r, c);
            if (x < b.x || x >= b.x + b.w || y < b.y || y >= b.y + b.h) continue;
            const Block *block = &q->cell[r][c];
            int p = hit_patch(block_pattern(block), block->turn, (float)((x - b.x) * BLOCK_UNITS / b.w),
                              (float)((y - b.y) * BLOCK_UNITS / b.h));
            if (p >= 0) {
                a->row = r;
                a->col = c;
                a->patch = p;
                a->cursor = true;
                place(a);
            }
            return;
        }
}

static bool control_visible(const App *a, Control id)
{
    if (id <= LANGUAGE_MENU) return true;
    const Quilt *q = current_quilt(a);
    bool basket = id >= FABRIC_PREV && id <= ADD_SWATCH;
    bool pager = id == PREV_PAGE || id == NEXT_PAGE;
    switch (a->screen) {
    case DESIGN:
        return basket || (id >= SWATCH_A && id <= SET_SEAMS) ||
               (a->brush && (id == ROTATE_BLOCK || id == FINISH_PLACING)) ||
               (q->applique && !a->brush && id == EDIT_LAYER);
    case SHEET:
        return id == SHARE || id == EXPORT_PDF ||
               ((id == FABRIC_PREV || id == FABRIC_NEXT) && q->fabrics > STARTER_FABRICS);
    case COLORS:
        return basket || pager || id == ORIGINAL_COLORS || (id >= SWATCH_A && id <= SWATCH_E);
    default:
        if (!a->whole_library) return id >= FILTER_ALL && id <= BRUSH;
        return (id >= FILTER_ALL && id <= FILTER_WHOLE) || id == APPLY || (id == BRUSH && a->whole_choice >= VINE) ||
               (id == REMOVE_OVERLAY && q->applique) || (id == RESTORE_BLOCKS && q->foundation);
    }
}

static void pointer(App *a, float x, float y)
{
    SDL_FPoint point = {x, y};
    if (a->editor) {
        if (SDL_PointInRectFloat(&point, &control_box[EDIT_APPLY])) apply_editor(a);
        else if (SDL_PointInRectFloat(&point, &control_box[EDIT_CANCEL])) close_editor(a);
        else adjust_slider(a, point);
        return;
    }
    if (a->language_menu) {
        for (Control id = LANGUAGE_EN; id <= LANGUAGE_PL; ++id)
            if (SDL_PointInRectFloat(&point, &control_box[id])) { click_control(a, id); return; }
        click_control(a, LANGUAGE_MENU);
        return;
    }
    for (Control id = DESIGN; id < LANGUAGE_EN; ++id) {
        if (!control_visible(a, id)) continue;
        bool basket = id >= FABRIC_PREV && id <= ADD_SWATCH;
        SDL_FRect rect = basket ? fabric_nav(id, a->screen) : control_box[id];
        if (SDL_PointInRectFloat(&point, &rect)) {
            click_control(a, id);
            return;
        }
    }
    if (a->screen == DESIGN) board_pointer(a, point);
    else if (a->screen == LIBRARY) library_pointer(a, point);
    else if (a->screen == COLORS)
        for (int slot = 0; slot < PALETTES_PER_PAGE; ++slot) {
            SDL_FRect card = palette_card(slot);
            if (SDL_PointInRectFloat(&point, &card)) {
                use_palette(a, a->palette_page * PALETTES_PER_PAGE + slot);
                return;
            }
        }
}

static void library_key(App *a, SDL_Keycode key)
{
    int ids[PATTERN_COUNT];
    int count = catalog_ids(a->filter, ids);
    if (a->whole_library) {
        if (key == SDLK_X && current_quilt(a)->applique) {
            click_control(a, REMOVE_OVERLAY);
            return;
        }
        if (key == SDLK_G && current_quilt(a)->foundation) {
            click_control(a, RESTORE_BLOCKS);
            return;
        }
        if (key == SDLK_B && a->whole_choice < VINE) return;
        int delta =
            (key == SDLK_RIGHT) - (key == SDLK_LEFT) + CARDS_PER_ROW * ((key == SDLK_DOWN) - (key == SDLK_UP));
        if (delta) {
            a->whole_choice = (WholeDesign)SDL_clamp((int)a->whole_choice + delta, HEX_FIELD, WHOLE_COUNT - 1);
            a->dirty = true;
            return;
        }
    }
    switch (key) {
    case SDLK_ESCAPE: click_control(a, DESIGN); break;
    case SDLK_4: click_control(a, FILTER_WHOLE); break;
    case SDLK_RETURN: case SDLK_SPACE: click_control(a, APPLY); break;
    case SDLK_B: click_control(a, BRUSH); break;
    case SDLK_PAGEUP: click_control(a, PREV_PAGE); break;
    case SDLK_PAGEDOWN: click_control(a, NEXT_PAGE); break;
    case SDLK_1: case SDLK_2: case SDLK_3:
        click_control(a, (Control)(FILTER_ALL + key - SDLK_1));
        break;
    case SDLK_LEFT: a->chosen = SDL_max(0, a->chosen - 1); break;
    case SDLK_RIGHT: a->chosen = SDL_min(count - 1, a->chosen + 1); break;
    case SDLK_UP: a->chosen = SDL_max(0, a->chosen - CARDS_PER_ROW); break;
    case SDLK_DOWN: a->chosen = SDL_min(count - 1, a->chosen + CARDS_PER_ROW); break;
    case SDLK_LEFTBRACKET: case SDLK_RIGHTBRACKET:
        a->setting = (QuiltSetting)((a->setting + SETTING_COUNT + (key == SDLK_RIGHTBRACKET ? 1 : -1)) %
                                    SETTING_COUNT);
        break;
    default: break;
    }
    a->dirty = true;
}

static void design_key(App *a, SDL_Keycode key, bool shift)
{
    const Quilt *q = current_quilt(a);
    bool whole = !a->brush && (a->selected_layer == APPLIQUE ? q->applique : q->foundation);
    if (whole && (key == SDLK_LEFTBRACKET || key == SDLK_RIGHTBRACKET || key == SDLK_LEFT || key == SDLK_RIGHT ||
                  key == SDLK_UP || key == SDLK_DOWN)) {
        int dir = key == SDLK_LEFT || key == SDLK_LEFTBRACKET || key == SDLK_UP ? -1 : 1;
        a->whole_piece = next_whole(q, a->selected_layer, a->whole_piece, dir);
        a->cursor = true;
        a->dirty = true;
        return;
    }
    switch (key) {
    case SDLK_W: if (q->applique) click_control(a, EDIT_LAYER); break;
    case SDLK_ESCAPE: click_control(a, FINISH_PLACING); break;
    case SDLK_M: click_control(a, SET_SIZE); break;
    case SDLK_R: click_control(a, shift ? ROW_LESS : ROW_MORE); break;
    case SDLK_C: click_control(a, shift ? COL_LESS : COL_MORE); break;
    case SDLK_MINUS: click_control(a, PATCH_LESS); break;
    case SDLK_PLUS: case SDLK_EQUALS: click_control(a, PATCH_MORE); break;
    case SDLK_SPACE: case SDLK_RETURN: a->cursor = true; place(a); break;
    case SDLK_LEFTBRACKET: case SDLK_RIGHTBRACKET: {
        int delta = key == SDLK_RIGHTBRACKET ? 1 : -1;
        int count = block_pattern(&q->cell[a->row][a->col])->count;
        if (a->brush) a->turn = 1 + (a->turn - 1 + 4 + delta) % 4;
        else a->patch = (a->patch + count + delta) % count;
        a->cursor = true;
        a->dirty = true;
        break;
    }
    case SDLK_LEFT: case SDLK_RIGHT: case SDLK_UP: case SDLK_DOWN:
        a->col = SDL_clamp(a->col + (key == SDLK_RIGHT) - (key == SDLK_LEFT), 0, q->cols - 1);
        a->row = SDL_clamp(a->row + (key == SDLK_DOWN) - (key == SDLK_UP), 0, q->rows - 1);
        a->patch = 0;
        a->cursor = true;
        a->dirty = true;
        break;
    default: break;
    }
}

static void keyboard(App *a, SDL_Keycode key, SDL_Keymod mod)
{
    if (a->editor) { editor_key(a, key, mod); return; }
    bool system = (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI | SDL_KMOD_ALT)) != 0;
    bool shift = (mod & SDL_KMOD_SHIFT) != 0;
    if (a->language_menu) {
        if (system) return;
        if (key >= SDLK_1 && key <= SDLK_3) click_control(a, (Control)(LANGUAGE_EN + key - SDLK_1));
        else if (key == SDLK_L || key == SDLK_ESCAPE || key == SDLK_TAB) click_control(a, LANGUAGE_MENU);
        return;
    }
    if (key == SDLK_TAB) { click_control(a, a->screen == DESIGN ? SHEET : DESIGN); return; }
    if (key == SDLK_Z && (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
        click_control(a, shift ? REDO : UNDO);
        return;
    }
    if (key == SDLK_Y && (mod & SDL_KMOD_CTRL)) { click_control(a, REDO); return; }
    if (system) return;
    switch (key) {
    case SDLK_S: persist(a); a->dirty = true; return;
    case SDLK_L: click_control(a, LANGUAGE_MENU); return;
    case SDLK_U: click_control(a, a->prefs.unit == INCHES ? UNIT_CM : UNIT_IN); return;
    case SDLK_P: click_control(a, a->screen == LIBRARY ? DESIGN : LIBRARY); return;
    case SDLK_F: click_control(a, a->screen == COLORS ? DESIGN : COLORS); return;
    default: break;
    }
    if (a->screen == LIBRARY) { library_key(a, key); return; }
    if (key == SDLK_COMMA || key == SDLK_PERIOD) {
        click_control(a, key == SDLK_COMMA ? FABRIC_PREV : FABRIC_NEXT);
        return;
    }
    if (key == SDLK_N && a->screen != SHEET) {
        click_control(a, ADD_SWATCH);
        return;
    }
    if (a->screen == SHEET) {
        if (key == SDLK_RETURN || key == SDLK_SPACE) click_control(a, SHARE);
        else if (key == SDLK_D) click_control(a, EXPORT_PDF);
        return;
    }
    if (key >= SDLK_1 && key <= SDLK_5) click_control(a, (Control)(SWATCH_A + key - SDLK_1));
    else if (a->screen == COLORS) {
        if (key == SDLK_LEFT || key == SDLK_PAGEUP) click_control(a, PREV_PAGE);
        else if (key == SDLK_RIGHT || key == SDLK_PAGEDOWN) click_control(a, NEXT_PAGE);
        else if (key == SDLK_ESCAPE) click_control(a, DESIGN);
    } else design_key(a, key, shift);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    App *a = SDL_calloc(1, sizeof *a);
    *appstate = a;
    if (!a) return SDL_APP_FAILURE;
#ifndef __EMSCRIPTEN__
    char *dir = SDL_GetPrefPath("Patchwork", "Planner");
    if (dir) SDL_asprintf(&a->settings_path, "%ssettings", dir);
    if (argc <= 1 && dir) SDL_asprintf(&a->path, "%squilt.pw", dir);
    SDL_free(dir);
    a->prefs = load_preferences(a->settings_path);
    if (argc == 3 && strcmp(argv[1], "--project") == 0) {
        a->path = SDL_strdup(argv[2]);
    } else if (argc > 1) {
        SDL_Log("%s", tr(a->prefs.language, USAGE));
        return SDL_APP_FAILURE;
    }
#else
    (void)argc;
    (void)argv;
    a->prefs = load_preferences(NULL);
#endif
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "30");
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, render_drivers, SDL_HINT_DEFAULT);
    if (!SDL_Init(SDL_INIT_VIDEO) ||
        !SDL_CreateWindowAndRenderer("Patchwork", WALL_WIDTH, WALL_HEIGHT, SDL_WINDOW_RESIZABLE,
                                     &a->window, &a->renderer) ||
        !SDL_SetRenderLogicalPresentation(a->renderer, WALL_WIDTH, WALL_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX) ||
        !(a->view = open_view(a->renderer, SDL_GetBasePath()))) {
        SDL_Log(tr(a->prefs.language, OPEN_FAILED), SDL_GetError());
        return SDL_APP_FAILURE;
    }

    a->history.steps[0] = garden_path();
    LoadResult load = load_quilt(a->path, &a->history.steps[0]);
    a->protected_plan = load == STORE_BAD;
    a->status = a->protected_plan ? PROTECTED_PLAN : load == STORE_OK ? SAVED : EDITS_SAVE;
    a->fabric = 1;
    a->whole_choice = HEX_FIELD;
    a->whole_piece = -1;
    a->selected_layer = a->history.steps[0].applique ? APPLIQUE : FOUNDATION;
    a->dirty = true;
    printf("Patchwork renderer: %s\n", SDL_GetRendererName(a->renderer));
    fflush(stdout);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *e)
{
    App *a = appstate;
    switch (e->type) {
    case SDL_EVENT_QUIT: case SDL_EVENT_WINDOW_CLOSE_REQUESTED: return SDL_APP_SUCCESS;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e->button.button == SDL_BUTTON_LEFT) {
            float x, y;
            if (SDL_RenderCoordinatesFromWindow(a->renderer, e->button.x, e->button.y, &x, &y)) pointer(a, x, y);
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (a->editor == EDIT_FABRIC && (e->motion.state & SDL_BUTTON_LMASK)) {
            SDL_FPoint p;
            if (SDL_RenderCoordinatesFromWindow(a->renderer, e->motion.x, e->motion.y, &p.x, &p.y))
                adjust_slider(a, p);
        }
        break;
    case SDL_EVENT_TEXT_INPUT: if (a->editor) type_value(a, e->text.text); break;
    case SDL_EVENT_KEY_DOWN: keyboard(a, e->key.key, e->key.mod); break;
    case SDL_EVENT_WINDOW_EXPOSED: case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_RENDER_TARGETS_RESET: a->dirty = true; break;
    case SDL_EVENT_USER:
        a->pdf_pending = false;
        if (e->user.code == PDF_EXPORTED || e->user.code == EXPORT_FAILED) {
            a->status = (Text)e->user.code;
            a->dirty = true;
        }
        break;
    default: break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    App *a = appstate;
    if (a->dirty) { draw_app(a); a->dirty = false; }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    App *a = appstate;
    if (!a) return;
    close_view(a->view);
    SDL_DestroyRenderer(a->renderer);
    SDL_DestroyWindow(a->window);
    SDL_free(a->settings_path);
    SDL_free(a->path);
    SDL_free(a);
}
