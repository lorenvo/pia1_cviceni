// #####################################################################
//  kresleni.h - KRESLENÍ DO OBRÁZKŮ
// #####################################################################
#pragma once
#include "obrazek.h"
#include "simulace.h"
#include <vector>

// Převod šedotónu na RGB (R = G = B = jas * jas_procent / 100);
// jas_procent < 100 obrázek ztmaví, aby barevné čáry víc vynikly
ObrazekRGB naRGB(const Obrazek& img, int jas_procent = 100);

// Obrysy buněk (zeleně): pixel buňky, jehož soused má jiné ID
void kresliObrysy(ObrazekRGB& img, const std::vector<int>& stitky);

// Úsečka - Bresenhamův algoritmus
void kresliCaru(ObrazekRGB& img, int x0, int y0, int x1, int y1,
                unsigned char r, unsigned char g, unsigned char b);

// Křížek (značka těžiště / konce trajektorie), výchozí barva červená
void kresliKrizek(ObrazekRGB& img, int x, int y, int vel,
                  unsigned char r = 255, unsigned char g = 0, unsigned char b = 0);

// Kóta - číslo vykreslené vlastním fontem 3x5 px (žlutě)
void kresliCislo(ObrazekRGB& img, int x, int y, int cislo, int meritko);

// Barva trajektorie podle jejího ID (paleta 12 barev)
void barvaTrajektorie(int id, unsigned char& r, unsigned char& g, unsigned char& b);

// Trajektorie jako lomená čára; kreslí jen úseky v čase tOd..tDo
void kresliTrajektorii(ObrazekRGB& img, const Trajektorie& tr, int tOd, int tDo);
