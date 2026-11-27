// #####################################################################
//  simulace.cpp - SIMULACE POHYBU VYSEGMENTOVANÝCH BUNĚK
// #####################################################################
#include "simulace.h"
#include "parametry.h"
#include <random>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------
//  Odraz od okraje obrázku
//  Když buňka přejde přes okraj, vrátí se o stejný kus zpět
//  a její rychlost v tomto směru se otočí (jako kulečníková koule).
// ---------------------------------------------------------------------
static void odraz(double& poloha, double& rychlost, int rozmer)
{
    if (poloha < 0)          { poloha = -poloha;                     rychlost = -rychlost; }
    if (poloha > rozmer - 1) { poloha = 2.0 * (rozmer - 1) - poloha; rychlost = -rychlost; }
}

// ---------------------------------------------------------------------
//  Odpuzování buněk (zjednodušená kontaktní inhibice pohybu)
//  Buňka = kruh o poloměru r. Když jsou dvě buňky blíž než r1 + r2,
//  překrývají se:
//    - obě se posunou od sebe po spojnici středů, každá o polovinu
//      překryvu -> po posunu se právě dotýkají,
//    - pokud se buňka pohybovala směrem k té druhé, odrazí se
//      (složka rychlosti ve směru srážky se otočí) - buňka po
//      kontaktu změní směr, jako skutečné buňky.
//  Posunutím jedné dvojice může vzniknout překryv jinde, proto se
//  kontrola opakuje SIM_ITERACE_ODPUZOVANI-krát.
// ---------------------------------------------------------------------
static void odpuzovani(std::vector<double>& x, std::vector<double>& y,
                       std::vector<double>& vx, std::vector<double>& vy,
                       const std::vector<double>& polomer)
{
    int n = x.size();
    for (int iterace = 0; iterace < SIM_ITERACE_ODPUZOVANI; iterace++)
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)          // každá dvojice jednou
            {
                double dx = x[j] - x[i], dy = y[j] - y[i];
                double d = std::hypot(dx, dy);                  // vzdálenost středů
                double dmin = polomer[i] + polomer[j];          // vzdálenost při dotyku
                if (d >= dmin || d == 0) continue;              // nepřekrývají se

                // jednotkový vektor od buňky i k buňce j
                double nx = dx / d, ny = dy / d;

                // posun obou buněk od sebe o polovinu překryvu
                double polovina = (dmin - d) / 2;
                x[i] -= nx * polovina;  y[i] -= ny * polovina;
                x[j] += nx * polovina;  y[j] += ny * polovina;

                // odraz rychlosti, pokud buňka míří ke druhé (skalární součin > 0)
                double vi = vx[i] * nx + vy[i] * ny;
                if (vi > 0) { vx[i] -= 2 * vi * nx;  vy[i] -= 2 * vi * ny; }
                double vj = vx[j] * nx + vy[j] * ny;
                if (vj < 0) { vx[j] -= 2 * vj * nx;  vy[j] -= 2 * vj * ny; }
            }
}

// ---------------------------------------------------------------------
//  1) SIMULACE POHYBU
// ---------------------------------------------------------------------
std::vector<Trajektorie> simulujPohyb(const std::vector<Bunka>& bunky, Model model,
                                      int sirka, int vyska)
{
    // --- generátor náhodných čísel (pevný seed = simulace jde zopakovat) ---
    std::mt19937 generator(SIM_SEED);
    std::normal_distribution<double> N01(0.0, 1.0);     // normální rozdělení N(0,1)

    // --- konstanty modelů (vzorce viz simulace.h) ---
    const double S = SIM_RYCHLOST;
    const double P = SIM_PERZISTENCE;
    const double sigma = S / std::sqrt(2.0);              // rozptyl rychlosti na jednu osu
    const double alfa  = std::exp(-1.0 / P);              // "paměť" rychlosti (0 až 1)
    const double D     = S * S * P / 2.0;                 // difuzní koeficient [px^2/snímek]
    const double krokBrown = std::sqrt(2.0 * D);          // velikost Brownova kroku na osu

    // --- start: poloha = těžiště ze segmentace, náhodná počáteční rychlost ---
    int n = bunky.size();
    std::vector<Trajektorie> traj(n);
    std::vector<double> vx(n), vy(n);
    for (int i = 0; i < n; i++)
    {
        traj[i].id = bunky[i].id;
        traj[i].body.push_back({0, bunky[i].sx, bunky[i].sy});
        vx[i] = sigma * N01(generator);
        vy[i] = sigma * N01(generator);
    }

    // --- poloměr buňky = poloměr kruhu se stejnou plochou: r = sqrt(A / pi) ---
    std::vector<double> polomer(n);
    for (int i = 0; i < n; i++) polomer[i] = std::sqrt(bunky[i].plocha / M_PI);

    // --- časové kroky ---
    std::vector<double> x(n), y(n);          // nové polohy v aktuálním kroku
    for (int t = 1; t < SIM_SNIMKU; t++)
    {
        // (a) každá buňka udělá krok podle modelu
        for (int i = 0; i < n; i++)
        {
            Bod p = traj[i].body.back();      // poslední poloha
            double dx, dy;

            if (model == Model::Brown)
            {
                // Brown: každý krok úplně náhodný, bez paměti
                dx = krokBrown * N01(generator);
                dy = krokBrown * N01(generator);
            }
            else
            {
                // perzistentní: nová rychlost = část staré + náhodná změna
                vx[i] = alfa * vx[i] + sigma * std::sqrt(1 - alfa * alfa) * N01(generator);
                vy[i] = alfa * vy[i] + sigma * std::sqrt(1 - alfa * alfa) * N01(generator);
                dx = vx[i];
                dy = vy[i];
            }
            x[i] = p.x + dx;
            y[i] = p.y + dy;
        }

        // (b) překrývající se buňky se odstrčí od sebe
        if (SIM_ODPUZOVANI) odpuzovani(x, y, vx, vy, polomer);

        // (c) odraz od okraje a uložení polohy
        for (int i = 0; i < n; i++)
        {
            odraz(x[i], vx[i], sirka);
            odraz(y[i], vy[i], vyska);
            traj[i].body.push_back({t, x[i], y[i]});
        }
    }

    return traj;
}

// ---------------------------------------------------------------------
//  2) VYKRESLENÍ SNÍMKŮ SIMULACE
// ---------------------------------------------------------------------
std::vector<Obrazek> vykresliSimulaci(const Obrazek& snimek, const Segmentace& seg,
                                      const std::vector<Trajektorie>& traj)
{
    int W = snimek.sirka, H = snimek.vyska;

    // --- a) "záplaty": pixely každé buňky relativně k jejímu těžišti ---
    //        (ID buněk jsou 1, 2, 3, ... -> buňka s ID k má index k-1)
    struct Zaplata { std::vector<int> dx, dy; std::vector<unsigned char> jas; };
    std::vector<Zaplata> zaplaty(seg.bunky.size());
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            int s = seg.stitky[y * W + x];
            if (s == 0) continue;                        // pozadí
            const Bunka& b = seg.bunky[s - 1];
            zaplaty[s - 1].dx.push_back(x - std::lround(b.sx));
            zaplaty[s - 1].dy.push_back(y - std::lround(b.sy));
            zaplaty[s - 1].jas.push_back(snimek.at(x, y));
        }

    // --- b) průměrný jas pozadí (pixely mimo buňky) ---
    double suma = 0;
    int pocet = 0;
    for (int i = 0; i < W * H; i++)
        if (seg.stitky[i] == 0) { suma += snimek.px[i]; pocet++; }
    double pozadi = suma / pocet;

    // --- c) snímky: pozadí se šumem + buňky na nových polohách ---
    std::mt19937 generator(SIM_SEED + 1);
    std::normal_distribution<double> sum(0.0, SIM_SUM);

    std::vector<Obrazek> snimky;
    for (int t = 0; t < SIM_SNIMKU; t++)
    {
        Obrazek f;
        f.sirka = W; f.vyska = H;
        f.px.resize(W * H);
        for (auto& v : f.px)                             // pozadí + šum
            v = std::clamp(pozadi + sum(generator), 0.0, 255.0);

        for (size_t k = 0; k < traj.size(); k++)         // každou buňku vložit
        {
            int cx = std::lround(traj[k].body[t].x);
            int cy = std::lround(traj[k].body[t].y);
            const Zaplata& z = zaplaty[k];
            for (size_t p = 0; p < z.jas.size(); p++)
            {
                int x = cx + z.dx[p], y = cy + z.dy[p];
                if (x >= 0 && y >= 0 && x < W && y < H) f.at(x, y) = z.jas[p];
            }
        }
        snimky.push_back(f);
    }
    return snimky;
}

// ---------------------------------------------------------------------
//  3) STATISTIKA POHYBU (význam veličin viz simulace.h)
// ---------------------------------------------------------------------
Statistika spocitejStatistiku(const Trajektorie& tr)
{
    Statistika s;
    for (size_t i = 1; i < tr.body.size(); i++)
        s.draha += std::hypot(tr.body[i].x - tr.body[i - 1].x, tr.body[i].y - tr.body[i - 1].y);
    s.posunuti = std::hypot(tr.body.back().x - tr.body.front().x,
                            tr.body.back().y - tr.body.front().y);
    int kroky = tr.body.size() - 1;
    if (kroky > 0)   s.rychlost = s.draha / kroky;
    if (s.draha > 0) s.primost  = s.posunuti / s.draha;
    return s;
}
