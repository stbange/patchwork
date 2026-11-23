#include "i18n.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

const char *const language_tag[LANGUAGE_COUNT] = {"en", "de", "pl"};
const char *const language_name[LANGUAGE_COUNT] = {"English", "Deutsch", "Polski"};
const char *const unit_tag[UNIT_COUNT] = {"in", "cm"};

static const char *const translations[TEXT_COUNT][LANGUAGE_COUNT] = {
#define TEXT(key, en, de, pl) [key] = {en, de, pl},
#include "strings.def"
#undef TEXT
};

const char *tr(Language language, Text key)
{
    return translations[key][language];
}

Language language_from_tag(const char *tag)
{
    if (tag && tag[0] && tag[1] && (!tag[2] || tag[2] == '-' || tag[2] == '_')) {
        for (int i = 0; i < LANGUAGE_COUNT; ++i)
            if (SDL_strncasecmp(tag, language_tag[i], 2) == 0) return (Language)i;
    }
    return LANGUAGE_COUNT;
}

bool parse_preferences(const char *data, Preferences *preferences)
{
    if (strlen(data) != 5 || data[2] != ' ') return false;
    char tag[3] = {data[0], data[1], 0};
    Language language = language_from_tag(tag);
    if (language == LANGUAGE_COUNT) return false;
    for (int i = 0; i < UNIT_COUNT; ++i) {
        if (strcmp(data + 3, unit_tag[i]) == 0) {
            *preferences = (Preferences){language, (Unit)i};
            return true;
        }
    }
    return false;
}

const char *unit_label(Preferences prefs)
{
    static const char *const inches[LANGUAGE_COUNT] = {"in", "Zoll", "cal"};
    return prefs.unit == CENTIMETRES ? "cm" : inches[prefs.language];
}

void decimal(char out[NUMBER_BYTES], Language language, double value, int precision, bool trim)
{
    snprintf(out, NUMBER_BYTES, "%.*f", precision, value);
    char *point = strchr(out, '.');
    if (!point) return;
    if (trim) {
        char *end = out + strlen(out);
        while (end > point + 1 && end[-1] == '0') *--end = 0;
        if (end == point + 1) *point = 0;
    }
    if (*point && language != ENGLISH) *point = ',';
}
