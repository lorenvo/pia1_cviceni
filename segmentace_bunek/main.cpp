// =====================================================================
//  Segmentace buněčných jader z mikroskopického snímku
//  VSTUP : data/input/mikroskop_bunky.jpg (šedotónový snímek)
//  VÝSTUP: data/output/1_maska.png        - binární maska (bílá = buňka)
//          data/output/2_obrysy.png       - snímek se zelenými obrysy
//          data/output/3_vzdalenosti.png  - středy, spojnice, kóty
//          data/output/bunky.csv          - ID, těžiště, plocha
//          data/output/vzdalenosti.csv    - dvojice sousedů a vzdálenost
// =====================================================================

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/stb_image_write.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <set>
#include <cmath>
#include <string>
#include <algorithm>

// ---------------------------------------------------------------------
//  Datové struktury
// ---------------------------------------------------------------------
struct Obrazek            // šedotónový obrázek: 1 bajt na pixel (0-255)
{
    int sirka = 0, vyska = 0;
    std::vector<unsigned char> px;
    unsigned char& at(int x, int y) { return px[y * sirka + x]; }
};

struct ObrazekRGB         // barevný obrázek: 3 bajty na pixel (R,G,B)
{
    int sirka = 0, vyska = 0;
    std::vector<unsigned char> px;
    void nastav(int x, int y, unsigned char r, unsigned char g, unsigned char b)
    {
        if (x < 0 || y < 0 || x >= sirka || y >= vyska) return;  // mimo obrázek
        int i = 3 * (y * sirka + x);
        px[i] = r; px[i + 1] = g; px[i + 2] = b;
    }
};

struct Bunka
{
    int id;
    int plocha = 0;        // počet pixelů
    double sx = 0, sy = 0; // těžiště [px]
};

struct Dvojice { int a, b; double d; };   // indexy dvou buněk a jejich vzdálenost

// ---------------------------------------------------------------------
//  1) Načtení snímku (stb_image převede JPG/PNG rovnou na šedotón)
// ---------------------------------------------------------------------
bool nactiObrazek(const std::string& cesta, Obrazek& img)
{
    int kanaly;
    unsigned char* data = stbi_load(cesta.c_str(), &img.sirka, &img.vyska, &kanaly, 1);
    if (!data) return false;
    img.px.assign(data, data + img.sirka * img.vyska);
    stbi_image_free(data);
    return true;
}

// ---------------------------------------------------------------------
//  2) Rozmazání (průměr okolí) - potlačí šum a texturu uvnitř jader
// ---------------------------------------------------------------------
Obrazek rozmaz(Obrazek& img, int r)
{
    Obrazek out = img;
    for (int y = 0; y < img.vyska; y++)
        for (int x = 0; x < img.sirka; x++)
        {
            int suma = 0, n = 0;
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++)
                {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= img.sirka || yy >= img.vyska) continue;
                    suma += img.at(xx, yy);
                    n++;
                }
            out.at(x, y) = suma / n;
        }
    return out;
}

// ---------------------------------------------------------------------
//  2b) Logaritmická transformace jasu - zesílí tmavá jádra a zeslabí
//      velmi světlá, takže Otsu nepřehlédne slabě svítící buňky
// ---------------------------------------------------------------------
Obrazek logaritmuj(const Obrazek& img)
{
    Obrazek out = img;
    for (auto& v : out.px) v = 255.0 * std::log(1.0 + v) / std::log(256.0);
    return out;
}

// ---------------------------------------------------------------------
//  3) Otsuova metoda - automaticky najde práh, který nejlépe rozdělí
//     histogram na dvě třídy (pozadí / buňky)
// ---------------------------------------------------------------------
int otsuPrah(const Obrazek& img)
{
    std::vector<double> hist(256, 0);
    for (unsigned char v : img.px) hist[v]++;

    double N = img.px.size(), sumaVse = 0;
    for (int i = 0; i < 256; i++) sumaVse += i * hist[i];

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
//     Pixel je buňka, když splní OBĚ podmínky:
//       a) je světlejší než globální práh (Otsu)  -> není to pozadí
//       b) je světlejší než průměr svého okolí + C -> odliší se od
//          sousedního jádra, takže se dotýkající jádra méně slévají
// ---------------------------------------------------------------------
Obrazek prahuj(const Obrazek& img, const Obrazek& lokalniPrumer, int prah, int C)
{
    Obrazek maska = img;
    for (size_t i = 0; i < img.px.size(); i++)
        maska.px[i] = (img.px[i] > prah && img.px[i] > lokalniPrumer.px[i] + C) ? 255 : 0;
    return maska;
}

// ---------------------------------------------------------------------
//  4b) Vyplnění děr - tmavá jadérka uvnitř jádra vytvoří v masce díry.
//      Pozadí "zaplavíme" od okraje obrázku; černé pixely, kam se
//      záplava nedostala, leží uvnitř buňky -> přebarvíme na bílou.
// ---------------------------------------------------------------------
void vyplnDiry(Obrazek& maska)
{
    int W = maska.sirka, H = maska.vyska;
    std::vector<bool> venku(W * H, false);
    std::queue<int> fronta;
    for (int x = 0; x < W; x++) { fronta.push(x); fronta.push((H - 1) * W + x); }
    for (int y = 0; y < H; y++) { fronta.push(y * W); fronta.push(y * W + W - 1); }

    while (!fronta.empty())
    {
        int p = fronta.front(); fronta.pop();
        if (venku[p] || maska.px[p] == 255) continue;
        venku[p] = true;
        int x = p % W, y = p / W;
        if (x > 0)     fronta.push(p - 1);
        if (x < W - 1) fronta.push(p + 1);
        if (y > 0)     fronta.push(p - W);
        if (y < H - 1) fronta.push(p + W);
    }
    for (int i = 0; i < W * H; i++)
        if (!venku[i]) maska.px[i] = 255;
}

// ---------------------------------------------------------------------
//  5) Označení buněk (connected components) - flood fill pomocí BFS.
//     Každá souvislá bílá oblast dostane vlastní ID. Malé oblasti
//     (šum) se zahodí. Zároveň se počítá těžiště: S = [mean X, mean Y].
// ---------------------------------------------------------------------
std::vector<Bunka> oznacBunky(Obrazek& maska, std::vector<int>& stitky, int minPlocha)
{
    int W = maska.sirka, H = maska.vyska;
    stitky.assign(W * H, 0);
    std::vector<Bunka> bunky;
    int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    int dalsiID = 1;

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (maska.at(x, y) == 0 || stitky[y * W + x] != 0) continue;

            // nová buňka -> vyplň celou oblast
            std::vector<int> pixelyBunky;
            std::queue<int> fronta;
            fronta.push(y * W + x);
            stitky[y * W + x] = dalsiID;
            while (!fronta.empty())
            {
                int p = fronta.front(); fronta.pop();
                pixelyBunky.push_back(p);
                int px = p % W, py = p / W;
                for (int k = 0; k < 4; k++)
                {
                    int nx = px + dx[k], ny = py + dy[k];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int q = ny * W + nx;
                    if (maska.px[q] == 255 && stitky[q] == 0)
                    {
                        stitky[q] = dalsiID;
                        fronta.push(q);
                    }
                }
            }

            if ((int)pixelyBunky.size() < minPlocha)       // šum -> smazat
            {
                for (int p : pixelyBunky) { stitky[p] = -1; maska.px[p] = 0; }
                continue;
            }

            Bunka b;
            b.id = dalsiID++;
            b.plocha = pixelyBunky.size();
            for (int p : pixelyBunky) { b.sx += p % W; b.sy += p / W; }
            b.sx /= b.plocha;
            b.sy /= b.plocha;
            bunky.push_back(b);
        }

    for (auto& s : stitky) if (s < 0) s = 0;
    return bunky;
}

// ---------------------------------------------------------------------
//  6) Sousedé - každá buňka se spojí se svými k nejbližšími buňkami.
//     Dvojice se ukládají jen jednou (set zabrání duplicitám A-B / B-A).
// ---------------------------------------------------------------------
std::vector<Dvojice> najdiSousedy(const std::vector<Bunka>& bunky, int k)
{
    std::set<std::pair<int, int>> uz;
    std::vector<Dvojice> vysledek;
    int n = bunky.size();

    for (int i = 0; i < n; i++)
    {
        std::vector<std::pair<double, int>> vzd;   // (vzdálenost, index)
        for (int j = 0; j < n; j++)
            if (j != i)
                vzd.push_back({std::hypot(bunky[i].sx - bunky[j].sx,
                                          bunky[i].sy - bunky[j].sy), j});
        std::sort(vzd.begin(), vzd.end());

        for (int m = 0; m < k && m < (int)vzd.size(); m++)
        {
            int a = std::min(i, vzd[m].second), b = std::max(i, vzd[m].second);
            if (uz.insert({a, b}).second)          // true = dvojice je nová
                vysledek.push_back({a, b, vzd[m].first});
        }
    }
    return vysledek;
}

// ---------------------------------------------------------------------
//  Kreslicí funkce
// ---------------------------------------------------------------------
ObrazekRGB naRGB(const Obrazek& img)      // R = G = B = jas
{
    ObrazekRGB out;
    out.sirka = img.sirka; out.vyska = img.vyska;
    out.px.resize(3 * img.px.size());
    for (size_t i = 0; i < img.px.size(); i++)
        out.px[3 * i] = out.px[3 * i + 1] = out.px[3 * i + 2] = img.px[i];
    return out;
}

// Obrys = pixel buňky, jehož soused má jiný štítek (pozadí nebo jiná buňka)
void kresliObrysy(ObrazekRGB& img, const std::vector<int>& stitky)
{
    int W = img.sirka, H = img.vyska;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            int s = stitky[y * W + x];
            if (s == 0) continue;
            bool hrana = (x == 0 || y == 0 || x == W - 1 || y == H - 1) ||
                         stitky[y * W + x - 1] != s || stitky[y * W + x + 1] != s ||
                         stitky[(y - 1) * W + x] != s || stitky[(y + 1) * W + x] != s;
            if (hrana) img.nastav(x, y, 0, 255, 0);
        }
}

// Bresenhamův algoritmus - úsečka z celých pixelů
void kresliCaru(ObrazekRGB& img, int x0, int y0, int x1, int y1,
                unsigned char r, unsigned char g, unsigned char b)
{
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int chyba = dx + dy;
    while (true)
    {
        img.nastav(x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * chyba;
        if (e2 >= dy) { chyba += dy; x0 += sx; }
        if (e2 <= dx) { chyba += dx; y0 += sy; }
    }
}

void kresliKrizek(ObrazekRGB& img, int x, int y, int vel)
{
    for (int i = -vel; i <= vel; i++)
    {
        img.nastav(x + i, y, 255, 0, 0);
        img.nastav(x, y + i, 255, 0, 0);
    }
}

// Mini font 3x5 pixelů pro číslice 0-9 (každý řádek = 3 bity)
void kresliCislo(ObrazekRGB& img, int x, int y, int cislo, int meritko)
{
    static const int font[10][5] = {
        {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
        {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};
    std::string text = std::to_string(cislo);
    for (size_t c = 0; c < text.size(); c++)
    {
        int d = text[c] - '0';
        for (int radek = 0; radek < 5; radek++)
            for (int sl = 0; sl < 3; sl++)
                if (font[d][radek] & (4 >> sl))
                    for (int my = 0; my < meritko; my++)
                        for (int mx = 0; mx < meritko; mx++)
                            img.nastav(x + (c * 4 + sl) * meritko + mx,
                                       y + radek * meritko + my, 255, 255, 0);
    }
}

// ---------------------------------------------------------------------
//  Uložení výsledků
// ---------------------------------------------------------------------
void ulozCSV(const std::vector<Bunka>& bunky, const std::vector<Dvojice>& dvojice,
             const std::string& slozka)
{
    std::ofstream f1(slozka + "bunky.csv");
    f1 << "id;stred_x;stred_y;plocha_px\n";
    for (auto& b : bunky) f1 << b.id << ";" << b.sx << ";" << b.sy << ";" << b.plocha << "\n";

    std::ofstream f2(slozka + "vzdalenosti.csv");
    f2 << "id_1;id_2;vzdalenost_px\n";
    for (auto& d : dvojice) f2 << bunky[d.a].id << ";" << bunky[d.b].id << ";" << d.d << "\n";
}

// =====================================================================
//  HLAVNÍ PROGRAM
// =====================================================================
int main()
{
    const std::string VSTUP  = "data/input/mikroskop_bunky.jpg";
    const std::string VYSTUP = "data/output/";
    const int ROZMAZANI = 1;     // poloměr rozmazání proti šumu [px]
    const int OKOLI = 15;        // poloměr okolí pro lokální průměr [px]
    const int C_LOKALNI = 3;     // o kolik musí být pixel světlejší než okolí
    const int MIN_PLOCHA = 80;   // menší objekty = šum [px]
    const int K_SOUSEDU = 2;     // počet nejbližších sousedů

    // 1) načtení
    Obrazek snimek;
    if (!nactiObrazek(VSTUP, snimek))
    {
        std::cout << "Obrazek se nepodarilo nacist: " << VSTUP << std::endl;
        return 1;
    }
    std::cout << "Nacteno " << snimek.sirka << " x " << snimek.vyska << " px\n";

    // 2-4) segmentace
    Obrazek hladky = rozmaz(snimek, ROZMAZANI);
    Obrazek okoli  = rozmaz(hladky, OKOLI);                 // lokální průměr jasu
    int prahLog = otsuPrah(logaritmuj(hladky));             // Otsu v log. škále
    int prah = std::exp(prahLog * std::log(256.0) / 255.0) - 1; // zpět na jas 0-255
    Obrazek maska = prahuj(hladky, okoli, prah, C_LOKALNI);
    vyplnDiry(maska);
    std::cout << "Otsu prah: " << prah << "\n";

    // 5) buňky a těžiště
    std::vector<int> stitky;
    std::vector<Bunka> bunky = oznacBunky(maska, stitky, MIN_PLOCHA);
    std::cout << "Nalezeno bunek: " << bunky.size() << "\n";

    // 6) sousedé
    std::vector<Dvojice> dvojice = najdiSousedy(bunky, K_SOUSEDU);

    // --- výstup 1: maska
    stbi_write_png((VYSTUP + "1_maska.png").c_str(), maska.sirka, maska.vyska, 1,
                   maska.px.data(), maska.sirka);

    // --- výstup 2: obrysy
    ObrazekRGB obrysy = naRGB(snimek);
    kresliObrysy(obrysy, stitky);
    stbi_write_png((VYSTUP + "2_obrysy.png").c_str(), obrysy.sirka, obrysy.vyska, 3,
                   obrysy.px.data(), 3 * obrysy.sirka);

    // --- výstup 3: obrysy + středy + spojnice + kóty
    ObrazekRGB kota = obrysy;
    for (auto& d : dvojice)
        kresliCaru(kota, bunky[d.a].sx, bunky[d.a].sy, bunky[d.b].sx, bunky[d.b].sy, 0, 160, 255);
    for (auto& b : bunky)
        kresliKrizek(kota, b.sx, b.sy, 3);
    for (auto& d : dvojice)              // popisek doprostřed úsečky
        kresliCislo(kota, (bunky[d.a].sx + bunky[d.b].sx) / 2 + 2,
                          (bunky[d.a].sy + bunky[d.b].sy) / 2 + 2, std::lround(d.d), 1);
    stbi_write_png((VYSTUP + "3_vzdalenosti.png").c_str(), kota.sirka, kota.vyska, 3,
                   kota.px.data(), 3 * kota.sirka);

    ulozCSV(bunky, dvojice, VYSTUP);
    std::cout << "Hotovo, vysledky jsou ve slozce " << VYSTUP << std::endl;
    return 0;
}
