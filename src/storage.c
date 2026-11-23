#include "storage.h"
#include "pdf.h"
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
extern int browser_load(char *data, int capacity);
extern int browser_save(const char *data);
extern void browser_load_prefs(char *data);
extern int browser_save_prefs(const char *data);
extern int browser_download(const void *data, int size, const char *filename, int pdf);
#else
#ifdef _WIN32
#include <process.h>
#define process_id _getpid
#else
#include <unistd.h>
#define process_id getpid
#endif

static bool save_file(const char *path, const void *data, size_t size)
{
    char *temp = NULL;
    if (!path || SDL_asprintf(&temp, "%s.%ld.tmp", path, (long)process_id()) < 0) return false;
    bool ok = SDL_SaveFile(temp, data, size) && SDL_RenamePath(temp, path);
    if (!ok) SDL_RemovePath(temp);
    SDL_free(temp);
    return ok;
}
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
    return save_file(path, data, size);
#endif
}

Preferences load_preferences(const char *path)
{
    char data[6] = {0};
    Preferences prefs;
#ifdef __EMSCRIPTEN__
    (void)path;
    browser_load_prefs(data);
    if (parse_preferences(data, &prefs)) return prefs;
#else
    SDL_IOStream *file = path ? SDL_IOFromFile(path, "rb") : NULL;
    if (file) {
        size_t read = SDL_ReadIO(file, data, sizeof data);
        if (SDL_CloseIO(file) && read == 5 && parse_preferences(data, &prefs)) return prefs;
    }
    SDL_Locale **locales = SDL_GetPreferredLocales(NULL);
    Language language = LANGUAGE_COUNT;
    for (int i = 0; locales && locales[i] && language == LANGUAGE_COUNT; ++i)
        language = language_from_tag(locales[i]->language);
    SDL_free(locales);
    if (language != LANGUAGE_COUNT) return (Preferences){language, language == ENGLISH ? INCHES : CENTIMETRES};
#endif
    return (Preferences){ENGLISH, INCHES};
}

bool save_preferences(const char *path, Preferences prefs)
{
    char data[6];
    snprintf(data, sizeof data, "%s %s", language_tag[prefs.language], unit_tag[prefs.unit]);
#ifdef __EMSCRIPTEN__
    (void)path;
    return browser_save_prefs(data) != 0;
#else
    return save_file(path, data, strlen(data));
#endif
}

bool share_sheet(const Quilt *q, Preferences prefs)
{
    char *text = SDL_malloc(SHEET_BYTES);
    if (!text) return false;
    bool ok = cutting_sheet(q, prefs, text);
#ifdef __EMSCRIPTEN__
    ok = ok && browser_download(text, (int)strlen(text), tr(prefs.language, TEXT_FILENAME), 0);
#else
    ok = ok && SDL_SetClipboardText(text);
#endif
    SDL_free(text);
    return ok;
}

#ifndef __EMSCRIPTEN__
typedef struct {
    SDL_IOStream *pdf;
    SDL_DialogFileFilter filter;
    char *suggestion;
} SaveDialog;

static void SDLCALL save_pdf_result(void *data, const char *const *paths, int selected)
{
    (void)selected;
    SaveDialog *dialog = data;
    SDL_Event event = {.type = SDL_EVENT_USER};
    if (!paths) event.user.code = EXPORT_FAILED;
    else if (*paths) {
        bool ok = save_file(*paths, pdf_data(dialog->pdf), (size_t)SDL_GetIOSize(dialog->pdf));
        event.user.code = ok ? PDF_EXPORTED : EXPORT_FAILED;
    }
    SDL_CloseIO(dialog->pdf);
    SDL_free(dialog->suggestion);
    SDL_free(dialog);
    SDL_PushEvent(&event);
}
#endif

bool share_pdf(const Quilt *q, Preferences prefs, SDL_Window *window)
{
    SDL_IOStream *pdf = make_pdf(q, prefs);
    if (!pdf) return false;
    const char *filename = tr(prefs.language, PDF_FILENAME);
#ifdef __EMSCRIPTEN__
    (void)window;
    bool ok = browser_download(pdf_data(pdf), (int)SDL_GetIOSize(pdf), filename, 1) != 0;
    SDL_CloseIO(pdf);
    return ok;
#else
    SaveDialog *dialog = SDL_calloc(1, sizeof *dialog);
    if (!dialog) {
        SDL_CloseIO(pdf);
        return false;
    }
    dialog->pdf = pdf;
    dialog->filter = (SDL_DialogFileFilter){tr(prefs.language, PDF_FILETYPE), "pdf"};
    const char *folder = SDL_GetUserFolder(SDL_FOLDER_DOWNLOADS);
    if (folder) SDL_asprintf(&dialog->suggestion, "%s%s", folder, filename);
    SDL_ShowSaveFileDialog(save_pdf_result, dialog, window, &dialog->filter, 1, dialog->suggestion);
    return true;
#endif
}
