// #####################################################################
//  kresleni.cpp - KRESLENÍ DO OBRÁZKŮ
// #####################################################################
#include "kresleni.h"
#include <cstdlib>   // abs
#include <string>

// ---------------------------------------------------------------------
//  Převod šedotónu na RGB
// ---------------------------------------------------------------------
ObrazekRGB naRGB(const Obrazek& img, int jas_procent)
{
    ObrazekRGB out;
    out.sirka = img.sirka; out.vyska = img.vyska;
    out.px.resize(3 * img.px.size());
    for (size_t i = 0; i < img.px.size(); i++)
        out.px[3 * i] = out.px[3 * i + 1] = out.px[3 * i + 2] = img.px[i] * jas_procent / 100;
    return out;
}

// ---------------------------------------------------------------------
//  Obrysy buněk (zeleně)
// ---------------------------------------------------------------------
void kresliObrysy(ObrazekRGB& img, const std::vector<int>& stitky)
{
    int W = img.sirka, H = img.vyska;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            int s = stitky[y * W + x];
            if (s == 0) continue;                          // pozadí přeskočit
            bool hrana = (x == 0 || y == 0 || x == W - 1 || y == H - 1) ||   // okraj obrázku
                         stitky[y * W + x - 1] != s || stitky[y * W + x + 1] != s ||
                         stitky[(y - 1) * W + x] != s || stitky[(y + 1) * W + x] != s;
            if (hrana) img.nastav(x, y, 0, 255, 0);
        }
}

// ---------------------------------------------------------------------
//  Úsečka - Bresenhamův algoritmus (jen celočíselné operace)
// ---------------------------------------------------------------------
void kresliCaru(ObrazekRGB& img, int x0, int y0, int x1, int y1,
                unsigned char r, unsigned char g, unsigned char b)
{
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int chyba = dx + dy;                   // odchylka od ideální přímky
    while (true)
    {
        img.nastav(x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;   // došli jsme na konec
        int e2 = 2 * chyba;
        if (e2 >= dy) { chyba += dy; x0 += sx; }   // krok vodorovně
        if (e2 <= dx) { chyba += dx; y0 += sy; }   // krok svisle
    }
}

// ---------------------------------------------------------------------
//  Křížek
// ---------------------------------------------------------------------
void kresliKrizek(ObrazekRGB& img, int x, int y, int vel,
                  unsigned char r, unsigned char g, unsigned char b)
{
    for (int i = -vel; i <= vel; i++)
    {
        img.nastav(x + i, y, r, g, b);   // vodorovná část
        img.nastav(x, y + i, r, g, b);   // svislá část
    }
}

// ---------------------------------------------------------------------
//  Kóta - číslo fontem 3x5 px
//  Každý řádek číslice = 3 bity, např. 7 = 111 (plný), 5 = 101 (krajní)
// ---------------------------------------------------------------------
void kresliCislo(ObrazekRGB& img, int x, int y, int cislo, int meritko)
{
    static const int font[10][5] = {
        {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
        {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};
    std::string text = std::to_string(cislo);
    for (size_t c = 0; c < text.size(); c++)       // každá číslice
    {
        int d = text[c] - '0';                     // znak -> číslo 0-9
        for (int radek = 0; radek < 5; radek++)
            for (int sl = 0; sl < 3; sl++)
                if (font[d][radek] & (4 >> sl))    // svítí tento pixel?
                    for (int my = 0; my < meritko; my++)
                        for (int mx = 0; mx < meritko; mx++)
                            img.nastav(x + (c * 4 + sl) * meritko + mx,
                                       y + radek * meritko + my, 255, 255, 0);
    }
}

// ---------------------------------------------------------------------
//  Barva trajektorie - paleta 12 dobře rozlišitelných barev,
//  trajektorie dostane barvu podle svého ID (cyklicky)
// ---------------------------------------------------------------------
void barvaTrajektorie(int id, unsigned char& r, unsigned char& g, unsigned char& b)
{
    static const unsigned char paleta[12][3] = {
        {255, 80, 80}, {80, 200, 255}, {255, 220, 60}, {120, 255, 120},
        {255, 120, 255}, {255, 160, 60}, {60, 255, 220}, {180, 140, 255},
        {255, 255, 255}, {160, 255, 60}, {255, 100, 170}, {100, 160, 255}};
    r = paleta[id % 12][0]; g = paleta[id % 12][1]; b = paleta[id % 12][2];
}

// ---------------------------------------------------------------------
//  Trajektorie (lomená čára) - kreslí body s časem od tOd do tDo
// ---------------------------------------------------------------------
void kresliTrajektorii(ObrazekRGB& img, const Trajektorie& tr, int tOd, int tDo)
{
    unsigned char r, g, b;
    barvaTrajektorie(tr.id, r, g, b);
    for (size_t i = 1; i < tr.body.size(); i++)
    {
        const Bod& p = tr.body[i - 1];
        const Bod& q = tr.body[i];
        if (p.t < tOd || q.t > tDo) continue;      // úsek mimo zvolený časový rozsah
        kresliCaru(img, p.x, p.y, q.x, q.y, r, g, b);
    }
}
