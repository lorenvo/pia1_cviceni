// #####################################################################
//  segmentace.h - ROZPOZNÁNÍ BUNĚK V JEDNOM SNÍMKU
// #####################################################################
//  Postup: rozmazání -> automatický práh (Otsu) -> maska
//          -> vyplnění děr -> označení buněk + těžiště
// #####################################################################
#pragma once
#include "obrazek.h"
#include <vector>

// Jedna nalezená buňka
struct Bunka
{
    int id;
    int plocha = 0;        // počet pixelů
    double sx = 0, sy = 0; // těžiště [px]
};

// Výsledek segmentace jednoho snímku
struct Segmentace
{
    Obrazek maska;               // 255 = buňka, 0 = pozadí
    std::vector<int> stitky;     // ID buňky pro každý pixel (0 = pozadí)
    std::vector<Bunka> bunky;    // seznam buněk s těžišti
    int prah = 0;                // použitý globální práh
};

// Celá segmentace jednoho snímku (parametry viz parametry.h)
Segmentace segmentuj(const Obrazek& snimek);
