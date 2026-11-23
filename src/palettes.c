#include "palettes.h"
#include <stdio.h>

typedef struct {
    const char *name[LANGUAGE_COUNT];
    uint32_t color[9];
} ColorFamily;

/* Each family has two lights, a soft neutral, three midtones, two accents and a dark. */
static const ColorFamily families[] = {
    {{"Baltic Coast", "Ostseeküste", "Wybrzeże Bałtyku"},
     {0xf3ede0, 0xd7e8e9, 0xc9bca5, 0x719da6, 0x607581, 0x99aa8e, 0xe0b570, 0xc17259, 0x223f52}},
    {{"Wild Orchard", "Wilder Obstgarten", "Dziki sad"},
     {0xfff2d5, 0xf5d9c4, 0xc8b89e, 0x8e9d61, 0xa74c51, 0xc98565, 0xebbb55, 0x743e56, 0x304a39}},
    {{"Alpine Lake", "Alpensee", "Alpejskie jezioro"},
     {0xf2f4eb, 0xdcebf2, 0xbfc9c5, 0x8fac97, 0x567d8c, 0x758fa2, 0xd6ac71, 0xbe716a, 0x273e53}},
    {{"Terracotta", "Terrakotta", "Terakota"},
     {0xfff0db, 0xead2b5, 0xc4a894, 0xd1816c, 0xaa5443, 0x8d936c, 0xe6a657, 0x624f7c, 0x51342f}},
    {{"Market Flowers", "Marktblumen", "Kwiaty z targu"},
     {0xfcefe6, 0xf6d9e4, 0xc7ada9, 0xdb7b96, 0x88619c, 0x7b957c, 0xf3bb63, 0xba4565, 0x473352}},
    {{"Winter Forest", "Winterwald", "Zimowy las"},
     {0xf4f3e7, 0xdbe6dc, 0xb8bcb1, 0x7a9985, 0x477369, 0x66788a, 0xd6b876, 0xab6861, 0x233b36}},
    {{"Indigo Workshop", "Indigowerkstatt", "Pracownia indygo"},
     {0xf4efe3, 0xdeebf1, 0xb9b9d1, 0x738db7, 0x425f9c, 0x63847e, 0xe8b05d, 0xbf6d76, 0x263555}},
    {{"Autumn Pantry", "Herbstvorrat", "Jesienna spiżarnia"},
     {0xffefd0, 0xf3d6a3, 0xc7a687, 0xc58153, 0xa44937, 0x7b7352, 0xe7a928, 0x89445d, 0x473027}},
    {{"Desert Garden", "Wüstengarten", "Pustynny ogród"},
     {0xfcf2db, 0xe9e9ce, 0xd0b39c, 0xbe8778, 0x729081, 0x9a8caf, 0xe2a44f, 0xbb5e60, 0x404a4d}},
    {{"Evening Studio", "Abendatelier", "Wieczorne atelier"},
     {0xf8eee6, 0xeaddf1, 0xc5b3c0, 0xa67caa, 0x676593, 0x5d898c, 0xe5b468, 0xcf7666, 0x342b4a}}
};

static const struct {
    const char *name[LANGUAGE_COUNT];
    uint8_t pick[STARTER_FABRICS];
} combinations[] = {
    {{"Morning", "Morgen", "Poranek"}, {0, 3, 5, 8, 6}},
    {{"Mist", "Nebel", "Mgła"}, {1, 2, 3, 8, 5}},
    {{"Bloom", "Blüte", "Rozkwit"}, {0, 7, 5, 8, 3}},
    {{"Twilight", "Dämmerung", "Zmierzch"}, {1, 4, 5, 8, 7}},
    {{"Sunbeam", "Sonnenstrahl", "Promień"}, {0, 6, 2, 8, 4}},
    {{"Still Life", "Stillleben", "Martwa natura"}, {1, 2, 4, 8, 6}},
    {{"Harvest", "Ernte", "Zbiory"}, {0, 7, 3, 8, 6}},
    {{"Shade", "Schatten", "Cień"}, {1, 3, 4, 8, 2}},
    {{"Festival", "Fest", "Święto"}, {0, 6, 5, 8, 7}},
    {{"Postcard", "Postkarte", "Pocztówka"}, {1, 7, 4, 8, 3}}
};

enum { PALETTES_PER_FAMILY = sizeof combinations / sizeof *combinations };
_Static_assert(sizeof families / sizeof *families * PALETTES_PER_FAMILY == PALETTE_COUNT, "Palette count");

void palette_name(int id, Language language, char out[PALETTE_NAME_BYTES])
{
    snprintf(out, PALETTE_NAME_BYTES, "%s / %s", families[id / PALETTES_PER_FAMILY].name[language],
             combinations[id % PALETTES_PER_FAMILY].name[language]);
}

void palette_colors(int id, uint32_t out[STARTER_FABRICS])
{
    const ColorFamily *family = &families[id / PALETTES_PER_FAMILY];
    const uint8_t *pick = combinations[id % PALETTES_PER_FAMILY].pick;
    for (int f = 0; f < STARTER_FABRICS; ++f) out[f] = family->color[pick[f]];
}

uint32_t extra_fabric_color(int fabric)
{
    static const uint32_t scraps[MAX_FABRICS - STARTER_FABRICS] = {
        0x679e9b, 0xa37491, 0xca8158, 0x536f51, 0xe7b2a8, 0xaabcca, 0x733f50, 0xa69a63, 0x56789b, 0xdbc891, 0x9b7161,
        0x7f909b, 0xc58b9f, 0x68836c, 0xf2d6b0, 0x6f6190, 0x90aba6, 0xc48c62, 0x455868, 0xb7b481, 0x8f5e68};
    return scraps[fabric - STARTER_FABRICS];
}
