#define SDL_MAIN_USE_CALLBACKS 1
#define SDL_MAIN_NOIMPL
#include <SDL3/SDL_main.h>
#undef main
#include "app.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

static void test_quilt(void)
{
    Quilt q = garden_path();
    int counts[FABRICS];
    count_pieces(&q, counts);
    CHECK(memcmp(counts, (int[]){8, 4, 4, 8, 1}, sizeof counts) == 0);
    CHECK(!paint(&q, 0, 0, 2) && !paint(&q, 0, 0, FABRICS));
    CHECK(!paint(&q, -1, 0, 1) && !paint(&q, 1, q.cols, 1));
    CHECK(paint(&q, 2, 2, 1) && q.cell[2][2] == 1);
    count_pieces(&q, counts);
    CHECK(counts[1] == 5 && counts[4] == 0 && counts[0] == 8);
    CHECK(!resize_quilt(&q, 5, 5, 4));
    CHECK(!resize_quilt(&q, 2, 5, 4) && !resize_quilt(&q, 5, 10, 4));
    CHECK(!resize_quilt(&q, 5, 5, 1) && !resize_quilt(&q, 5, 5, MAX_PATCH + 1));
    CHECK(resize_quilt(&q, 3, 7, 8) && q.cell[2][2] == 1 && q.cell[2][5] == 0);
    CHECK(resize_quilt(&q, 5, 5, 2) && q.cell[4][0] == 0);
}

static void test_history(void)
{
    History h = {0};
    Quilt q = garden_path();
    h.steps[0] = q;
    CHECK(!record(&h, &q) && !travel(&h, -1) && !travel(&h, 1));
    CHECK(resize_quilt(&q, 3, 3, 4) && record(&h, &q));
    CHECK(travel(&h, -1) && h.steps[h.at].rows == 5 && h.steps[h.at].cell[4][4] == 2);
    CHECK(travel(&h, 1) && h.steps[h.at].rows == 3);
    CHECK(travel(&h, -1) && !travel(&h, 2));
    q = h.steps[h.at];
    CHECK(paint(&q, 0, 0, 4) && record(&h, &q) && !travel(&h, 1));
    for (int i = 0; i < 2 * HISTORY; ++i) {
        CHECK(paint(&q, 0, 0, i % 2) && record(&h, &q));
    }
    int count = 0;
    while (travel(&h, -1)) ++count;
    CHECK(count == HISTORY - 1);
}

static void test_format(void)
{
    Quilt original = garden_path(), decoded = {0};
    char data[PLAN_BYTES], text[SHEET_BYTES];
    size_t size = encode_plan(&original, data);
    CHECK(strcmp(data, "PATCHWORK 1\n5 5 4\nCADAC\nADBDA\nDBEBD\nADBDA\nCADAC\n") == 0);
    CHECK(decode_plan(data, size, &decoded) && memcmp(&decoded, &original, sizeof decoded) == 0);
    for (size_t i = 0; i < size; ++i) CHECK(!decode_plan(data, i, &decoded));
    CHECK(!decode_plan(data, size + 1, &decoded));
    const int offsets[] = {10, 12, 13, 14, 16, 17, 18, 23};
    for (size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
        char *c = &data[offsets[i]], saved = *c;
        *c = '!';
        CHECK(!decode_plan(data, size, &decoded) && memcmp(&decoded, &original, sizeof decoded) == 0);
        *c = saved;
    }
    cutting_sheet(&original, text);
    CHECK(strstr(text, "20 x 20 in (50.8 x 50.8 cm)") && strstr(text, "25 squares at 4.5 x 4.5 in"));
    CHECK(strstr(text, "Honey check     1") && strstr(text, "C A D A C\nA D B D A\n"));
    CHECK(resize_quilt(&original, MAX_SIDE, MAX_SIDE, MAX_PATCH));
    size = encode_plan(&original, data);
    CHECK(size < PLAN_BYTES && decode_plan(data, size, &decoded));
    cutting_sheet(&decoded, text);
    CHECK(strstr(text, "81 squares at 8.5 x 8.5 in") && strlen(text) < SHEET_BYTES);
}

static void test_storage(const char *path)
{
    Quilt q = garden_path(), other = {0};
    CHECK(load_quilt(NULL, &other) == STORE_BAD && !save_quilt(NULL, &q));
    CHECK(load_quilt(path, &other) == STORE_MISSING);
    CHECK(save_quilt(path, &q) && load_quilt(path, &other) == STORE_OK);
    CHECK(memcmp(&q, &other, sizeof q) == 0);
    CHECK(paint(&q, 0, 0, 1) && save_quilt(path, &q));
    CHECK(load_quilt(path, &other) == STORE_OK && other.cell[0][0] == 1);
    CHECK(SDL_SaveFile(path, "", 0) && load_quilt(path, &other) == STORE_BAD);
    char too_big[PLAN_BYTES] = {0};
    CHECK(SDL_SaveFile(path, too_big, sizeof too_big) && load_quilt(path, &other) == STORE_BAD);
    CHECK(SDL_RemovePath(path));
}

static void key(App *a, SDL_Keycode code, SDL_Keymod mod)
{
    SDL_Event e = {0};
    e.type = SDL_EVENT_KEY_DOWN; e.key.key = code; e.key.mod = mod;
    CHECK(SDL_AppEvent(a, &e) == SDL_APP_CONTINUE);
}

static void click(App *a, float x, float y)
{
    SDL_Event e = {0};
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN; e.button.button = SDL_BUTTON_LEFT;
    CHECK(SDL_RenderCoordinatesToWindow(a->renderer, x, y, &e.button.x, &e.button.y));
    CHECK(SDL_AppEvent(a, &e) == SDL_APP_CONTINUE);
}

static void button(App *a, Control id)
{
    SDL_FRect b = control_box[id];
    click(a, b.x + b.w / 2, b.y + b.h / 2);
}

static App *start(char *path)
{
    void *state = NULL;
    char *args[] = {"patchwork", "--project", path, NULL};
    CHECK(SDL_AppInit(&state, 3, args) == SDL_APP_CONTINUE);
    return state;
}

static void test_inputs(App *a)
{
    key(a, SDLK_5, 0);
    CHECK(a->fabric == 4);
    key(a, SDLK_LEFT, 0); key(a, SDLK_UP, 0); key(a, SDLK_RIGHT, 0); key(a, SDLK_DOWN, 0);
    CHECK(a->row == 1 && a->col == 1);
    key(a, SDLK_SPACE, 0);
    CHECK(a->history.steps[a->history.at].cell[1][1] == 4);
    key(a, SDLK_Z, SDL_KMOD_CTRL); CHECK(a->history.at == 0);
    key(a, SDLK_Y, SDL_KMOD_CTRL); CHECK(a->history.at == 1);
    button(a, UNDO); key(a, SDLK_Z, SDL_KMOD_CTRL | SDL_KMOD_SHIFT);
    CHECK(a->history.at == 1);
    key(a, SDLK_R, 0); key(a, SDLK_C, 0); key(a, SDLK_EQUALS, 0);
    Quilt q = a->history.steps[a->history.at];
    CHECK(q.rows == 6 && q.cols == 6 && q.inches == 5);
    key(a, SDLK_R, SDL_KMOD_SHIFT); key(a, SDLK_C, SDL_KMOD_SHIFT); key(a, SDLK_MINUS, 0);
    key(a, SDLK_UNKNOWN, 0); key(a, SDLK_C, SDL_KMOD_CTRL);
    q = a->history.steps[a->history.at];
    CHECK(q.rows == 5 && q.cols == 5 && q.inches == 4);
    button(a, SWATCH_B);
    SDL_FRect b = quilt_box(&q, design_mat);
    click(a, b.x + b.w / 2, b.y + b.h / 2);
    CHECK(a->row == 2 && a->col == 2 && a->history.steps[a->history.at].cell[2][2] == 1);
    click(a, b.x + b.w, b.y + b.h); click(a, 0, 0);
    CHECK(a->row == 2 && a->col == 2);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE && !a->dirty);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    button(a, SHEET); CHECK(a->sheet);
    key(a, SDLK_5, 0); CHECK(a->fabric == 1);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    key(a, SDLK_RETURN, 0);
    char *sheet = SDL_GetClipboardText();
    CHECK(sheet && strstr(sheet, "25 squares at 4.5 x 4.5 in"));
    SDL_free(sheet);
    button(a, SHARE);
    CHECK(strstr(a->status, "copied"));
    key(a, SDLK_TAB, 0); CHECK(!a->sheet);
    key(a, SDLK_S, 0); CHECK(strstr(a->status, "Saved"));
    SDL_Event e = {0}; e.type = SDL_EVENT_WINDOW_EXPOSED;
    CHECK(SDL_AppEvent(a, &e) == SDL_APP_CONTINUE && a->dirty);
    e.type = SDL_EVENT_MOUSE_MOTION; CHECK(SDL_AppEvent(a, &e) == SDL_APP_CONTINUE);
    e.type = SDL_EVENT_QUIT; CHECK(SDL_AppEvent(a, &e) == SDL_APP_SUCCESS);
}

static void test_drawing(App *a)
{
    SDL_Surface *surface = SDL_CreateSurface(WALL_WIDTH, WALL_HEIGHT, SDL_PIXELFORMAT_RGBA32);
    CHECK(surface);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    CHECK(renderer && !open_view(renderer, "missing-assets/"));
    App frame = *a;
    frame.renderer = renderer;
    frame.view = open_view(renderer, SDL_GetBasePath());
    CHECK(frame.view);
    frame.cursor = false;
    SDL_FRect b = quilt_box(&frame.history.steps[frame.history.at], design_mat);
    int x = (int)b.x + 7, y = (int)b.y + 9;
    Uint8 r, g, blue, alpha;
    draw_app(&frame);
    CHECK(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &blue, &alpha) && g > r);
    frame.history.steps[frame.history.at].cell[0][0] = 1;
    draw_app(&frame);
    CHECK(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &blue, &alpha) && r > g);
    frame.sheet = true;
    draw_app(&frame);
    CHECK(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &blue, &alpha) && r > 200);
    close_view(frame.view);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
}

int main(void)
{
    SDL_SetMainReady();
    test_quilt(); test_history(); test_format();
    char *dir = SDL_GetPrefPath("PatchworkTests", "Unit");
    CHECK(dir);
    char *path, *bad;
    CHECK(SDL_asprintf(&path, "%sunit-%llu.pw", dir, (unsigned long long)SDL_GetPerformanceCounter()) > 0);
    CHECK(SDL_asprintf(&bad, "%s/missing/plan", path) > 0);
    test_storage(path);
    App *a = start(path);
    CHECK(strcmp(SDL_GetRendererName(a->renderer), "software") == 0);
    test_inputs(a); test_drawing(a);
    SDL_AppQuit(a, SDL_APP_SUCCESS);
    a = start(path); CHECK(a->history.steps[0].cell[2][2] == 1);
    SDL_AppQuit(a, SDL_APP_SUCCESS);
    CHECK(SDL_SaveFile(path, "broken", 6));
    a = start(path); key(a, SDLK_SPACE, 0);
    CHECK(a->protected_plan && strstr(a->status, "preserved"));
    SDL_AppQuit(a, SDL_APP_SUCCESS);
    size_t size;
    char *original = SDL_LoadFile(path, &size);
    CHECK(original && size == 6 && memcmp(original, "broken", 6) == 0);
    SDL_free(original);
    a = start(bad); key(a, SDLK_SPACE, 0);
    CHECK(strstr(a->status, "Saving failed"));
    SDL_AppQuit(a, SDL_APP_SUCCESS);
    void *state = NULL;
    char *args[] = {"patchwork", "--unknown", NULL};
    CHECK(SDL_AppInit(&state, 2, args) == SDL_APP_FAILURE);
    SDL_AppQuit(state, SDL_APP_FAILURE);
    CHECK(SDL_AppInit(&state, 1, args) == SDL_APP_CONTINUE);
    SDL_AppQuit(state, SDL_APP_SUCCESS);
    SDL_AppQuit(NULL, SDL_APP_FAILURE);
    CHECK(SDL_RemovePath(path));
    SDL_free(bad); SDL_free(path); SDL_free(dir);
    SDL_Quit();
    puts("Quilt, history, format, storage, input and rendering checks passed.");
    return 0;
}
