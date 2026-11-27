// #####################################################################
//  sousede.h - NEJBLIŽŠÍ SOUSEDÉ A JEJICH VZDÁLENOSTI
// #####################################################################
#pragma once
#include "segmentace.h"
#include <vector>

// Dvojice sousedních buněk: indexy do seznamu buněk a vzdálenost těžišť
struct Dvojice { int a, b; double d; };

// Každou buňku spojí s jejími k nejbližšími buňkami (podle těžišť).
// Dvojice A-B / B-A se vrátí jen jednou.
std::vector<Dvojice> najdiSousedy(const std::vector<Bunka>& bunky, int k);
