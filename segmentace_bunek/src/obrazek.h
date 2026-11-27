// #####################################################################
//  obrazek.h - DATOVÉ STRUKTURY OBRÁZKŮ A JEJICH NAČÍTÁNÍ
// #####################################################################
#pragma once
#include <string>
#include <vector>

// Šedotónový obrázek: 1 bajt na pixel (0 = černá, 255 = bílá).
// Pixely jsou uložené po řádcích v jednom poli: index = y * sirka + x
struct Obrazek
{
    int sirka = 0, vyska = 0;
    std::vector<unsigned char> px;
    unsigned char& at(int x, int y)       { return px[y * sirka + x]; }  // čtení i zápis
    unsigned char  at(int x, int y) const { return px[y * sirka + x]; }  // jen čtení
};

// Barevný obrázek: 3 bajty na pixel (R, G, B)
struct ObrazekRGB
{
    int sirka = 0, vyska = 0;
    std::vector<unsigned char> px;

    // Obarví pixel (x, y); pixely mimo obrázek se ignorují
    void nastav(int x, int y, unsigned char r, unsigned char g, unsigned char b)
    {
        if (x < 0 || y < 0 || x >= sirka || y >= vyska) return;  // mimo obrázek
        int i = 3 * (y * sirka + x);
        px[i] = r; px[i + 1] = g; px[i + 2] = b;
    }
};

// Načte jeden snímek (JPG/PNG) jako šedotón; false = soubor nenalezen
bool nactiObrazek(const std::string& cesta, Obrazek& img);

// Načte časovou sérii t000.png, t001.png, ... dokud další soubor existuje
std::vector<Obrazek> nactiSekvenci(const std::string& slozka);

// Uloží obrázek jako PNG
void ulozPNG(const std::string& cesta, const Obrazek& img);
void ulozPNG(const std::string& cesta, const ObrazekRGB& img);
