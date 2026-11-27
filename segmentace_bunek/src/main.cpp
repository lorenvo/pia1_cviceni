// #####################################################################
//
//   SEGMENTACE A SIMULACE POHYBU BUNĚČNÝCH JADER
//
// #####################################################################
//
//  A) ANALÝZA SNÍMKU
//     Najde jádra buněk, spočítá jejich těžiště a vzdálenosti
//     k nejbližším sousedům.
//       vstup : data/input/mikroskop_bunky.jpg
//       výstup: data/output/1_maska.png, 2_obrysy.png, 3_vzdalenosti.png,
//               bunky.csv, vzdalenosti.csv
//
//  B) SIMULACE POHYBU
//     Vysegmentované buňky se rozpohybují dvěma modely:
//       - Brownův pohyb (pro srovnání)
//       - perzistentní náhodná procházka (biologicky správný model)
//       výstup: data/output/4_drahy_brown.png, 4_drahy_perzistentni.png,
//               animace_brown/f000.png ..., animace_perzistentni/f000.png ...,
//               brown_trajektorie.csv, brown_souhrn.csv,
//               perzistentni_trajektorie.csv, perzistentni_souhrn.csv
//
//  MODULY (složka src/)
//    parametry.h          všechny nastavitelné konstanty
//    obrazek.h/.cpp       datové struktury obrázků, načítání a ukládání
//    segmentace.h/.cpp    rozpoznání buněk v jednom snímku
//    sousede.h/.cpp       nejbližší sousedé a vzdálenosti
//    simulace.h/.cpp      simulace pohybu buněk + statistika pohybu
//    kresleni.h/.cpp      obrysy, čáry, křížky, čísla, trajektorie
//    export.h/.cpp        CSV tabulky
//    main.cpp             tento soubor - řídí celý výpočet
//
// #####################################################################
#include "parametry.h"
#include "obrazek.h"
#include "segmentace.h"
#include "sousede.h"
#include "simulace.h"
#include "kresleni.h"
#include "export.h"

#include <iostream>
#include <cstdio>      // snprintf
#include <cmath>       // lround
#include <filesystem>  // vytvoření výstupních složek

// =====================================================================
//  A) ANALÝZA SNÍMKU
//  sousedé -> 3 obrázky + 2 tabulky
// =====================================================================
void analyzaSnimku(const Obrazek& snimek, const Segmentace& s)
{
    std::cout << "=== A) ANALYZA SNIMKU ===\n";
    std::cout << "Otsu prah: " << s.prah << "\n";
    std::cout << "Nalezeno bunek: " << s.bunky.size() << "\n";

    std::vector<Dvojice> dvojice = najdiSousedy(s.bunky, K_SOUSEDU);

    // --- výstup 1: binární maska ---
    ulozPNG(VYSTUP + "1_maska.png", s.maska);

    // --- výstup 2: snímek se zelenými obrysy ---
    ObrazekRGB obrysy = naRGB(snimek);
    kresliObrysy(obrysy, s.stitky);
    ulozPNG(VYSTUP + "2_obrysy.png", obrysy);

    // --- výstup 3: obrysy + spojnice + středy + kóty ---
    ObrazekRGB kota = obrysy;
    const std::vector<Bunka>& bunky = s.bunky;
    for (auto& d : dvojice)              // modré spojnice
        kresliCaru(kota, bunky[d.a].sx, bunky[d.a].sy, bunky[d.b].sx, bunky[d.b].sy, 0, 160, 255);
    for (auto& b : bunky)                // červené křížky v těžištích
        kresliKrizek(kota, b.sx, b.sy, 3);
    for (auto& d : dvojice)              // žluté popisky doprostřed úsečky
        kresliCislo(kota, (bunky[d.a].sx + bunky[d.b].sx) / 2 + 2,
                          (bunky[d.a].sy + bunky[d.b].sy) / 2 + 2, std::lround(d.d), 1);
    ulozPNG(VYSTUP + "3_vzdalenosti.png", kota);

    // --- výstup 4: tabulky ---
    ulozCSV(bunky, dvojice, VYSTUP);
}

// =====================================================================
//  B) SIMULACE POHYBU
//  pro každý model: simulace -> snímky -> obrázek drah + animace
//  -> tabulky + průměrná statistika do konzole
// =====================================================================
void simulacePohybu(const Obrazek& snimek, const Segmentace& s)
{
    std::cout << "\n=== B) SIMULACE POHYBU ===\n";
    std::cout << "Snimku: " << SIM_SNIMKU << ", rychlost S = " << SIM_RYCHLOST
              << " px/snimek, perzistence P = " << SIM_PERZISTENCE << " snimku\n";

    Model modely[2]       = {Model::Brown, Model::Perzistentni};
    std::string nazvy[2]  = {"brown", "perzistentni"};

    for (int m = 0; m < 2; m++)
    {
        const std::string& nazev = nazvy[m];

        // --- simulace a vykreslení snímků ---
        std::vector<Trajektorie> traj = simulujPohyb(s.bunky, modely[m], snimek.sirka, snimek.vyska);
        std::vector<Obrazek> snimky = vykresliSimulaci(snimek, s, traj);
        int posledni = SIM_SNIMKU - 1;

        // --- výstup 1: celé dráhy přes původní (ztmavený) snímek ---
        ObrazekRGB drahy = naRGB(snimek, 60);
        for (auto& tr : traj)
        {
            kresliTrajektorii(drahy, tr, 0, posledni);
            unsigned char r, g, b;
            barvaTrajektorie(tr.id, r, g, b);
            kresliKrizek(drahy, tr.body.back().x, tr.body.back().y, 2, r, g, b);  // konec dráhy
        }
        ulozPNG(VYSTUP + "4_drahy_" + nazev + ".png", drahy);

        // --- výstup 2: animace - každý snímek s "ocásky" posledních poloh ---
        std::filesystem::create_directories(VYSTUP + "animace_" + nazev);
        for (int t = 0; t <= posledni; t++)
        {
            ObrazekRGB snimekRGB = naRGB(snimky[t]);
            for (auto& tr : traj)
            {
                kresliTrajektorii(snimekRGB, tr, t - DELKA_OCASKU, t);
                unsigned char r, g, b;
                barvaTrajektorie(tr.id, r, g, b);
                kresliKrizek(snimekRGB, tr.body[t].x, tr.body[t].y, 3, r, g, b);
            }
            char jmeno[32];
            std::snprintf(jmeno, sizeof(jmeno), "/f%03d.png", t);
            ulozPNG(VYSTUP + "animace_" + nazev + jmeno, snimekRGB);
        }

        // --- výstup 3: tabulky ---
        ulozTrajektorie(traj, VYSTUP, nazev);

        // --- průměrná statistika do konzole ---
        Statistika prumer;
        for (auto& tr : traj)
        {
            Statistika st = spocitejStatistiku(tr);
            prumer.draha += st.draha;     prumer.posunuti += st.posunuti;
            prumer.rychlost += st.rychlost; prumer.primost += st.primost;
        }
        int n = traj.size();
        std::cout << "  " << nazev << ": prumerna draha " << prumer.draha / n
                  << " px, posunuti " << prumer.posunuti / n
                  << " px, primost " << prumer.primost / n << "\n";
    }
}

// =====================================================================
//  main - načte snímek, segmentuje ho a spustí obě části
// =====================================================================
int main()
{
    std::filesystem::create_directories(VYSTUP);   // vytvoří data/output, pokud chybí

    // --- načtení a segmentace (společné pro A i B) ---
    Obrazek snimek;
    if (!nactiObrazek(VSTUP_SNIMEK, snimek))
    {
        std::cout << "Obrazek se nepodarilo nacist: " << VSTUP_SNIMEK << std::endl;
        return 1;
    }
    std::cout << "Nacteno " << snimek.sirka << " x " << snimek.vyska << " px\n";
    Segmentace s = segmentuj(snimek);

    analyzaSnimku(snimek, s);      // část A
    simulacePohybu(snimek, s);     // část B

    std::cout << "\nHotovo, vysledky jsou ve slozce " << VYSTUP << std::endl;
    return 0;
}
