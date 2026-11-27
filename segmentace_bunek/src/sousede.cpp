// #####################################################################
//  sousede.cpp - NEJBLIŽŠÍ SOUSEDÉ A JEJICH VZDÁLENOSTI
// #####################################################################
#include "sousede.h"
#include <set>
#include <cmath>
#include <algorithm>

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
