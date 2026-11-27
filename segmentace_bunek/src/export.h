// #####################################################################
//  export.h - UKLÁDÁNÍ TABULEK (CSV, oddělovač ;)
// #####################################################################
#pragma once
#include "segmentace.h"
#include "sousede.h"
#include "simulace.h"
#include <string>
#include <vector>

// bunky.csv (ID, těžiště, plocha) + vzdalenosti.csv (dvojice sousedů)
void ulozCSV(const std::vector<Bunka>& bunky, const std::vector<Dvojice>& dvojice,
             const std::string& slozka);

// <nazev>_trajektorie.csv (všechny polohy) + <nazev>_souhrn.csv (statistika)
void ulozTrajektorie(const std::vector<Trajektorie>& traj, const std::string& slozka,
                     const std::string& nazev);
