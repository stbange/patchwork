#include "storage.h"
#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
extern int browser_load(char *data, int capacity);
extern int browser_save(const char *data);
extern int browser_download(const char *text);
#else
#ifdef _WIN32
#include <process.h>
#define process_id _getpid
#else
#include <unistd.h>
#define process_id getpid
#endif
#endif

LoadResult load_quilt(const char *path, Quilt *q)
{
    char data[PLAN_BYTES];
#ifdef __EMSCRIPTEN__
    (void)path;
    int length = browser_load(data, PLAN_BYTES);
    if (length == 0) return STORE_MISSING;
    if (length < 0) return STORE_BAD;
    size_t size = (size_t)length;
#else
    if (!path) return STORE_BAD;
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) return STORE_MISSING;
    if (info.type != SDL_PATHTYPE_FILE || !info.size || info.size >= PLAN_BYTES) return STORE_BAD;
    SDL_IOStream *file = SDL_IOFromFile(path, "rb");
    if (!file) return STORE_BAD;
    size_t size = SDL_ReadIO(file, data, sizeof data);
    bool complete = size == info.size && SDL_GetIOStatus(file) != SDL_IO_STATUS_ERROR;
    if (!SDL_CloseIO(file) || !complete) return STORE_BAD;
#endif
    return decode_plan(data, size, q) ? STORE_OK : STORE_BAD;
}

bool save_quilt(const char *path, const Quilt *q)
{
    char data[PLAN_BYTES];
    size_t size = encode_plan(q, data);
#ifdef __EMSCRIPTEN__
    (void)size;
    (void)path;
    return browser_save(data) != 0;
#else
    char *temp = NULL;
    if (!path || SDL_asprintf(&temp, "%s.%ld.tmp", path, (long)process_id()) < 0) return false;
    bool ok = SDL_SaveFile(temp, data, size) && SDL_RenamePath(temp, path);
    if (!ok) SDL_RemovePath(temp);
    SDL_free(temp);
    return ok;
#endif
}

bool share_sheet(const Quilt *q)
{
    char text[SHEET_BYTES];
    cutting_sheet(q, text);
#ifdef __EMSCRIPTEN__
    return browser_download(text) != 0;
#else
    return SDL_SetClipboardText(text);
#endif
}
