// #####################################################################
//  segmentace.cpp - ROZPOZNÁNÍ BUNĚK V JEDNOM SNÍMKU
// #####################################################################
//  Pomocné funkce (rozmaz, logaritmuj, otsuPrah, ...) jsou "static":
//  jsou vidět jen v tomto souboru. Navenek modul nabízí jen segmentuj().
// #####################################################################
#include "segmentace.h"
#include "parametry.h"
#include <queue>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------
//  1) Rozmazání (průměrovací filtr)
//  Každý pixel nahradí průměrem čtverce (2r+1)x(2r+1) kolem sebe.
//  Malé r = potlačení šumu, velké r = průměrný jas okolí.
//
//  Zrychlení: součet přes čtverec = součet přes řádek, pak přes sloupec.
//  Místo (2r+1)^2 sčítání na pixel stačí 2*(2r+1).
// ---------------------------------------------------------------------
static Obrazek rozmaz(const Obrazek& img, int r)
{
    int W = img.sirka, H = img.vyska;
    std::vector<int> radky(W * H);   // mezivýsledek: součty ve vodorovném směru

    // krok 1: vodorovné součty
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            int suma = 0;
            for (int dx = -r; dx <= r; dx++)
            {
                int xx = x + dx;
                if (xx >= 0 && xx < W) suma += img.at(xx, y);
            }
            radky[y * W + x] = suma;
        }

    // krok 2: svislé součty mezivýsledku a vydělení počtem pixelů
    Obrazek out = img;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            int suma = 0;
            for (int dy = -r; dy <= r; dy++)
            {
                int yy = y + dy;
                if (yy >= 0 && yy < H) suma += radky[yy * W + x];
            }
            // počet sečtených pixelů (u okraje obrázku je čtverec oříznutý)
            int nx = std::min(x + r, W - 1) - std::max(x - r, 0) + 1;
            int ny = std::min(y + r, H - 1) - std::max(y - r, 0) + 1;
            out.at(x, y) = suma / (nx * ny);
        }
    return out;
}

// ---------------------------------------------------------------------
//  2) Logaritmická transformace jasu
//  Roztáhne tmavé hodnoty, stlačí světlé -> Otsu nepřehlédne
//  slabě svítící jádra. Používá se jen pro výpočet prahu.
// ---------------------------------------------------------------------
static Obrazek logaritmuj(const Obrazek& img)
{
    Obrazek out = img;
    for (auto& v : out.px) v = 255.0 * std::log(1.0 + v) / std::log(256.0);
    return out;
}

// ---------------------------------------------------------------------
//  3) Otsuova metoda - automatický práh
//  Vyzkouší všechny prahy 0-255 a vybere ten, při kterém se skupiny
//  "pozadí" a "buňky" nejvíce liší (max. mezitřídní rozptyl).
// ---------------------------------------------------------------------
static int otsuPrah(const Obrazek& img)
{
    // histogram: kolik pixelů má jas 0, 1, ..., 255
    std::vector<double> hist(256, 0);
    for (unsigned char v : img.px) hist[v]++;

    double N = img.px.size(), sumaVse = 0;
    for (int i = 0; i < 256; i++) sumaVse += i * hist[i];

    // hledání prahu s největším rozptylem mezi třídami
    double w0 = 0, suma0 = 0, nejlepsi = 0;
    int prah = 0;
    for (int t = 0; t < 256; t++)
    {
        w0 += hist[t];                 // počet pixelů v třídě "pozadí"
        if (w0 == 0) continue;
        double w1 = N - w0;            // počet pixelů v třídě "buňka"
        if (w1 == 0) break;
        suma0 += t * hist[t];
        double m0 = suma0 / w0;                 // střední jas pozadí
        double m1 = (sumaVse - suma0) / w1;     // střední jas buněk
        double rozptyl = w0 * w1 * (m0 - m1) * (m0 - m1); // rozptyl mezi třídami
        if (rozptyl > nejlepsi) { nejlepsi = rozptyl; prah = t; }
    }
    return prah;
}

// ---------------------------------------------------------------------
//  4) Prahování -> binární maska (255 = buňka, 0 = pozadí)
//  Pixel je buňka, když splní OBĚ podmínky:
//    a) je světlejší než globální práh (Otsu)   -> není to pozadí
//    b) je světlejší než průměr svého okolí + C -> oddělí dotýkající
//       se jádra (mezera mezi nimi je tmavší než okolí)
// ---------------------------------------------------------------------
static Obrazek prahuj(const Obrazek& img, const Obrazek& lokalniPrumer, int prah, int C)
{
    Obrazek maska = img;
    for (size_t i = 0; i < img.px.size(); i++)
        maska.px[i] = (img.px[i] > prah && img.px[i] > lokalniPrumer.px[i] + C) ? 255 : 0;
    return maska;
}

// ---------------------------------------------------------------------
//  5) Vyplnění děr
//  Tmavá jadérka vytvoří v masce díry. Pozadí "zaplavíme" od okraje
//  obrázku; černé pixely, kam se záplava nedostala, jsou díry uvnitř
//  buňky -> přebarvíme je na bílou.
// ---------------------------------------------------------------------
static void vyplnDiry(Obrazek& maska)
{
    int W = maska.sirka, H = maska.vyska;
    std::vector<bool> venku(W * H, false);    // true = vnější pozadí

    // start záplavy: všechny pixely na okraji obrázku
    std::queue<int> fronta;
    for (int x = 0; x < W; x++) { fronta.push(x); fronta.push((H - 1) * W + x); }
    for (int y = 0; y < H; y++) { fronta.push(y * W); fronta.push(y * W + W - 1); }

    // šíření záplavy po černých pixelech (BFS)
    while (!fronta.empty())
    {
        int p = fronta.front(); fronta.pop();
        if (venku[p] || maska.px[p] == 255) continue;   // navštívený nebo bílý -> dál ne
        venku[p] = true;
        int x = p % W, y = p / W;
        if (x > 0)     fronta.push(p - 1);   // vlevo
        if (x < W - 1) fronta.push(p + 1);   // vpravo
        if (y > 0)     fronta.push(p - W);   // nahoru
        if (y < H - 1) fronta.push(p + W);   // dolů
    }

    // co nebylo zaplaveno, patří buňce (včetně děr)
    for (int i = 0; i < W * H; i++)
        if (!venku[i]) maska.px[i] = 255;
}

// ---------------------------------------------------------------------
//  6) Označení buněk (connected components) + těžiště
//  Každá souvislá bílá oblast dostane vlastní ID (flood fill / BFS).
//  Oblasti menší než minPlocha se zahodí jako šum.
//  Těžiště S = [průměr X, průměr Y] všech pixelů buňky.
// ---------------------------------------------------------------------
static std::vector<Bunka> oznacBunky(Obrazek& maska, std::vector<int>& stitky, int minPlocha)
{
    int W = maska.sirka, H = maska.vyska;
    stitky.assign(W * H, 0);                       // ID buňky pro každý pixel (0 = pozadí)
    std::vector<Bunka> bunky;
    int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};   // 4 směry: vpravo, vlevo, dolů, nahoru
    int dalsiID = 1;

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            // hledáme bílý pixel, který ještě nepatří žádné buňce
            if (maska.at(x, y) == 0 || stitky[y * W + x] != 0) continue;

            // --- nová buňka -> flood fill celé oblasti ---
            std::vector<int> pixelyBunky;
            std::queue<int> fronta;
            fronta.push(y * W + x);
            stitky[y * W + x] = dalsiID;
            while (!fronta.empty())
            {
                int p = fronta.front(); fronta.pop();
                pixelyBunky.push_back(p);
                int px = p % W, py = p / W;
                for (int k = 0; k < 4; k++)            // kontrola 4 sousedů
                {
                    int nx = px + dx[k], ny = py + dy[k];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int q = ny * W + nx;
                    if (maska.px[q] == 255 && stitky[q] == 0)
                    {
                        stitky[q] = dalsiID;           // soused patří stejné buňce
                        fronta.push(q);
                    }
                }
            }

            // --- filtr šumu: malé oblasti smazat ---
            if ((int)pixelyBunky.size() < minPlocha)
            {
                for (int p : pixelyBunky) { stitky[p] = -1; maska.px[p] = 0; }
                continue;
            }

            // --- uložení buňky + výpočet těžiště ---
            Bunka b;
            b.id = dalsiID++;
            b.plocha = pixelyBunky.size();
            for (int p : pixelyBunky) { b.sx += p % W; b.sy += p / W; }
            b.sx /= b.plocha;
            b.sy /= b.plocha;
            bunky.push_back(b);
        }

    for (auto& s : stitky) if (s < 0) s = 0;    // smazaný šum zpět na pozadí
    return bunky;
}

// ---------------------------------------------------------------------
//  CELÁ SEGMENTACE JEDNOHO SNÍMKU (kroky 1 - 6 za sebou)
//  Výsledek používá analýza snímku (A) i simulace pohybu (B).
// ---------------------------------------------------------------------
Segmentace segmentuj(const Obrazek& snimek)
{
    Segmentace s;
    Obrazek hladky = rozmaz(snimek, ROZMAZANI);
    Obrazek okoli  = rozmaz(hladky, OKOLI);                     // lokální průměr jasu
    int prahLog = otsuPrah(logaritmuj(hladky));                 // Otsu v log. škále
    s.prah = std::exp(prahLog * std::log(256.0) / 255.0) - 1;   // zpět na jas 0-255
    s.maska = prahuj(hladky, okoli, s.prah, C_LOKALNI);
    vyplnDiry(s.maska);
    s.bunky = oznacBunky(s.maska, s.stitky, MIN_PLOCHA);
    return s;
}
