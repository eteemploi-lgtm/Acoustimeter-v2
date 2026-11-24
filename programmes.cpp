// Les deux programmes : acoustimeter_mesure (ACMB) et acoustimeter_enregistrement (ACOUSBIN).
// Un fichier donne au mauvais programme est passe a l'autre.
#include "programmes.h"

#include "fenetre_graphes.h"
#include "fichiers_acoustimeter.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace acous {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

struct Options {
    QString fichier, x, png;
    QStringList y, voies, exports, pont;
    double debut = -kInf, fin = kInf, fsPont = 2e5;
    QString ecouter;
    bool pasDeTrace = false, aide = false;
};

const char* kAideMesure =
    "acoustimeter_mesure : ouvre, affiche et exploite un .bin de la fenetre Mesure d'Acoustimeter (format ACMB).\n\n"
    "  acoustimeter_mesure                         choisir le fichier dans une fenetre\n"
    "  acoustimeter_mesure mesure.bin              resume + courbes\n"
    "  acoustimeter_mesure mesure.bin --y G_S B_S --export csv\n\n"
    "Options\n"
    "  --x COLONNE        colonne horizontale (defaut automatique : f_Hz, angle_deg, Urms_V, Irms_A)\n"
    "  --y COL1 COL2 ...  colonnes a tracer (le debut du nom suffit)\n"
    "  --export csv       ecrit mesure.csv a cote du .bin (';' et virgule decimale, pour Excel en francais)\n"
    "  --png FICHIER      enregistre la figure\n"
    "  --pas-de-trace     pas de fenetre (resume et export seulement)\n";

const char* kAideEnr =
    "acoustimeter_enregistrement : ouvre, affiche et exploite un enregistrement .bin d'Acoustimeter (format ACOUSBIN) :\n"
    "Supervision (SAVE-MANU / SAVE-TRIG) ou trames MLI enregistrees depuis Configuration > Ampli > MLI.\n\n"
    "  acoustimeter_enregistrement                       choisir le fichier dans une fenetre\n"
    "  acoustimeter_enregistrement enr.bin               resume + courbes\n"
    "  acoustimeter_enregistrement enr.bin --voies Hydro --debut 10 --fin 10.05 --export csv wav\n"
    "  acoustimeter_enregistrement trames_MLI.bin --pont MLI1 MLI3 --export wav\n\n"
    "Options\n"
    "  --voies V1 V2 ...      voies a tracer / exporter (debut du nom suffit ; defaut : toutes)\n"
    "  --debut S  --fin S     fenetre de temps en secondes\n"
    "  --pont HAUT_A HAUT_B   trames MLI : reconstruit le pont complet A - B\n"
    "  --fs-pont HZ           cadence du pont apres filtrage (defaut 200000)\n"
    "  --export csv wav       ecrit a cote du .bin (un fichier par voie ; WAV float 32 bits normalise)\n"
    "  --png FICHIER          enregistre la figure\n"
    "  --pas-de-trace         pas de fenetre\n";

Options analyse(const QStringList& a)
{
    Options o;
    QStringList* liste = nullptr;
    for (int i = 1; i < a.size(); ++i) {
        const QString t = a[i];
        auto suivant = [&]() { return i + 1 < a.size() ? a[++i] : QString(); };
        if (t == QLatin1String("--y")) liste = &o.y;
        else if (t == QLatin1String("--voies")) liste = &o.voies;
        else if (t == QLatin1String("--export")) liste = &o.exports;
        else if (t == QLatin1String("--pont")) liste = &o.pont;
        else if (t == QLatin1String("--x")) { o.x = suivant(); liste = nullptr; }
        else if (t == QLatin1String("--png")) { o.png = suivant(); liste = nullptr; }
        else if (t == QLatin1String("--debut")) { o.debut = suivant().toDouble(); liste = nullptr; }
        else if (t == QLatin1String("--fin")) { o.fin = suivant().toDouble(); liste = nullptr; }
        else if (t == QLatin1String("--fs-pont")) { o.fsPont = suivant().toDouble(); liste = nullptr; }
        else if (t == QLatin1String("--ecouter")) { o.ecouter = suivant(); liste = nullptr; }
        else if (t == QLatin1String("--pas-de-trace")) { o.pasDeTrace = true; liste = nullptr; }
        else if (t == QLatin1String("--help") || t == QLatin1String("-h") || t == QLatin1String("/?")) o.aide = true;
        else if (t.startsWith(QLatin1String("--"))) ecrire(QStringLiteral("Option inconnue ignorée : %1\n").arg(t));
        else if (liste) *liste << t;
        else o.fichier = t;
    }
    for (QString& e : o.exports) e = e.toLower();
    return o;
}

QString libelleX(const QString& n)
{
    if (n == QLatin1String("f_Hz")) return QStringLiteral("Fréquence (Hz)");
    if (n == QLatin1String("angle_deg")) return QStringLiteral("Angle (°)");
    if (n == QLatin1String("Urms_V")) return QStringLiteral("U eff (V)");
    if (n == QLatin1String("Irms_A")) return QStringLiteral("I eff (A)");
    if (n == QLatin1String("temps_s")) return QStringLiteral("Temps (s)");
    if (n == QLatin1String("niveau_5413_V_crete")) return QStringLiteral("Niveau 5413 (V crête / %)");
    return n;
}

bool varie(const std::vector<double>& c)
{
    double a = kInf, b = -kInf;
    for (double v : c) if (std::isfinite(v)) { a = std::min(a, v); b = std::max(b, v); }
    return b > a;
}

int executeMesure(Options& o);
int executeEnregistrement(Options& o);

// ======================================================================================= mesure
int executeMesure(Options& o)
{
    Mesure m;
    QString err;
    if (!lireMesure(o.fichier, m, err)) { ecrire(err + QLatin1Char('\n')); return 1; }
    resumeMesure(m);
    const QFileInfo fi(o.fichier);
    const QString base = fi.path() + QLatin1Char('/') + fi.completeBaseName();
    for (const QString& e : o.exports) {
        if (e == QLatin1String("csv")) ecrire(exportCsvMesure(m, base + QStringLiteral(".csv")) ? QStringLiteral("CSV écrit : %1.csv\n").arg(base) : QStringLiteral("Échec de l'écriture du CSV\n"));
        else ecrire(QStringLiteral("Export « %1 » non disponible pour une mesure (csv seulement).\n").arg(e));
    }
    if ((o.pasDeTrace && o.png.isEmpty()) || m.nPoints == 0) return 0;

    // ---- axe horizontal
    QString xn;
    if (!o.x.isEmpty()) {
        const int j = m.indice(o.x);
        if (j < 0) { ecrire(QStringLiteral("Colonne « %1 » absente.\n").arg(o.x)); return 1; }
        xn = m.colonnes[j];
    } else {
        const QString t = m.cle(QStringLiteral("mesure")).toLower();
        xn = t.contains(QLatin1String("directiv")) ? QStringLiteral("angle_deg") : t.contains(QLatin1String("u=f(i)")) ? QStringLiteral("Irms_A")
           : t.contains(QLatin1String("lin")) ? QStringLiteral("Urms_V") : QStringLiteral("f_Hz");
        if (!varie(m.colonne(xn)))
            for (const char* c : {"f_Hz", "angle_deg", "Urms_V", "niveau_5413_V_crete", "temps_s"})
                if (varie(m.colonne(QString::fromLatin1(c)))) { xn = QString::fromLatin1(c); break; }
    }
    std::vector<double> xv = m.colonne(xn);
    std::vector<size_t> ordre(xv.size());
    std::iota(ordre.begin(), ordre.end(), 0);
    std::stable_sort(ordre.begin(), ordre.end(), [&](size_t a, size_t b) { return xv[a] < xv[b]; });
    std::vector<double> xs;
    for (size_t i : ordre) xs.push_back(xv[i]);
    double xmin = kInf, xmax = -kInf;
    for (double v : xs) if (std::isfinite(v)) { xmin = std::min(xmin, v); xmax = std::max(xmax, v); }
    const bool logX = xn == QLatin1String("f_Hz") && xmin > 0 && xmax / xmin > 20;

    auto serie = [&](const QString& nom, double facteur) {
        Serie s;
        s.nom = nom;
        s.style = Serie::Style::LignePoints;
        s.x = xs;
        const std::vector<double> c = m.colonne(m.indice(nom));
        for (size_t i : ordre) s.y.push_back(float(c[i] * facteur));
        return s;
    };
    auto utile = [&](const QString& n) {
        if (n == xn) return false;
        const int j = m.indice(n);
        if (j < 0 || m.colonnes[j] != n) return false;
        for (double v : m.colonne(j)) if (!std::isnan(v)) return true;
        return false;
    };

    const QString titre = fi.completeBaseName() + QStringLiteral(" — ") + m.cle(QStringLiteral("mesure"), QString());
    struct Famille { QString titre; QStringList cols; double facteur; };
    std::vector<Famille> P;
    if (!o.y.isEmpty()) {
        QStringList c;
        for (const QString& n : o.y) {
            const int j = m.indice(n);
            if (j < 0) { ecrire(QStringLiteral("Colonne « %1 » absente. Colonnes : %2\n").arg(n, m.colonnes.join(QStringLiteral(", ")))); return 1; }
            c << m.colonnes[j];
        }
        P.push_back({c.join(QStringLiteral(", ")), c, 1});
    } else {
        QStringList temp;
        for (const QString& n : m.colonnes) if (QRegularExpression(QStringLiteral("^T_.*_C$")).match(n).hasMatch()) temp << n;
        const std::vector<Famille> tous = {
            {QStringLiteral("TVR / TCR (dB)"), {QStringLiteral("TVR_dB_re_1uPa_m_per_V"), QStringLiteral("TCR_dB_re_1uPa_m_per_A")}, 1},
            {QStringLiteral("SL, SLmax, Lpmax (dB)"), {QStringLiteral("SL_dB_re_1uPa_m"), QStringLiteral("SLmax_dB_re_1uPa_m"), QStringLiteral("Lpmax_Umax_dB_re_1uPa")}, 1},
            {QStringLiteral("TPR (dB)"), {QStringLiteral("TPR_dB_re_1uPa_m_per_sqrtW")}, 1},
            {QStringLiteral("Admittance (mS)"), {QStringLiteral("G_S"), QStringLiteral("B_S"), QStringLiteral("Y_S")}, 1e3},
            {QStringLiteral("Impédance (Ω)"), {QStringLiteral("Z_ohm")}, 1},
            {QStringLiteral("U eff (V)"), {QStringLiteral("Urms_V")}, 1},
            {QStringLiteral("I eff (A)"), {QStringLiteral("Irms_A")}, 1},
            {QStringLiteral("P (W), Q (var), S (VA)"), {QStringLiteral("P_W"), QStringLiteral("Q_var"), QStringLiteral("S_VA")}, 1},
            {QStringLiteral("Distorsion THD (%)"), {QStringLiteral("THDF_U_pct"), QStringLiteral("THDF_I_pct"), QStringLiteral("THDF_hydro_pct")}, 1},
            {QStringLiteral("Directivité (dB)"), {QStringLiteral("directivite_dB")}, 1},
            {QStringLiteral("Températures (°C)"), temp, 1}};
        for (const Famille& f : tous) {
            Famille g{f.titre, {}, f.facteur};
            for (const QString& n : f.cols) if (utile(n)) g.cols << n;
            if (!g.cols.isEmpty()) P.push_back(g);
        }
    }
    if (P.empty()) { ecrire(QStringLiteral("Aucune colonne standard à tracer : utilisez --y.\n")); return 0; }

    const int nc = int(std::min<size_t>(3, P.size())), nl = int((P.size() + size_t(nc) - 1) / size_t(nc));
    auto* w = new FenetreGraphes(titre, nc, true);
    w->setAttribute(Qt::WA_DeleteOnClose);
    for (const Famille& f : P) {
        Panneau& p = w->ajoute(f.titre, libelleX(xn));
        p.logX = logX;
        for (const QString& n : f.cols) p.series.push_back(serie(n, f.facteur));
    }
    const QSize taille(1400, std::min(1000, 320 * nl + 50));
    FenetreGraphes* polaire = nullptr;
    if (xn == QLatin1String("angle_deg") && o.y.isEmpty() && utile(QStringLiteral("directivite_dB"))) {
        polaire = new FenetreGraphes(fi.completeBaseName() + QStringLiteral(" — directivité"), 1, false);
        polaire->setAttribute(Qt::WA_DeleteOnClose);
        Panneau& p = polaire->ajoute(QStringLiteral("Directivité (bord = 0 dB, centre = -40 dB)"), QString());
        p.polaire = true;
        p.series.push_back(serie(QStringLiteral("directivite_dB"), 1));
    }
    if (!o.png.isEmpty()) {
        ecrire(w->enregistrePng(o.png, taille) ? QStringLiteral("Figure écrite : %1\n").arg(o.png) : QStringLiteral("Échec de l'écriture de %1\n").arg(o.png));
        if (polaire) {
            const QString pp = QFileInfo(o.png).path() + QLatin1Char('/') + QFileInfo(o.png).completeBaseName() + QStringLiteral("_polaire.png");
            if (polaire->enregistrePng(pp, QSize(700, 720))) ecrire(QStringLiteral("Figure écrite : %1\n").arg(pp));
        }
    }
    if (o.pasDeTrace) { delete w; delete polaire; return 0; }
    w->resize(taille);
    w->show();
    if (polaire) { polaire->resize(700, 720); polaire->show(); }
    return qApp->exec();
}

// ======================================================================================= enregistrement
int executeEnregistrement(Options& o)
{
    Enregistrement r;
    QString err;
    if (!lireEnregistrement(o.fichier, r, err)) { ecrire(err + QLatin1Char('\n')); return 1; }
    Pont pont;
    const bool avecPont = o.pont.size() == 2;
    if (!o.pont.isEmpty() && !avecPont) { ecrire(QStringLiteral("--pont attend deux voies : haut bras A, haut bras B.\n")); return 1; }
    if (avecPont && !calculePont(r, o.pont[0], o.pont[1], o.fsPont, pont, err)) { ecrire(err + QLatin1Char('\n')); return 1; }
    resumeEnregistrement(r);
    if (avecPont) ecrire(QStringLiteral("  Pont complet : %1 -> %2 Hz, %3 points\n\n").arg(pont.description).arg(pont.fs).arg(pont.signal.size()));

    for (const QString& n : o.voies)
        if (r.indice(n) < 0) { QStringList l; for (const Voie& v : r.voies) l << v.nom; ecrire(QStringLiteral("Voie « %1 » absente. Voies : %2\n").arg(n, l.join(QStringLiteral(", ")))); return 1; }
    const std::vector<int> sel = r.selection(o.voies);
    const QFileInfo fi(o.fichier);
    const QString base = fi.path() + QLatin1Char('/') + fi.completeBaseName();
    for (const QString& e : o.exports) {
        if (e == QLatin1String("csv")) {
            for (int k : sel) {
                const QString f = base + QLatin1Char('_') + nomPropre(r.voies[size_t(k)].nom) + QStringLiteral(".csv");
                ecrire(exportCsvVoie(r.voies[size_t(k)], o.debut, o.fin, f) ? QStringLiteral("CSV écrit : %1\n").arg(f) : QStringLiteral("Échec : %1\n").arg(f));
            }
        } else if (e == QLatin1String("wav")) {
            for (int k : sel) {
                const Voie& v = r.voies[size_t(k)];
                if (v.dtype == QLatin1String("u8")) { ecrire(QStringLiteral("WAV ignoré pour %1 : trame 0/1 (utilisez --pont pour un signal écoutable)\n").arg(v.nom)); continue; }
                const auto [i0, i1] = v.fenetre(o.debut, o.fin);
                std::vector<double> x(v.f32.begin() + qsizetype(i0), v.f32.begin() + qsizetype(i1));
                const QString f = base + QLatin1Char('_') + nomPropre(v.nom) + QStringLiteral(".wav");
                if (ecritWav(f, x, v.fs)) ecrire(QStringLiteral("WAV écrit : %1  (%2 Hz, normalisé)\n").arg(f).arg(std::lround(v.fs)));
            }
            if (avecPont) {
                const std::vector<double> x(pont.signal.begin(), pont.signal.end());
                if (ecritWav(base + QStringLiteral("_pont.wav"), x, pont.fs)) ecrire(QStringLiteral("WAV écrit : %1_pont.wav  (%2 Hz, normalisé)\n").arg(base).arg(std::lround(pont.fs)));
            }
        } else {
            ecrire(QStringLiteral("Export « %1 » non disponible (csv ou wav).\n").arg(e));
        }
    }
    if (!o.ecouter.isEmpty()) ecrire(QStringLiteral("Écoute non disponible dans la version C++ : utilisez --export wav puis ouvrez le WAV.\n"));
    if ((o.pasDeTrace && o.png.isEmpty()) || (sel.empty() && !avecPont)) return 0;

    // ---- une ligne de courbes par groupe
    QStringList groupes;
    for (int k : sel) if (!groupes.contains(r.voies[size_t(k)].groupe)) groupes << r.voies[size_t(k)].groupe;
    auto* w = new FenetreGraphes(fi.completeBaseName() + QStringLiteral(" — ") + r.titre, 1, true);
    w->setAttribute(Qt::WA_DeleteOnClose);
    const QString axe = QStringLiteral("Temps (s) depuis le début de l'enregistrement — ") + r.debut;
    bool seulementMli = true;
    double finMli = -kInf, debutMli = kInf;
    for (const QString& g : groupes) {
        std::vector<int> W;
        QStringList unites;
        for (int k : sel) if (r.voies[size_t(k)].groupe == g) { W.push_back(k); if (!unites.contains(r.voies[size_t(k)].unite)) unites << r.voies[size_t(k)].unite; }
        const bool mli = g == QLatin1String("MLI");
        Panneau& p = w->ajoute(mli ? QStringLiteral("MLI (trames 0/1)") : QStringLiteral("%1 (%2)").arg(g, unites.join(QStringLiteral(", "))), axe);
        if (!mli) seulementMli = false;
        for (size_t j = 0; j < W.size(); ++j) {
            Voie& v = r.voies[size_t(W[j])];
            Serie s;
            s.nom = v.nom;
            s.x0 = v.t0;
            s.dx = 1.0 / v.fs;
            if (mli) {
                s.style = Serie::Style::Escalier;
                s.y.assign(v.u8.begin(), v.u8.end());
                s.echelle = 0.8;
                s.decalage = 1.2 * double(j);
                p.etiquettesY.emplace_back(1.2 * double(j) + 0.4, v.nom);
                debutMli = std::min(debutMli, v.t0);
                finMli = std::max(finMli, v.t0 + 5000.0 / v.fs);
                std::vector<uint8_t>().swap(v.u8);                       // libere la memoire
            } else {
                s.style = v.fs <= 10 ? Serie::Style::LignePoints : Serie::Style::Ligne;
                s.y = std::move(v.f32);
            }
            p.series.push_back(std::move(s));
        }
        if (mli) { p.yFixe = true; p.yMin = -0.3; p.yMax = 1.2 * double(W.size()); }
        for (const Evenement& e : r.evenements) p.marques.push_back({e.t, e.type});
    }
    if (avecPont) {
        Panneau& p = w->ajoute(QStringLiteral("Pont complet : ") + pont.description, axe);
        Serie s;
        s.nom = QStringLiteral("pont");
        s.x0 = pont.t0 + 0.5 / pont.fs;
        s.dx = 1.0 / pont.fs;
        s.y = std::move(pont.signal);
        p.series.push_back(std::move(s));
        p.yFixe = true; p.yMin = -1.1; p.yMax = 1.1;
    }
    const int n = int(w->panneaux().size());
    const QSize taille(1300, std::min(1000, 230 * n + 60));
    w->resize(taille);
    if (std::isfinite(o.debut) || std::isfinite(o.fin)) w->limiteX(o.debut, o.fin);
    else if (seulementMli && std::isfinite(finMli)) {
        w->limiteX(debutMli, finMli);
        ecrire(QStringLiteral("Vue initiale : 5000 premiers échantillons des trames (molette pour dézoomer, double-clic = tout).\n"));
    }
    if (!o.png.isEmpty())
        ecrire(w->enregistrePng(o.png, taille) ? QStringLiteral("Figure écrite : %1\n").arg(o.png) : QStringLiteral("Échec de l'écriture de %1\n").arg(o.png));
    if (o.pasDeTrace) { delete w; return 0; }
    w->show();
    return qApp->exec();
}

}  // namespace

int lancer(int argc, char** argv, Programme prog)
{
    bool sansFenetre = false;
    for (int i = 1; i < argc; ++i) {
        const QByteArray a(argv[i]);
        if (a == "--pas-de-trace") sansFenetre = true;
        if (a == "--help" || a == "-h" || a == "/?") { ecrire(QString::fromUtf8(prog == Programme::Mesure ? kAideMesure : kAideEnr)); return 0; }
    }
    if (sansFenetre && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    Options o = analyse(app.arguments());
    if (o.aide) { ecrire(QString::fromUtf8(prog == Programme::Mesure ? kAideMesure : kAideEnr)); return 0; }
    if (o.fichier.isEmpty()) {
        if (sansFenetre) { ecrire(QStringLiteral("Donnez un fichier .bin (--help pour l'aide).\n")); return 1; }
        o.fichier = QFileDialog::getOpenFileName(nullptr, prog == Programme::Mesure ? QStringLiteral("Choisir une mesure Acoustimeter") : QStringLiteral("Choisir un enregistrement Acoustimeter"),
                                                 QString(), QStringLiteral("Fichiers .bin Acoustimeter (*.bin);;Tous les fichiers (*)"));
        if (o.fichier.isEmpty()) return 0;
    }
    switch (typeFichier(o.fichier)) {
    case Type::Mesure:
        if (prog != Programme::Mesure) ecrire(QStringLiteral("%1 est une mesure (ACMB) : ouverture comme mesure.\n").arg(o.fichier));
        return executeMesure(o);
    case Type::Enregistrement:
        if (prog != Programme::Enregistrement) ecrire(QStringLiteral("%1 est un enregistrement (ACOUSBIN) : ouverture comme enregistrement.\n").arg(o.fichier));
        return executeEnregistrement(o);
    default:
        ecrire(QStringLiteral("%1 n'est pas un fichier .bin d'Acoustimeter (ni ACMB ni ACOUSBIN), ou est illisible.\n").arg(o.fichier));
        return 1;
    }
}

}  // namespace acous
