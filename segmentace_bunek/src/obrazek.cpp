// #####################################################################
//  obrazek.cpp - NAČÍTÁNÍ A UKLÁDÁNÍ OBRÁZKŮ
// #####################################################################
//  Knihovny stb_image (načtení JPG/PNG) a stb_image_write (uložení PNG).
//  #define ..._IMPLEMENTATION smí být jen v JEDNOM .cpp souboru celého
//  projektu - jinak by linker hlásil, že funkce jsou definované dvakrát.
// #####################################################################
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "obrazek.h"
#include <cstdio>   // snprintf

// ---------------------------------------------------------------------
//  Načtení jednoho snímku
//  stb_image načte JPG/PNG a rovnou ho převede na šedotón (1 kanál)
// ---------------------------------------------------------------------
bool nactiObrazek(const std::string& cesta, Obrazek& img)
{
    int kanaly;
    unsigned char* data = stbi_load(cesta.c_str(), &img.sirka, &img.vyska, &kanaly, 1);
    if (!data) return false;                               // soubor nenalezen
    img.px.assign(data, data + img.sirka * img.vyska);     // kopie do vektoru
    stbi_image_free(data);                                 // uvolnění paměti knihovny
    return true;
}

// ---------------------------------------------------------------------
//  Načtení časové série t000.png, t001.png, ...
// ---------------------------------------------------------------------
std::vector<Obrazek> nactiSekvenci(const std::string& slozka)
{
    std::vector<Obrazek> snimky;
    for (int t = 0; ; t++)
    {
        char jmeno[32];
        std::snprintf(jmeno, sizeof(jmeno), "t%03d.png", t);   // t = 7 -> "t007.png"
        Obrazek img;
        if (!nactiObrazek(slozka + jmeno, img)) break;         // další snímek už není
        snimky.push_back(img);
    }
    return snimky;
}

// ---------------------------------------------------------------------
//  Uložení PNG (poslední parametr stbi_write_png = počet bajtů na řádek)
// ---------------------------------------------------------------------
void ulozPNG(const std::string& cesta, const Obrazek& img)
{
    stbi_write_png(cesta.c_str(), img.sirka, img.vyska, 1, img.px.data(), img.sirka);
}

void ulozPNG(const std::string& cesta, const ObrazekRGB& img)
{
    stbi_write_png(cesta.c_str(), img.sirka, img.vyska, 3, img.px.data(), 3 * img.sirka);
}
