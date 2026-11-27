// #####################################################################
//  export.cpp - UKLÁDÁNÍ TABULEK (CSV, oddělovač ;)
// #####################################################################
#include "export.h"
#include <fstream>

// ---------------------------------------------------------------------
//  Buňky a vzdálenosti sousedů
// ---------------------------------------------------------------------
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

// ---------------------------------------------------------------------
//  Trajektorie simulace
//    <nazev>_trajektorie.csv - jedna řádka = jedna buňka v jednom snímku
//    <nazev>_souhrn.csv      - jedna řádka = jedna buňka + statistika pohybu
// ---------------------------------------------------------------------
void ulozTrajektorie(const std::vector<Trajektorie>& traj, const std::string& slozka,
                     const std::string& nazev)
{
    std::ofstream f1(slozka + nazev + "_trajektorie.csv");
    f1 << "id_bunky;snimek;x;y\n";
    for (auto& tr : traj)
        for (auto& p : tr.body)
            f1 << tr.id << ";" << p.t << ";" << p.x << ";" << p.y << "\n";

    std::ofstream f2(slozka + nazev + "_souhrn.csv");
    f2 << "id_bunky;draha_px;posunuti_px;rychlost_px_za_snimek;primost\n";
    for (auto& tr : traj)
    {
        Statistika s = spocitejStatistiku(tr);
        f2 << tr.id << ";" << s.draha << ";" << s.posunuti << ";"
           << s.rychlost << ";" << s.primost << "\n";
    }
}
