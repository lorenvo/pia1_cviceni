// #####################################################################
//  parametry.h - VŠECHNY NASTAVITELNÉ PARAMETRY NA JEDNOM MÍSTĚ
// #####################################################################
//  Konstanty jsou "inline constexpr" / "inline const" (C++17): hlavičku
//  může načíst více .cpp souborů a konstanta přesto existuje jen jednou.
// #####################################################################
#pragma once
#include <string>

// PROJEKT_DIR doplní CMake (cesta ke složce projektu), takže program
// najde data/ i když je spuštěn ze složky build/.
#ifndef PROJEKT_DIR
#define PROJEKT_DIR ""
#endif

// --- cesty ---
inline const std::string VSTUP_SNIMEK = std::string(PROJEKT_DIR) + "data/input/mikroskop_bunky.jpg";
inline const std::string VYSTUP       = std::string(PROJEKT_DIR) + "data/output/";

// --- segmentace ---
inline constexpr int ROZMAZANI  = 1;    // poloměr rozmazání proti šumu [px]
inline constexpr int OKOLI      = 15;   // poloměr okolí pro lokální průměr [px]
inline constexpr int C_LOKALNI  = 3;    // o kolik musí být pixel světlejší než okolí
inline constexpr int MIN_PLOCHA = 80;   // menší objekty = šum [px]

// --- sousedé ---
inline constexpr int K_SOUSEDU = 2;     // počet nejbližších sousedů

// --- simulace pohybu ---
//  Orientačně: při 1 snímku = 10 min a 1 px ~ 0,65 um odpovídá
//  S = 2 px/snímek rychlosti ~ 8 um/h a P = 10 snímků perzistenci
//  ~ 100 min - řádově hodnoty měřené u buněk v kultuře.
inline constexpr int    SIM_SNIMKU      = 60;   // počet snímků simulace
inline constexpr double SIM_RYCHLOST    = 2.0;  // S - typická rychlost buňky [px/snímek]
inline constexpr double SIM_PERZISTENCE = 10.0; // P - jak dlouho si buňka drží směr [snímky]
inline constexpr unsigned SIM_SEED      = 42;   // seed generátoru (stejný = stejný výsledek)
inline constexpr double SIM_SUM         = 1.5;  // šum pozadí ve snímcích simulace [jas]
inline constexpr bool   SIM_ODPUZOVANI  = true; // true = buňky se nepřekrývají (odstrčí se)
inline constexpr int    SIM_ITERACE_ODPUZOVANI = 3;  // kolikrát se překryvy kontrolují v kroku
inline constexpr int    DELKA_OCASKU    = 10;   // kolik posledních poloh ukazuje animace
