// =====================================================================
//  SEGMENTACE BUNĚČNÝCH JADER Z MIKROSKOPICKÉHO SNÍMKU
// ---------------------------------------------------------------------
//  Postup: načtení -> vyhlazení -> automatický práh (Otsu) -> maska
//          -> vyplnění děr -> označení buněk + těžiště -> sousedé
//          -> vykreslení -> uložení
//
//  VSTUP : data/input/mikroskop_bunky.jpg (šedotónový snímek)
//  VÝSTUP: data/output/1_maska.png        - binární maska (bílá = buňka)
//          data/output/2_obrysy.png       - snímek se zelenými obrysy
//          data/output/3_vzdalenosti.png  - středy, spojnice, kóty
//          data/output/bunky.csv          - ID, těžiště, plocha
//          data/output/vzdalenosti.csv    - dvojice sousedů a vzdálenost
// =====================================================================

// ---------------------------------------------------------------------
//  KNIHOVNY
//  stb_image       - načtení JPG/PNG
//  stb_image_write - uložení PNG
//  (#define ..._IMPLEMENTATION vloží do tohoto souboru i kód knihovny)
// ---------------------------------------------------------------------
#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/stb_image_write.h"

#include <iostream>   // výpis do konzole (cout)
#include <fstream>    // zápis do souborů (CSV)
#include <vector>     // dynamická pole
#include <queue>      // fronta pro flood fill (BFS)
#include <set>        // množina bez duplicit (dvojice sousedů)
#include <cmath>      // log, exp, hypot
#include <string>     // textové řetězce
#include <algorithm>  // sort, min, max

// =====================================================================
//  DATOVÉ STRUKTURY
// =====================================================================

// Šedotónový obrázek: 1 bajt na pixel (0 = černá, 255 = bílá).
// Pixely jsou uložené po řádcích v jednom poli: index = y * sirka + x
struct Obrazek
{
    int sirka = 0, vyska = 0;
    std::vector<unsigned char> px;
    unsigned char& at(int x, int y) { return px[y * sirka + x]; }   // přístup přes (x, y)
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

// Jedna nalezená buňka
struct Bunka
{
    int id;
    int plocha = 0;        // počet pixelů
    double sx = 0, sy = 0; // těžiště [px]
};

// Dvojice sousedních buněk: indexy do seznamu buněk a vzdálenost těžišť
struct Dvojice { int a, b; double d; };

// =====================================================================
//  1) NAČTENÍ SNÍMKU
//  stb_image načte JPG/PNG a rovnou ho převede na šedotón (1 kanál)
// =====================================================================
bool nactiObrazek(const std::string& cesta, Obrazek& img)
{
    int kanaly;
    unsigned char* data = stbi_load(cesta.c_str(), &img.sirka, &img.vyska, &kanaly, 1);
    if (!data) return false;                               // soubor nenalezen
    img.px.assign(data, data + img.sirka * img.vyska);     // kopie do vektoru
    stbi_image_free(data);                                 // uvolnění paměti knihovny
    return true;
}

// =====================================================================
//  2) ROZMAZÁNÍ (průměrovací filtr)
//  Každý pixel nahradí průměrem čtverce (2r+1)x(2r+1) kolem sebe.
//  Malé r = potlačení šumu, velké r = průměrný jas okolí.
// =====================================================================
Obrazek rozmaz(Obrazek& img, int r)
{
    Obrazek out = img;                         // výsledek se píše do kopie
    for (int y = 0; y < img.vyska; y++)
        for (int x = 0; x < img.sirka; x++)    // pro každý pixel...
        {
            int suma = 0, n = 0;
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++)   // ...projdi jeho okolí
                {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= img.sirka || yy >= img.vyska) continue;
                    suma += img.at(xx, yy);
                    n++;                           // počet skutečně sečtených pixelů
                }
            out.at(x, y) = suma / n;               // průměr
        }
    return out;
}

// =====================================================================
//  2b) LOGARITMICKÁ TRANSFORMACE JASU
//  Roztáhne tmavé hodnoty, stlačí světlé -> Otsu nepřehlédne
//  slabě svítící jádra. Používá se jen pro výpočet prahu.
// =====================================================================
Obrazek logaritmuj(const Obrazek& img)
{
    Obrazek out = img;
    for (auto& v : out.px) v = 255.0 * std::log(1.0 + v) / std::log(256.0);
    return out;
}

// =====================================================================
//  3) OTSUOVA METODA - automatický práh
//  Vyzkouší všechny prahy 0-255 a vybere ten, při kterém se skupiny
//  "pozadí" a "buňky" nejvíce liší (max. mezitřídní rozptyl).
// =====================================================================
int otsuPrah(const Obrazek& img)
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

// =====================================================================
//  4) PRAHOVÁNÍ -> BINÁRNÍ MASKA (255 = buňka, 0 = pozadí)
//  Pixel je buňka, když splní OBĚ podmínky:
//    a) je světlejší než globální práh (Otsu)   -> není to pozadí
//    b) je světlejší než průměr svého okolí + C -> oddělí dotýkající
//       se jádra (mezera mezi nimi je tmavší než okolí)
// =====================================================================
Obrazek prahuj(const Obrazek& img, const Obrazek& lokalniPrumer, int prah, int C)
{
    Obrazek maska = img;
    for (size_t i = 0; i < img.px.size(); i++)
        maska.px[i] = (img.px[i] > prah && img.px[i] > lokalniPrumer.px[i] + C) ? 255 : 0;
    return maska;
}

// =====================================================================
//  4b) VYPLNĚNÍ DĚR
//  Tmavá jadérka vytvoří v masce díry. Pozadí "zaplavíme" od okraje
//  obrázku; černé pixely, kam se záplava nedostala, jsou díry uvnitř
//  buňky -> přebarvíme je na bílou.
// =====================================================================
void vyplnDiry(Obrazek& maska)
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

// =====================================================================
//  5) OZNAČENÍ BUNĚK (connected components) + TĚŽIŠTĚ
//  Každá souvislá bílá oblast dostane vlastní ID (flood fill / BFS).
//  Oblasti menší než minPlocha se zahodí jako šum.
//  Těžiště S = [průměr X, průměr Y] všech pixelů buňky.
// =====================================================================
std::vector<Bunka> oznacBunky(Obrazek& maska, std::vector<int>& stitky, int minPlocha)
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

// =====================================================================
//  6) HLEDÁNÍ SOUSEDŮ
//  Každá buňka se spojí se svými k nejbližšími buňkami (podle
//  vzdálenosti těžišť). Dvojice A-B / B-A se uloží jen jednou.
// =====================================================================
std::vector<Dvojice> najdiSousedy(const std::vector<Bunka>& bunky, int k)
{
    std::set<std::pair<int, int>> uz;          // už uložené dvojice
    std::vector<Dvojice> vysledek;
    int n = bunky.size();

    for (int i = 0; i < n; i++)
    {
        // vzdálenosti buňky i ke všem ostatním: (vzdálenost, index)
        std::vector<std::pair<double, int>> vzd;
        for (int j = 0; j < n; j++)
            if (j != i)
                vzd.push_back({std::hypot(bunky[i].sx - bunky[j].sx,
                                          bunky[i].sy - bunky[j].sy), j});
        std::sort(vzd.begin(), vzd.end());     // od nejbližší

        // prvních k = nejbližší sousedé
        for (int m = 0; m < k && m < (int)vzd.size(); m++)
        {
            int a = std::min(i, vzd[m].second), b = std::max(i, vzd[m].second);
            if (uz.insert({a, b}).second)          // true = dvojice je nová
                vysledek.push_back({a, b, vzd[m].first});
        }
    }
    return vysledek;
}

// =====================================================================
//  7) KRESLICÍ FUNKCE
// =====================================================================

// Převod šedotónu na RGB (R = G = B = jas), aby šlo kreslit barevně
ObrazekRGB naRGB(const Obrazek& img)
{
    ObrazekRGB out;
    out.sirka = img.sirka; out.vyska = img.vyska;
    out.px.resize(3 * img.px.size());
    for (size_t i = 0; i < img.px.size(); i++)
        out.px[3 * i] = out.px[3 * i + 1] = out.px[3 * i + 2] = img.px[i];
    return out;
}

// Obrysy buněk (zeleně): pixel buňky, jehož soused má jiné ID
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

// Úsečka - Bresenhamův algoritmus (jen celočíselné operace)
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

// Značka těžiště - červený křížek
void kresliKrizek(ObrazekRGB& img, int x, int y, int vel)
{
    for (int i = -vel; i <= vel; i++)
    {
        img.nastav(x + i, y, 255, 0, 0);   // vodorovná část
        img.nastav(x, y + i, 255, 0, 0);   // svislá část
    }
}

// Kóta - číslo vykreslené vlastním fontem 3x5 px (žlutě)
// Každý řádek číslice = 3 bity, např. 7 = 111 (plný), 5 = 101 (krajní)
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

// =====================================================================
//  8) ULOŽENÍ TABULEK (CSV, oddělovač ;)
// =====================================================================
void ulozCSV(const std::vector<Bunka>& bunky, const std::vector<Dvojice>& dvojice,
             const std::string& slozka)
{
    // seznam buněk
    std::ofstream f1(slozka + "bunky.csv");
    f1 << "id;stred_x;stred_y;plocha_px\n";
    for (auto& b : bunky) f1 << b.id << ";" << b.sx << ";" << b.sy << ";" << b.plocha << "\n";

    // seznam sousedních dvojic
    std::ofstream f2(slozka + "vzdalenosti.csv");
    f2 << "id_1;id_2;vzdalenost_px\n";
    for (auto& d : dvojice) f2 << bunky[d.a].id << ";" << bunky[d.b].id << ";" << d.d << "\n";
}

// =====================================================================
//  HLAVNÍ PROGRAM
// =====================================================================
int main()
{
    // --- PARAMETRY ---
    const std::string VSTUP  = "data/input/mikroskop_bunky.jpg";
    const std::string VYSTUP = "data/output/";
    const int ROZMAZANI = 1;     // poloměr rozmazání proti šumu [px]
    const int OKOLI = 15;        // poloměr okolí pro lokální průměr [px]
    const int C_LOKALNI = 3;     // o kolik musí být pixel světlejší než okolí
    const int MIN_PLOCHA = 80;   // menší objekty = šum [px]
    const int K_SOUSEDU = 2;     // počet nejbližších sousedů

    // --- 1) NAČTENÍ ---
    Obrazek snimek;
    if (!nactiObrazek(VSTUP, snimek))
    {
        std::cout << "Obrazek se nepodarilo nacist: " << VSTUP << std::endl;
        return 1;
    }
    std::cout << "Nacteno " << snimek.sirka << " x " << snimek.vyska << " px\n";

    // --- 2-4) SEGMENTACE ---
    Obrazek hladky = rozmaz(snimek, ROZMAZANI);
    Obrazek okoli  = rozmaz(hladky, OKOLI);                 // lokální průměr jasu
    int prahLog = otsuPrah(logaritmuj(hladky));             // Otsu v log. škále
    int prah = std::exp(prahLog * std::log(256.0) / 255.0) - 1; // zpět na jas 0-255
    Obrazek maska = prahuj(hladky, okoli, prah, C_LOKALNI);
    vyplnDiry(maska);
    std::cout << "Otsu prah: " << prah << "\n";

    // --- 5) BUŇKY A TĚŽIŠTĚ ---
    std::vector<int> stitky;
    std::vector<Bunka> bunky = oznacBunky(maska, stitky, MIN_PLOCHA);
    std::cout << "Nalezeno bunek: " << bunky.size() << "\n";

    // --- 6) SOUSEDÉ ---
    std::vector<Dvojice> dvojice = najdiSousedy(bunky, K_SOUSEDU);

    // --- VÝSTUP 1: binární maska ---
    stbi_write_png((VYSTUP + "1_maska.png").c_str(), maska.sirka, maska.vyska, 1,
                   maska.px.data(), maska.sirka);

    // --- VÝSTUP 2: snímek se zelenými obrysy ---
    ObrazekRGB obrysy = naRGB(snimek);
    kresliObrysy(obrysy, stitky);
    stbi_write_png((VYSTUP + "2_obrysy.png").c_str(), obrysy.sirka, obrysy.vyska, 3,
                   obrysy.px.data(), 3 * obrysy.sirka);

    // --- VÝSTUP 3: obrysy + spojnice + středy + kóty ---
    ObrazekRGB kota = obrysy;
    for (auto& d : dvojice)              // modré spojnice
        kresliCaru(kota, bunky[d.a].sx, bunky[d.a].sy, bunky[d.b].sx, bunky[d.b].sy, 0, 160, 255);
    for (auto& b : bunky)                // červené křížky v těžištích
        kresliKrizek(kota, b.sx, b.sy, 3);
    for (auto& d : dvojice)              // žluté popisky doprostřed úsečky
        kresliCislo(kota, (bunky[d.a].sx + bunky[d.b].sx) / 2 + 2,
                          (bunky[d.a].sy + bunky[d.b].sy) / 2 + 2, std::lround(d.d), 1);
    stbi_write_png((VYSTUP + "3_vzdalenosti.png").c_str(), kota.sirka, kota.vyska, 3,
                   kota.px.data(), 3 * kota.sirka);

    // --- VÝSTUP 4: tabulky ---
    ulozCSV(bunky, dvojice, VYSTUP);
    std::cout << "Hotovo, vysledky jsou ve slozce " << VYSTUP << std::endl;
    return 0;
}
