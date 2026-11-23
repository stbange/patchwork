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

static const char saved[] = "Saved on this device";
static const char protected_msg[] = "Could not read the saved plan. Original preserved.";
static const char unsaved[] = "Saving failed. Changes are in memory. Press S to retry.";

static void persist(App *a)
{
    if (a->protected_plan) {
        a->status = protected_msg;
        return;
    }
    const Quilt *q = &a->history.steps[a->history.at];
    a->status = save_quilt(a->path, q) ? saved : unsaved;
}

static void click_control(App *a, Control id)
{
    if (id <= LIBRARY) {
        a->screen = id;
    } else if (id == UNDO || id == REDO) {
        if (travel(&a->history, id == UNDO ? -1 : 1)) persist(a);
    } else if (id >= SWATCH_A && id <= SWATCH_E) {
        a->fabric = id - SWATCH_A;
    } else if (id == SHARE) {
        if (!share_sheet(&a->history.steps[a->history.at])) a->status = "Could not take the cutting sheet.";
#ifdef __EMSCRIPTEN__
        else a->status = "Cutting sheet downloaded";
#else
        else a->status = "Cutting sheet copied. Paste it into a document to print.";
#endif
    } else if (id >= FILTER_ALL && id <= FILTER_MODERN) {
        PatternFilter filter = (PatternFilter)(id - FILTER_ALL);
        if (a->filter != filter) a->chosen = 0;
        a->filter = filter;
    } else if (id == PREV_PAGE || id == NEXT_PAGE) {
        int ids[PATTERN_COUNT];
        int count = catalog_ids(a->filter, ids);
        int start = a->chosen / CARDS_PER_PAGE * CARDS_PER_PAGE;
        int next = start + (id == NEXT_PAGE ? CARDS_PER_PAGE : -CARDS_PER_PAGE);
        if (next >= 0 && next < count) a->chosen = next;
    } else if (id == APPLY) {
        int ids[PATTERN_COUNT];
        catalog_ids(a->filter, ids);
        Quilt q = a->history.steps[a->history.at];
        if (arrange(&q, ids[a->chosen], a->setting) && record(&a->history, &q)) persist(a);
        a->screen = DESIGN;
        a->patch = 0;
        a->cursor = false;
    } else {
        Quilt q = a->history.steps[a->history.at];
        int change = (id - COL_LESS) % 2 ? 1 : -1;
        int cols = q.cols + (id <= COL_MORE ? change : 0);
        int rows = q.rows + (id == ROW_LESS || id == ROW_MORE ? change : 0);
        int inches = q.inches + (id >= PATCH_LESS ? change : 0);
        if (resize_quilt(&q, rows, cols, inches) && record(&a->history, &q)) persist(a);
    }
    const Quilt *q = &a->history.steps[a->history.at];
    a->row = SDL_min(a->row, q->rows - 1);
    a->col = SDL_min(a->col, q->cols - 1);
    a->patch = SDL_min(a->patch, block_pattern(q, &q->cell[a->row][a->col])->count - 1);
    a->dirty = true;
}

static void place(App *a)
{
    Quilt q = a->history.steps[a->history.at];
    if (paint(&q, a->row, a->col, a->patch, a->fabric) && record(&a->history, &q)) persist(a);
    a->dirty = true;
}

static void pointer(App *a, float x, float y)
{
    SDL_FPoint point = {x, y};
    for (Control id = DESIGN; id < CONTROL_COUNT; ++id) {
        bool visible = id <= REDO ||
                       (a->screen == DESIGN && id >= SWATCH_A && id <= PATCH_MORE) ||
                       (a->screen == SHEET && id == SHARE) ||
                       (a->screen == LIBRARY && id >= FILTER_ALL);
        if (visible && SDL_PointInRectFloat(&point, &control_box[id])) {
            click_control(a, id);
            return;
        }
    }
    if (a->screen == LIBRARY) {
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
        return;
    }
    if (a->screen != DESIGN) return;
    const Quilt *q = &a->history.steps[a->history.at];
    SDL_FRect board = quilt_box(q, design_mat);
    if (x >= board.x && x < board.x + board.w && y >= board.y && y < board.y + board.h) {
        float side = board.w / q->cols;
        float u = (x - board.x) / side, v = (y - board.y) / side;
        int col = SDL_min((int)u, q->cols - 1), row = SDL_min((int)v, q->rows - 1);
        const Block *block = &q->cell[row][col];
        int patch = hit_patch(block_pattern(q, block), block->turn,
                              (u - col) * BLOCK_UNITS, (v - row) * BLOCK_UNITS);
        if (patch < 0) return;
        a->row = row;
        a->col = col;
        a->patch = patch;
        a->cursor = true;
        place(a);
    }
}

static void library_key(App *a, SDL_Keycode key)
{
    int ids[PATTERN_COUNT];
    int count = catalog_ids(a->filter, ids);
    switch (key) {
    case SDLK_ESCAPE: click_control(a, DESIGN); break;
    case SDLK_RETURN: case SDLK_SPACE: click_control(a, APPLY); break;
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

static void keyboard(App *a, SDL_Keycode key, SDL_Keymod mod)
{
    bool shift = (mod & SDL_KMOD_SHIFT) != 0;
    if (key == SDLK_TAB) { click_control(a, a->screen == DESIGN ? SHEET : DESIGN); return; }
    if (key == SDLK_Z && (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
        click_control(a, shift ? REDO : UNDO);
        return;
    }
    if (key == SDLK_Y && (mod & SDL_KMOD_CTRL)) { click_control(a, REDO); return; }
    if (key == SDLK_S) { persist(a); a->dirty = true; return; }
    if (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI | SDL_KMOD_ALT)) return;
    if (key == SDLK_P) { click_control(a, a->screen == LIBRARY ? DESIGN : LIBRARY); return; }
    if (a->screen == LIBRARY) { library_key(a, key); return; }
    if (a->screen == SHEET) {
        if (key == SDLK_RETURN || key == SDLK_SPACE) click_control(a, SHARE);
        return;
    }
    const Quilt *q = &a->history.steps[a->history.at];
    if (key >= SDLK_1 && key <= SDLK_5) click_control(a, (Control)(SWATCH_A + key - SDLK_1));
    else switch (key) {
    case SDLK_R: click_control(a, shift ? ROW_LESS : ROW_MORE); break;
    case SDLK_C: click_control(a, shift ? COL_LESS : COL_MORE); break;
    case SDLK_MINUS: click_control(a, PATCH_LESS); break;
    case SDLK_PLUS: case SDLK_EQUALS: click_control(a, PATCH_MORE); break;
    case SDLK_SPACE: case SDLK_RETURN: a->cursor = true; place(a); break;
    case SDLK_LEFTBRACKET: case SDLK_RIGHTBRACKET: {
        int count = block_pattern(q, &q->cell[a->row][a->col])->count;
        a->patch = (a->patch + count + (key == SDLK_RIGHTBRACKET ? 1 : -1)) % count;
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

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    App *a = SDL_calloc(1, sizeof *a);
    *appstate = a;
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "30");
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, render_drivers, SDL_HINT_DEFAULT);
    if (!a || !SDL_Init(SDL_INIT_VIDEO) ||
        !SDL_CreateWindowAndRenderer("Patchwork", WALL_WIDTH, WALL_HEIGHT, SDL_WINDOW_RESIZABLE,
                                     &a->window, &a->renderer) ||
        !SDL_SetRenderLogicalPresentation(a->renderer, WALL_WIDTH, WALL_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX) ||
        !(a->view = open_view(a->renderer, SDL_GetBasePath()))) {
        SDL_Log("Could not open the design wall: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

#ifndef __EMSCRIPTEN__
    if (argc == 3 && strcmp(argv[1], "--project") == 0) {
        a->path = SDL_strdup(argv[2]);
    } else if (argc <= 1) {
        char *dir = SDL_GetPrefPath("Patchwork", "Planner");
        if (dir) SDL_asprintf(&a->path, "%squilt.pw", dir);
        SDL_free(dir);
    } else {
        SDL_Log("Usage: patchwork [--project PATH]");
        return SDL_APP_FAILURE;
    }
#else
    (void)argc;
    (void)argv;
#endif
    a->history.steps[0] = garden_path();
    LoadResult load = load_quilt(a->path, &a->history.steps[0]);
    a->protected_plan = load == STORE_BAD;
    a->status = a->protected_plan ? protected_msg : load == STORE_OK ? saved : "Edits save on this device";
    a->fabric = 1;
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
    case SDL_EVENT_KEY_DOWN: keyboard(a, e->key.key, e->key.mod); break;
    case SDL_EVENT_WINDOW_EXPOSED: case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_RENDER_TARGETS_RESET: a->dirty = true; break;
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
    SDL_free(a->path);
    SDL_free(a);
}
