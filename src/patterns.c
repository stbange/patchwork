#include "whole.h"

enum { A, B, C, D, E };
#define TRI(x1,y1,x2,y2,x3,y3,f) {3, f, {{x1,y1}, {x2,y2}, {x3,y3}}}
#define QUAD(x1,y1,x2,y2,x3,y3,x4,y4,f) {4, f, {{x1,y1}, {x2,y2}, {x3,y3}, {x4,y4}}}
#define RECT(x,y,w,h,f) QUAD(x,y, (x)+(w),y, (x)+(w),(y)+(h), x,(y)+(h), f)

/* A rectangle split along its backslash or slash diagonal. */
#define BACK(x,y,w,h,upper,lower) \
    TRI(x,y, (x)+(w),y, (x)+(w),(y)+(h), upper), \
    TRI(x,y, (x)+(w),(y)+(h), x,(y)+(h), lower)
#define SLASH(x,y,w,h,upper,lower) \
    TRI(x,y, (x)+(w),y, x,(y)+(h), upper), \
    TRI((x)+(w),y, (x)+(w),(y)+(h), x,(y)+(h), lower)

#define QST(x,y,s,top,right,bottom,left) \
    TRI(x,y, (x)+(s),y, (x)+(s)/2,(y)+(s)/2, top), \
    TRI((x)+(s),y, (x)+(s),(y)+(s), (x)+(s)/2,(y)+(s)/2, right), \
    TRI((x)+(s),(y)+(s), x,(y)+(s), (x)+(s)/2,(y)+(s)/2, bottom), \
    TRI(x,(y)+(s), x,y, (x)+(s)/2,(y)+(s)/2, left)

/* Flying geese units have a 2:1 base-to-height ratio. */
#define G_UP(x,y,s,goose,sky) \
    TRI((x)+(s),y, (x)+2*(s),(y)+(s), x,(y)+(s), goose), \
    TRI(x,y, (x)+(s),y, x,(y)+(s), sky), \
    TRI((x)+(s),y, (x)+2*(s),y, (x)+2*(s),(y)+(s), sky)
#define G_RIGHT(x,y,s,goose,sky) \
    TRI(x,y, (x)+(s),(y)+(s), x,(y)+2*(s), goose), \
    TRI(x,y, (x)+(s),y, (x)+(s),(y)+(s), sky), \
    TRI((x)+(s),(y)+(s), (x)+(s),(y)+2*(s), x,(y)+2*(s), sky)
#define G_DOWN(x,y,s,goose,sky) \
    TRI(x,y, (x)+2*(s),y, (x)+(s),(y)+(s), goose), \
    TRI(x,y, (x)+(s),(y)+(s), x,(y)+(s), sky), \
    TRI((x)+2*(s),y, (x)+2*(s),(y)+(s), (x)+(s),(y)+(s), sky)
#define G_LEFT(x,y,s,goose,sky) \
    TRI((x)+(s),y, (x)+(s),(y)+2*(s), x,(y)+(s), goose), \
    TRI(x,y, (x)+(s),y, x,(y)+(s), sky), \
    TRI(x,(y)+(s), (x)+(s),(y)+2*(s), x,(y)+2*(s), sky)

#define CABIN_RING(x,y,s,light,dark) \
    RECT(x,(y)-3,s,3,light), RECT((x)+(s),(y)-3,3,(s)+3,light), \
    RECT(x,(y)+(s),(s)+3,3,dark), RECT((x)-3,(y)-3,3,(s)+6,dark)
#define STEPS_RING(x,y,s,top,sides) \
    RECT(x,(y)-3,s,3,top), RECT(x,(y)+(s),s,3,top), \
    RECT((x)-3,(y)-3,3,(s)+6,sides), RECT((x)+(s),(y)-3,3,(s)+6,sides)

static const Patch square[] = { RECT(0,0,24,24,A) };
static const Patch nine_patch[] = {
    RECT(0,0,8,8,D), RECT(8,0,8,8,A), RECT(16,0,8,8,D),
    RECT(0,8,8,8,A), RECT(8,8,8,8,D), RECT(16,8,8,8,A),
    RECT(0,16,8,8,D), RECT(8,16,8,8,A), RECT(16,16,8,8,D)
};
static const Patch rail_fence[] = {
    RECT(0,0,4,12,B), RECT(4,0,4,12,A), RECT(8,0,4,12,D),
    RECT(12,0,12,4,B), RECT(12,4,12,4,A), RECT(12,8,12,4,D),
    RECT(0,12,12,4,D), RECT(0,16,12,4,A), RECT(0,20,12,4,B),
    RECT(12,12,4,12,D), RECT(16,12,4,12,A), RECT(20,12,4,12,B)
};
static const Patch log_cabin[] = {
    RECT(9,9,6,6,E), CABIN_RING(9,9,6,A,B), CABIN_RING(6,6,12,C,D),
    CABIN_RING(3,3,18,A,B)
};
static const Patch courthouse_steps[] = {
    RECT(9,9,6,6,B), STEPS_RING(9,9,6,E,D), STEPS_RING(6,6,12,A,C),
    STEPS_RING(3,3,18,E,D)
};
static const Patch pinwheel[] = {
    BACK(0,0,12,12,D,A), SLASH(12,0,12,12,A,D),
    BACK(12,12,12,12,A,D), SLASH(0,12,12,12,D,A)
};
static const Patch broken_dishes[] = {
    BACK(0,0,12,12,B,A), SLASH(12,0,12,12,A,D),
    SLASH(0,12,12,12,D,A), BACK(12,12,12,12,A,B)
};
static const Patch hourglass[] = { QST(0,0,24,B,A,B,A) };
static const Patch flying_geese[] = { G_UP(0,0,12,D,A), G_UP(0,12,12,B,A) };
static const Patch square_in_square[] = {
    QUAD(12,0, 24,12, 12,24, 0,12, B),
    TRI(0,0, 12,0, 0,12, A), TRI(12,0, 24,0, 24,12, C),
    TRI(24,12, 24,24, 12,24, A), TRI(0,12, 12,24, 0,24, C)
};
static const Patch shoo_fly[] = {
    SLASH(0,0,8,8,A,D), RECT(8,0,8,8,A), BACK(16,0,8,8,A,D),
    RECT(0,8,8,8,A), RECT(8,8,8,8,D), RECT(16,8,8,8,A),
    BACK(0,16,8,8,D,A), RECT(8,16,8,8,A), SLASH(16,16,8,8,D,A)
};
static const Patch churn_dash[] = {
    SLASH(0,0,8,8,A,C), RECT(8,0,8,4,A), RECT(8,4,8,4,C), BACK(16,0,8,8,A,C),
    RECT(0,8,4,8,A), RECT(4,8,4,8,C), RECT(8,8,8,8,A),
    RECT(16,8,4,8,C), RECT(20,8,4,8,A),
    BACK(0,16,8,8,C,A), RECT(8,16,8,4,C), RECT(8,20,8,4,A), SLASH(16,16,8,8,C,A)
};
static const Patch friendship_star[] = {
    RECT(0,0,8,8,A), BACK(8,0,8,8,A,B), RECT(16,0,8,8,A),
    SLASH(0,8,8,8,A,B), RECT(8,8,8,8,B), SLASH(16,8,8,8,B,A),
    RECT(0,16,8,8,A), BACK(8,16,8,8,B,A), RECT(16,16,8,8,A)
};
static const Patch ohio_star[] = {
    RECT(0,0,8,8,A), QST(8,0,8,A,D,B,D), RECT(16,0,8,8,A),
    QST(0,8,8,D,B,D,A), RECT(8,8,8,8,B), QST(16,8,8,D,A,D,B),
    RECT(0,16,8,8,A), QST(8,16,8,B,D,A,D), RECT(16,16,8,8,A)
};
static const Patch sawtooth_star[] = {
    RECT(0,0,6,6,A), G_UP(6,0,6,E,A), RECT(18,0,6,6,A),
    G_LEFT(0,6,6,E,A), RECT(6,6,12,12,E), G_RIGHT(18,6,6,E,A),
    RECT(0,18,6,6,A), G_DOWN(6,18,6,E,A), RECT(18,18,6,6,A)
};
static const Patch snowball[] = {
    {8, A, {{6,0}, {18,0}, {24,6}, {24,18}, {18,24}, {6,24}, {0,18}, {0,6}}},
    TRI(0,0, 6,0, 0,6, B), TRI(18,0, 24,0, 24,6, B),
    TRI(24,18, 24,24, 18,24, B), TRI(0,18, 6,24, 0,24, B)
};
static const Patch bear_paw[] = {
    RECT(0,0,8,8,A), SLASH(8,0,8,8,A,D), SLASH(16,0,8,8,A,D),
    SLASH(0,8,8,8,A,D), SLASH(0,16,8,8,A,D), RECT(8,8,16,16,D)
};
static const Patch double_four_patch[] = {
    RECT(0,0,6,6,D), RECT(6,0,6,6,E), RECT(0,6,6,6,E), RECT(6,6,6,6,D),
    RECT(12,0,12,12,A), RECT(0,12,12,12,A),
    RECT(12,12,6,6,D), RECT(18,12,6,6,E), RECT(12,18,6,6,E), RECT(18,18,6,6,D)
};
static const Patch dutchmans_puzzle[] = {
    G_UP(0,0,6,B,A), G_UP(0,6,6,D,A),
    G_RIGHT(12,0,6,D,A), G_RIGHT(18,0,6,B,A),
    G_LEFT(0,12,6,B,A), G_LEFT(6,12,6,D,A),
    G_DOWN(12,12,6,D,A), G_DOWN(12,18,6,B,A)
};

static const Patch offset_bars[] = {
    RECT(0,0,6,24,D), RECT(6,0,12,5,A), RECT(6,5,12,10,B), RECT(6,15,12,9,C),
    RECT(18,0,6,17,E), RECT(18,17,6,7,A)
};
static const Patch bento_box[] = {
    RECT(0,0,12,3,D), RECT(0,3,3,9,D), RECT(3,3,9,3,A), RECT(3,6,3,6,A), RECT(6,6,6,6,B),
    RECT(12,0,12,3,A), RECT(21,3,3,9,A), RECT(12,3,9,3,C), RECT(18,6,3,6,C), RECT(12,6,6,6,D),
    RECT(0,21,12,3,A), RECT(0,12,3,9,A), RECT(3,18,9,3,C), RECT(3,12,3,6,C), RECT(6,12,6,6,D),
    RECT(12,21,12,3,D), RECT(21,12,3,9,D), RECT(12,18,9,3,A), RECT(18,12,3,6,A), RECT(12,12,6,6,B)
};
static const Patch split_horizon[] = {
    RECT(0,0,24,6,A),
    QUAD(0,6, 24,6, 24,10, 0,17, C), QUAD(0,17, 24,10, 24,14, 0,21, E),
    QUAD(0,21, 24,14, 24,24, 0,24, D)
};
static const Patch color_current[] = {
    TRI(0,0, 15,0, 0,15, C),
    QUAD(15,0, 24,0, 0,24, 0,15, A),
    TRI(24,0, 24,24, 0,24, D)
};
static const Patch facet_study[] = {
    TRI(0,0, 24,0, 8,16, B), TRI(24,0, 24,24, 8,16, D),
    TRI(24,24, 0,24, 8,16, E), TRI(0,24, 0,0, 8,16, A)
};
static const Patch kite_field[] = {
    QUAD(8,0, 24,8, 12,24, 0,16, D),
    TRI(0,0, 8,0, 0,16, E), TRI(8,0, 24,0, 24,8, A),
    TRI(24,8, 24,24, 12,24, C), TRI(0,16, 12,24, 0,24, B)
};
static const Patch honeycomb[] = {
    {6, E, {{6,0}, {18,0}, {24,12}, {18,24}, {6,24}, {0,12}}},
    TRI(0,0, 6,0, 0,12, B), TRI(18,0, 24,0, 24,12, C),
    TRI(24,12, 24,24, 18,24, B), TRI(0,12, 6,24, 0,24, D)
};
static const Patch hour_shift[] = {
    {8, D, {{4,0}, {16,0}, {24,9}, {24,20}, {19,24}, {10,24}, {0,16}, {0,8}}},
    TRI(0,0, 4,0, 0,8, A), TRI(16,0, 24,0, 24,9, E),
    TRI(24,20, 24,24, 19,24, A), TRI(0,16, 10,24, 0,24, B)
};
static const Patch paper_planes[] = {
    TRI(0,0, 24,0, 24,12, A), TRI(0,0, 24,12, 8,12, B),
    TRI(0,0, 8,12, 0,24, D), TRI(8,12, 24,12, 0,24, E),
    TRI(24,12, 24,24, 0,24, C)
};
static const Patch broken_chevron[] = {
    BACK(0,0,12,7,E,A), SLASH(12,0,12,7,E,D),
    BACK(0,7,12,8,D,B), SLASH(12,7,12,8,D,A),
    RECT(0,15,24,3,C), RECT(0,18,16,6,A), RECT(16,18,8,6,B)
};
static const Patch switchback[] = {
    QUAD(0,0, 8,0, 14,6, 0,6, A), QUAD(8,0, 24,0, 24,6, 14,6, D),
    QUAD(0,6, 14,6, 8,12, 0,12, C), QUAD(14,6, 24,6, 24,12, 8,12, E),
    QUAD(0,12, 8,12, 14,18, 0,18, E), QUAD(8,12, 24,12, 24,18, 14,18, B),
    QUAD(0,18, 14,18, 8,24, 0,24, D), QUAD(14,18, 24,18, 24,24, 8,24, A)
};
static const Patch window_lights[] = {
    RECT(0,0,24,3,D), RECT(0,21,24,3,D), RECT(0,3,3,18,D), RECT(21,3,3,18,D),
    RECT(3,3,18,3,B), RECT(3,6,18,2,C), RECT(3,8,8,9,A), RECT(11,8,3,9,D),
    RECT(14,8,7,9,E), RECT(3,17,18,4,B)
};
static const Patch terraced_steps[] = {
    RECT(0,0,7,5,C), RECT(7,0,17,5,A),
    RECT(0,5,11,5,C), RECT(11,5,13,5,A),
    RECT(0,10,5,5,A), RECT(5,10,10,5,D), RECT(15,10,9,5,E),
    RECT(0,15,19,5,B), RECT(19,15,5,5,E), RECT(0,20,24,4,D)
};
static const Patch confetti[] = {
    RECT(0,0,8,4,A), RECT(8,0,4,4,E), RECT(12,0,12,4,D),
    RECT(0,4,5,9,D), RECT(5,4,12,9,A), RECT(17,4,7,9,C),
    RECT(0,13,10,5,B), RECT(10,13,8,5,D), RECT(18,13,6,5,A),
    RECT(0,18,4,6,A), RECT(4,18,16,6,E), RECT(20,18,4,6,B)
};
static const Patch echo_frames[] = {
    RECT(0,0,24,4,C), RECT(0,4,4,15,D), RECT(20,4,4,15,D), RECT(0,19,24,5,C),
    RECT(4,4,16,3,A), RECT(4,7,3,9,E), RECT(17,7,3,9,E), RECT(4,16,16,3,A),
    RECT(7,7,10,9,B)
};
static const Patch crosswind[] = {
    RECT(0,0,10,9,A), SLASH(10,0,5,9,E,B), RECT(15,0,9,9,C),
    BACK(0,9,10,5,D,B), RECT(10,9,5,5,B), BACK(15,9,9,5,B,E),
    RECT(0,14,10,10,C), SLASH(10,14,5,10,B,D), RECT(15,14,9,10,A)
};
static const Patch spliced_stripe[] = {
    RECT(0,0,6,24,C), BACK(6,0,7,14,B,A), RECT(6,14,7,10,E),
    SLASH(13,0,5,19,A,D), RECT(13,19,5,5,C),
    RECT(18,0,6,9,D), BACK(18,9,6,15,E,B)
};
static const Patch field_notes[] = {
    RECT(0,0,24,4,A), RECT(0,20,24,4,C), RECT(0,4,5,16,D),
    RECT(5,4,11,6,B), RECT(5,10,11,4,A), RECT(5,14,11,6,E),
    BACK(16,4,8,8,E,D), SLASH(16,12,8,8,D,C)
};

#define DIAMOND_CELL(x,y,w,h,d,sky) \
    QUAD((x)+(w)/2,y, (x)+(w),(y)+(h)/2, (x)+(w)/2,(y)+(h), x,(y)+(h)/2, d), \
    TRI(x,y, (x)+(w)/2,y, x,(y)+(h)/2, sky), \
    TRI((x)+(w)/2,y, (x)+(w),y, (x)+(w),(y)+(h)/2, sky), \
    TRI((x)+(w),(y)+(h)/2, (x)+(w),(y)+(h), (x)+(w)/2,(y)+(h), sky), \
    TRI(x,(y)+(h)/2, (x)+(w)/2,(y)+(h), x,(y)+(h), sky)

static const Patch storm_at_sea[] = {
    RECT(0,0,6,6,D), DIAMOND_CELL(6,0,12,6,C,A), RECT(18,0,6,6,D),
    G_LEFT(0,6,6,C,A), DIAMOND_CELL(6,6,12,12,D,E), G_RIGHT(18,6,6,C,A),
    RECT(0,18,6,6,D), DIAMOND_CELL(6,18,12,6,C,A), RECT(18,18,6,6,D)
};
static const Patch woven_links[] = {
    QUAD(0,0, 12,0, 18,6, 0,6, D), TRI(12,0, 24,0, 18,6, B), TRI(24,0, 24,6, 18,6, D),
    RECT(0,6,8,12,C), DIAMOND_CELL(8,6,8,12,A,E), RECT(16,6,8,12,C),
    TRI(0,18, 6,18, 0,24, D), TRI(6,18, 12,24, 0,24, B),
    QUAD(6,18, 24,18, 24,24, 12,24, D)
};
static const Patch braided_ribbons[] = {
    BACK(0,0,12,8,B,D), SLASH(12,0,12,8,C,B),
    SLASH(0,8,12,8,B,C), BACK(12,8,12,8,D,B),
    BACK(0,16,12,8,C,A), SLASH(12,16,12,8,E,C)
};

#define FAMILY(en,de,pl,block,is_modern) {{en,de,pl}, block, sizeof block / sizeof *block, is_modern}
/* Project files use these indices and piece orders. Append future designs at the end. */
const Pattern patterns[PATTERN_COUNT + 1] = {
    FAMILY("Garden path", "Gartenpfad", "Ogrodowa ścieżka", square, false),
    FAMILY("Nine Patch", "Neunerblock", "Dziewięć pól", nine_patch, false),
    FAMILY("Rail Fence", "Lattenzaun", "Płot z listew", rail_fence, false),
    FAMILY("Log Cabin", "Blockhaus", "Domek z bali", log_cabin, false),
    FAMILY("Courthouse Steps", "Gerichtshaustreppe", "Schody do sądu", courthouse_steps, false),
    FAMILY("Pinwheel", "Windrad", "Wiatraczek", pinwheel, false),
    FAMILY("Broken Dishes", "Zerbrochene Teller", "Rozbite talerze", broken_dishes, false),
    FAMILY("Hourglass", "Sanduhr", "Klepsydra", hourglass, false),
    FAMILY("Flying Geese", "Fliegende Gänse", "Lecące gęsi", flying_geese, false),
    FAMILY("Square in a Square", "Quadrat im Quadrat", "Kwadrat w kwadracie", square_in_square, false),
    FAMILY("Shoo Fly", "Shoo Fly", "Shoo Fly", shoo_fly, false),
    FAMILY("Churn Dash", "Butterfass", "Maselnica", churn_dash, false),
    FAMILY("Friendship Star", "Freundschaftsstern", "Gwiazda przyjaźni", friendship_star, false),
    FAMILY("Ohio Star", "Ohio-Stern", "Gwiazda Ohio", ohio_star, false),
    FAMILY("Sawtooth Star", "Sägezahnstern", "Ząbkowana gwiazda", sawtooth_star, false),
    FAMILY("Snowball", "Schneeball", "Śnieżna kula", snowball, false),
    FAMILY("Bear's Paw", "Bärentatze", "Niedźwiedzia łapa", bear_paw, false),
    FAMILY("Double Four Patch", "Doppelter Viererblock", "Podwójne cztery pola", double_four_patch, false),
    FAMILY("Dutchman's Puzzle", "Holländisches Rätsel", "Holenderska łamigłówka", dutchmans_puzzle, false),
    FAMILY("Offset Bars", "Versetzte Streifen", "Przesunięte pasy", offset_bars, true),
    FAMILY("Bento Box", "Bento-Box", "Pudełko bento", bento_box, true),
    FAMILY("Split Horizon", "Geteilter Horizont", "Rozdzielony horyzont", split_horizon, true),
    FAMILY("Color Current", "Farbfluss", "Strumień kolorów", color_current, true),
    FAMILY("Facet Study", "Facettenstudie", "Studium faset", facet_study, true),
    FAMILY("Kite Field", "Drachenfeld", "Pole latawców", kite_field, true),
    FAMILY("Honeycomb", "Waben", "Plaster miodu", honeycomb, true),
    FAMILY("Hour Shift", "Zeitversatz", "Przesunięcie czasu", hour_shift, true),
    FAMILY("Paper Planes", "Papierflieger", "Papierowe samoloty", paper_planes, true),
    FAMILY("Broken Chevron", "Gebrochene Zacken", "Przełamane zygzaki", broken_chevron, true),
    FAMILY("Switchback", "Serpentinen", "Serpentyna", switchback, true),
    FAMILY("Window Lights", "Lichter im Fenster", "Światła w oknach", window_lights, true),
    FAMILY("Terraced Steps", "Terrassenstufen", "Tarasowe stopnie", terraced_steps, true),
    FAMILY("Confetti", "Konfetti", "Konfetti", confetti, true),
    FAMILY("Echo Frames", "Rahmen-Echo", "Echo ramek", echo_frames, true),
    FAMILY("Crosswind", "Seitenwind", "Boczny wiatr", crosswind, true),
    FAMILY("Spliced Stripe", "Geteilter Streifen", "Łączony pasek", spliced_stripe, true),
    FAMILY("Field Notes", "Feldnotizen", "Notatki z pola", field_notes, true),
    FAMILY("Storm at Sea", "Sturm auf See", "Sztorm na morzu", storm_at_sea, false),
    FAMILY("Woven Links", "Verflochtene Glieder", "Splecione ogniwa", woven_links, true),
    FAMILY("Braided Ribbons", "Geflochtene Bänder", "Plecione wstążki", braided_ribbons, true)
};

const char *const setting_name[SETTING_COUNT][LANGUAGE_COUNT] = {
    {"Full repeat", "Vollflächig", "Pełne powtórzenie"},
    {"Alternating", "Abwechselnd", "Naprzemiennie"},
    {"Offset", "Versetzt", "Z przesunięciem"},
    {"Rows", "Reihen", "Rzędy"},
    {"Columns", "Spalten", "Kolumny"},
    {"Diagonal", "Diagonal", "Po przekątnej"},
    {"Border", "Rand", "Obramowanie"},
    {"Medallion", "Medaillon", "Medalion"}
};

const char *const seam_name[SEAM_COUNT][LANGUAGE_COUNT] = {
    {"Straight grid", "Gerades Raster", "Równa siatka"},
    {"Staggered rows", "Versetzte Reihen", "Przesunięte rzędy"},
    {"Staggered columns", "Versetzte Spalten", "Przesunięte kolumny"}
};

int catalog_ids(PatternFilter filter, int ids[PATTERN_COUNT])
{
    int count = 0;
    for (int id = 1; id <= PATTERN_COUNT; ++id)
        if (filter == ALL_PATTERNS || patterns[id].modern == (filter == MODERN))
            ids[count++] = id;
    return count;
}

static int setting_turn(const Quilt *q, int r, int c)
{
    switch (q->setting) {
    case ALTERNATING: return (r + c) % 2 == 0 ? 1 + 2 * (r % 2) : 0;
    case OFFSET: return c % 3 == r % 2 ? 0 : 1 + 2 * (r % 2);
    case STRIPE_ROWS: return r % 2 == 0 ? 1 : 0;
    case STRIPE_COLUMNS: return c % 2 == 0 ? 2 : 0;
    case DIAGONAL: return (r - c) % 3 == 0 ? 1 : 0;
    case BORDER:
        if (r == 0) return 1;
        if (c == q->cols - 1) return 2;
        if (r == q->rows - 1) return 3;
        return c == 0 ? 4 : 0;
    case MEDALLION: return r > 0 && c > 0 && r < q->rows - 1 && c < q->cols - 1 ? 1 : 0;
    default: return 1;
    }
}

bool arrange(Quilt *q, int id, int setting)
{
    if (id < 1 || id > PATTERN_COUNT || setting < 0 || setting >= SETTING_COUNT) return false;
    set_whole_design(q, FOUNDATION, BLOCK_TOP);
    q->pattern = (uint8_t)id;
    q->setting = (uint8_t)setting;
    for (int r = 0; r < q->rows; ++r)
        for (int c = 0; c < q->cols; ++c) {
            int turn = setting_turn(q, r, c);
            place_block(&q->cell[r][c], turn ? id : 0, turn);
        }
    return true;
}

void place_block(Block *block, int id, int turn)
{
    *block = (Block){.pattern = (uint8_t)id, .turn = (uint8_t)turn};
    const Pattern *pattern = block_pattern(block);
    for (int p = 0; p < pattern->count; ++p) block->fabric[p] = pattern->patches[p].fabric;
}

const Pattern *block_pattern(const Block *block)
{
    return &patterns[block->pattern];
}

bool mixed_quilt(const Quilt *q)
{
    for (int r = 0; r < q->rows; ++r)
        for (int c = 0; c < q->cols; ++c) {
            int id = q->cell[r][c].pattern;
            if (id && id != q->pattern) return true;
        }
    return false;
}

const char *quilt_name(const Quilt *q, Language language)
{
    if (q->applique || q->foundation) return tr(language, whole_name[q->applique ? q->applique : q->foundation]);
    return mixed_quilt(q) ? tr(language, MIXED_NAME) : patterns[q->pattern].name[language];
}

static double stagger(int edge, int last)
{
    return edge > 0 && edge < last ? edge + 0.5 : edge;
}

BlockBounds block_bounds(const Quilt *q, int row, int col)
{
    if (q->seams == BRICK_ROWS && row % 2) {
        double x = stagger(col, q->cols);
        return (BlockBounds){x, row, stagger(col + 1, q->cols) - x, 1};
    }
    if (q->seams == BRICK_COLUMNS && col % 2) {
        double y = stagger(row, q->rows);
        return (BlockBounds){col, y, 1, stagger(row + 1, q->rows) - y};
    }
    return (BlockBounds){col, row, 1, 1};
}

Point turn_point(Point p, int turn)
{
    switch (turn) {
    case 2: return (Point){BLOCK_UNITS - p.y, p.x};
    case 3: return (Point){BLOCK_UNITS - p.x, BLOCK_UNITS - p.y};
    case 4: return (Point){p.y, BLOCK_UNITS - p.x};
    default: return p;
    }
}

int hit_patch(const Pattern *pattern, int turn, float x, float y)
{
    for (int i = 0; i < pattern->count; ++i) {
        const Patch *patch = &pattern->patches[i];
        Point a = turn_point(patch->point[patch->vertices - 1], turn);
        bool inside = true;
        for (int v = 0; v < patch->vertices; ++v) {
            Point b = turn_point(patch->point[v], turn);
            if ((b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x) < 0) {
                inside = false;
                break;
            }
            a = b;
        }
        if (inside) return i;
    }
    return -1;
}
