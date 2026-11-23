#ifndef PATCHWORK_I18N_H
#define PATCHWORK_I18N_H

#include <stdbool.h>

typedef enum { ENGLISH, GERMAN, POLISH, LANGUAGE_COUNT } Language;
typedef enum { INCHES, CENTIMETRES, UNIT_COUNT } Unit;
typedef struct { Language language; Unit unit; } Preferences;

typedef enum {
#define TEXT(key, en, de, pl) key,
#include "strings.def"
#undef TEXT
    TEXT_COUNT
} Text;

enum { NUMBER_BYTES = 24 };
extern const char *const language_tag[LANGUAGE_COUNT];
extern const char *const language_name[LANGUAGE_COUNT];
extern const char *const unit_tag[UNIT_COUNT];
const char *tr(Language language, Text key);
Language language_from_tag(const char *tag);
bool parse_preferences(const char *data, Preferences *preferences);
const char *unit_label(Preferences preferences);
void decimal(char out[NUMBER_BYTES], Language language, double value, int precision, bool trim);

#endif
