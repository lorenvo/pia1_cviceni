// #####################################################################
//  simulace.h - SIMULACE POHYBU VYSEGMENTOVANÝCH BUNĚK
// #####################################################################
//  Buňky v kultuře se nepohybují tepelným (Brownovým) pohybem - jsou
//  přichycené k podložce a posouvají se AKTIVNĚ (aktin, myozin, ATP).
//  Chvíli si drží směr a ten se postupně náhodně mění = PERZISTENCE.
//
//  Používáme dva modely:
//
//  1) BROWNŮV POHYB (pro srovnání)
//     každý krok je náhodný a nezávislý na předchozím:
//        x(t+1) = x(t) + sqrt(2D) * xi            xi ~ N(0,1)
//
//  2) PERZISTENTNÍ NÁHODNÁ PROCHÁZKA (biologicky správný model)
//     náhodně se mění RYCHLOST, ne přímo poloha:
//        v(t+1) = alfa * v(t) + sigma * sqrt(1 - alfa^2) * xi
//        x(t+1) = x(t) + v(t+1)
//        alfa  = exp(-1 / P)     P = doba perzistence [snímky]
//        sigma = S / sqrt(2)     S = typická rychlost [px/snímek]
//
//  Oba modely mají stejný difuzní koeficient D = S^2 * P / 2, takže
//  se buňky za dlouhou dobu dostanou podobně daleko - liší se TVAREM
//  dráhy (Brown = klikatá, perzistentní = plynulé oblouky).
//
//  ODPUZOVÁNÍ: buňky jsou brány jako kruhy (poloměr z plochy jádra);
//  když se překryjí, odstrčí se od sebe a změní směr pohybu
//  (zjednodušená kontaktní inhibice pohybu).
// #####################################################################
#pragma once
#include "obrazek.h"
#include "segmentace.h"
#include <vector>

// Jedna poloha buňky v čase
struct Bod { int t; double x, y; };

// Trajektorie = polohy jedné buňky ve všech snímcích simulace
struct Trajektorie
{
    int id;                      // stejné ID jako buňka ze segmentace
    std::vector<Bod> body;
};

// Statistika pohybu jedné trajektorie
//   draha    = součet délek všech kroků (kolik buňka skutečně ušla)
//   posunuti = vzdálenost začátek -> konec (vzdušnou čarou)
//   rychlost = draha / počet kroků [px/snímek]
//   primost  = posunuti / draha (1 = přímý pohyb, ~0 = náhodné bloudění)
struct Statistika { double draha = 0, posunuti = 0, rychlost = 0, primost = 0; };

// Model pohybu
enum class Model { Brown, Perzistentni };

// Nasimuluje pohyb všech buněk; vrátí jejich trajektorie
std::vector<Trajektorie> simulujPohyb(const std::vector<Bunka>& bunky, Model model,
                                      int sirka, int vyska);

// Vykreslí snímky simulace: buňky vystřižené z původního snímku
// se vloží na nové polohy na pozadí se šumem
std::vector<Obrazek> vykresliSimulaci(const Obrazek& snimek, const Segmentace& seg,
                                      const std::vector<Trajektorie>& traj);

// Spočítá statistiku pohybu jedné trajektorie
Statistika spocitejStatistiku(const Trajektorie& tr);
