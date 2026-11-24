#include "fichiers_acoustimeter.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#endif

namespace acous {

void ecrire(const QString& texte)
{
#ifdef _WIN32
    static bool init = false;
    if (!init) { SetConsoleOutputCP(CP_UTF8); init = true; }
#endif
    const QByteArray b = texte.toUtf8();
    std::fwrite(b.constData(), 1, size_t(b.size()), stdout);
    std::fflush(stdout);
}

static uint32_t lu32(const char* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }   // fichiers petit-boutistes (PC)

static QString nombre(double x, int prec = 6) { return QString::number(x, 'g', prec); }

static QString colonneTexte(const QString& s, int largeur)        // alignement en tenant compte des accents
{
    return s.size() >= largeur ? s : s + QString(largeur - s.size(), QLatin1Char(' '));
}
static QString droite(const QString& s, int largeur)
{
    return s.size() >= largeur ? s : QString(largeur - s.size(), QLatin1Char(' ')) + s;
}

QString nomPropre(const QString& s)
{
    QString r = s;
    r.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), QStringLiteral("_"));
    r.remove(QRegularExpression(QStringLiteral("^_|_$")));
    return r;
}

Type typeFichier(const QString& chemin)
{
    QFile f(chemin);
    if (!f.open(QIODevice::ReadOnly)) return Type::Inconnu;
    const QByteArray s = f.read(8);
    if (s.startsWith("ACMB")) return Type::Mesure;
    if (s == "ACOUSBIN") return Type::Enregistrement;
    return Type::Inconnu;
}

// =================================================================================== mesure

QString Mesure::cle(const QString& c, const QString& defaut) const
{
    for (const auto& kv : entete)
        if (kv.first == c) return kv.second;
    return defaut;
}

int Mesure::indice(const QString& nom) const
{
    int j = colonnes.indexOf(nom);
    if (j >= 0) return j;
    for (int k = 0; k < colonnes.size(); ++k)
        if (colonnes[k].startsWith(nom, Qt::CaseInsensitive)) return k;
    return -1;
}

std::vector<double> Mesure::colonne(int j) const
{
    std::vector<double> c(static_cast<size_t>(nPoints));
    for (int i = 0; i < nPoints; ++i) c[size_t(i)] = valeur(i, j);
    return c;
}

std::vector<double> Mesure::colonne(const QString& nom) const
{
    const int j = indice(nom);
    return j < 0 ? std::vector<double>() : colonne(j);
}

bool lireMesure(const QString& chemin, Mesure& m, QString& erreur)
{
    QFile f(chemin);
    if (!f.open(QIODevice::ReadOnly)) { erreur = QStringLiteral("Ouverture impossible : ") + chemin; return false; }
    const QByteArray b = f.readAll();
    if (b.size() < 20 || !b.startsWith("ACMB")) { erreur = chemin + QStringLiteral(" n'est pas un fichier de mesure Acoustimeter (signature ACMB absente)"); return false; }
    const uint32_t version = lu32(b.constData() + 4), ncol = lu32(b.constData() + 8), npts = lu32(b.constData() + 12), lent = lu32(b.constData() + 16);
    if (version != 1) { erreur = QStringLiteral("Version ACMB %1 non prise en charge").arg(version); return false; }
    if (20 + qsizetype(lent) > b.size()) { erreur = QStringLiteral("En-tête incomplet"); return false; }

    m = Mesure();
    m.fichier = chemin;
    const QString texte = QString::fromUtf8(b.constData() + 20, qsizetype(lent));
    static const QRegularExpression reRem(QStringLiteral("^remarque_(\\d+)$"));
    for (const QString& ligne : texte.split(QRegularExpression(QStringLiteral("\\r?\\n")))) {
        const int p = int(ligne.indexOf(QLatin1Char('=')));
        if (p < 0) continue;
        const QString k = ligne.left(p), v = ligne.mid(p + 1);
        m.entete.emplace_back(k, v);
        const auto r = reRem.match(k);
        if (r.hasMatch()) m.remarques.emplace_back(r.captured(1).toInt() + 1, v);
    }
    std::sort(m.remarques.begin(), m.remarques.end(), [](const auto& a, const auto& c) { return a.first < c.first; });
    const QString cols = m.cle(QStringLiteral("colonnes"), QString());
    if (!cols.isEmpty()) m.colonnes = cols.split(QLatin1Char(','));
    if (m.colonnes.size() != int(ncol)) {
        if (!m.colonnes.isEmpty()) ecrire(QStringLiteral("Attention : la clé colonnes ne liste pas %1 noms.\n").arg(ncol));
        m.colonnes.clear();
        for (uint32_t k = 0; k < ncol; ++k) m.colonnes << QStringLiteral("col%1").arg(k);
    }
    const qsizetype debut = 20 + qsizetype(lent), dispo = b.size() - debut;
    qsizetype n = qsizetype(npts);
    if (ncol > 0 && dispo < qsizetype(8) * ncol * npts) {
        n = dispo / (qsizetype(8) * ncol);
        ecrire(QStringLiteral("Attention : fichier tronqué (%1 points lus sur %2).\n").arg(n).arg(npts));
    }
    m.nPoints = int(n);
    m.donnees.resize(size_t(n) * ncol);
    if (!m.donnees.empty()) std::memcpy(m.donnees.data(), b.constData() + debut, m.donnees.size() * sizeof(double));
    return true;
}

void resumeMesure(const Mesure& m)
{
    QString s;
    s += QStringLiteral("\n=== Mesure Acoustimeter : %1\n").arg(m.fichier);
    s += QStringLiteral("  Type        : %1\n").arg(m.cle(QStringLiteral("mesure")));
    s += QStringLiteral("  Date        : %1   %2\n").arg(m.cle(QStringLiteral("date")), m.cle(QStringLiteral("simulation")) == QLatin1String("1") ? QStringLiteral("(SIMULATION)") : QString());
    s += QStringLiteral("  Ampli       : %1   classe %2\n").arg(m.cle(QStringLiteral("ampli")), m.cle(QStringLiteral("classe")));
    s += QStringLiteral("  Hydrophone  : %1   distance %2 m   gain préampli %3 dB\n")
             .arg(m.cle(QStringLiteral("hydrophone")), m.cle(QStringLiteral("distance_m")), m.cle(QStringLiteral("gain_preampli_dB")));
    s += QStringLiteral("  Points      : %1     Colonnes : %2\n").arg(m.nPoints).arg(m.colonnes.size());
    const QString lp = m.cle(QStringLiteral("Lp_max_global_dB_re_1uPa"), QString());
    if (!lp.isEmpty()) s += QStringLiteral("  Lp max global : %1 dB re 1 µPa\n").arg(lp);
    s += QStringLiteral("  ") + colonneTexte(QStringLiteral("colonne"), 34) + droite(QStringLiteral("mesures"), 9) + droite(QStringLiteral("min"), 15) + droite(QStringLiteral("max"), 15) + QLatin1Char('\n');
    for (int j = 0; j < m.colonnes.size(); ++j) {
        int n = 0; double mn = std::numeric_limits<double>::infinity(), mx = -mn;
        for (int i = 0; i < m.nPoints; ++i) {
            const double x = m.valeur(i, j);
            if (std::isnan(x)) continue;
            ++n; mn = std::min(mn, x); mx = std::max(mx, x);
        }
        s += QStringLiteral("  ") + colonneTexte(m.colonnes[j], 34);
        s += n ? droite(QString::number(n), 9) + droite(nombre(mn), 15) + droite(nombre(mx), 15) : droite(QStringLiteral("—"), 9);
        s += QLatin1Char('\n');
    }
    for (size_t k = 0; k < std::min<size_t>(5, m.remarques.size()); ++k)
        s += QStringLiteral("  Remarque point %1 : %2\n").arg(m.remarques[k].first).arg(m.remarques[k].second);
    if (m.remarques.size() > 5) s += QStringLiteral("  ... %1 remarques au total\n").arg(m.remarques.size());
    ecrire(s + QLatin1Char('\n'));
}

bool exportCsvMesure(const Mesure& m, const QString& chemin)
{
    QSaveFile f(chemin);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QByteArray out = "\xEF\xBB\xBF";                   // BOM : Excel reconnait l'UTF-8
    out += m.colonnes.join(QLatin1Char(';')).toUtf8() + '\n';
    for (int i = 0; i < m.nPoints; ++i) {
        QStringList l;
        for (int j = 0; j < m.colonnes.size(); ++j) {
            const double x = m.valeur(i, j);
            l << (std::isnan(x) ? QString() : QString::number(x, 'g', 10).replace(QLatin1Char('.'), QLatin1Char(',')));
        }
        out += l.join(QLatin1Char(';')).toUtf8() + '\n';
    }
    f.write(out);
    return f.commit();
}

// =================================================================================== enregistrement

std::pair<size_t, size_t> Voie::fenetre(double debut, double fin) const
{
    const size_t n = taille();
    size_t i0 = 0, i1 = n;
    if (std::isfinite(debut)) i0 = size_t(std::clamp(std::ceil((debut - t0) * fs), 0.0, double(n)));
    if (std::isfinite(fin)) i1 = size_t(std::clamp(std::floor((fin - t0) * fs) + 1, 0.0, double(n)));
    return {i0, std::max(i0, i1)};
}

int Enregistrement::indice(const QString& nom) const
{
    for (size_t k = 0; k < voies.size(); ++k) if (voies[k].nom == nom) return int(k);
    for (size_t k = 0; k < voies.size(); ++k) if (voies[k].nom.startsWith(nom, Qt::CaseInsensitive)) return int(k);
    for (size_t k = 0; k < voies.size(); ++k) if (voies[k].nom.contains(nom, Qt::CaseInsensitive)) return int(k);
    return -1;
}

std::vector<int> Enregistrement::selection(const QStringList& noms) const
{
    std::vector<int> r;
    auto ajoute = [&r](int k) { if (k >= 0 && std::find(r.begin(), r.end(), k) == r.end()) r.push_back(k); };
    if (noms.isEmpty()) { for (size_t k = 0; k < voies.size(); ++k) ajoute(int(k)); return r; }
    for (const QString& n : noms) {
        bool trouve = false;
        for (size_t k = 0; k < voies.size(); ++k)
            if (voies[k].nom.startsWith(n, Qt::CaseInsensitive)) { ajoute(int(k)); trouve = true; }
        if (!trouve) ajoute(indice(n));
    }
    return r;
}

bool lireEnregistrement(const QString& chemin, Enregistrement& r, QString& erreur)
{
    QFile f(chemin);
    if (!f.open(QIODevice::ReadOnly)) { erreur = QStringLiteral("Ouverture impossible : ") + chemin; return false; }
    const QByteArray b = f.readAll();
    if (b.size() < 16 || !b.startsWith("ACOUSBIN")) { erreur = chemin + QStringLiteral(" n'est pas un enregistrement Acoustimeter (signature ACOUSBIN absente)"); return false; }
    const uint32_t version = lu32(b.constData() + 8), h = lu32(b.constData() + 12);
    if (version != 1) { erreur = QStringLiteral("Version ACOUSBIN %1 non prise en charge").arg(version); return false; }
    if (qsizetype(h) > b.size() || h < 16) { erreur = QStringLiteral("En-tête incomplet"); return false; }
    QByteArray js = b.mid(16, qsizetype(h) - 16).trimmed();
    while (!js.isEmpty() && js.back() == '\0') js.chop(1);
    QJsonParseError pe;
    const QJsonObject e = QJsonDocument::fromJson(js, &pe).object();
    if (pe.error != QJsonParseError::NoError) { erreur = QStringLiteral("En-tête JSON illisible : ") + pe.errorString(); return false; }

    r = Enregistrement();
    r.fichier = chemin;
    r.titre = e.value(QStringLiteral("titre")).toString();
    r.debut = e.value(QStringLiteral("debut")).toString();
    r.meta = e.value(QStringLiteral("meta")).toObject();
    for (const auto& x : e.value(QStringLiteral("evenements")).toArray()) {
        const QJsonObject o = x.toObject();
        r.evenements.push_back({o.value(QStringLiteral("t")).toDouble(), o.value(QStringLiteral("type")).toString(), o.value(QStringLiteral("detail")).toString()});
    }
    for (const auto& x : e.value(QStringLiteral("voies")).toArray()) {
        const QJsonObject o = x.toObject();
        Voie v;
        v.nom = o.value(QStringLiteral("nom")).toString();
        v.groupe = o.value(QStringLiteral("groupe")).toString();
        v.unite = o.value(QStringLiteral("unite")).toString();
        v.dtype = o.value(QStringLiteral("dtype")).toString(QStringLiteral("f32"));
        v.fs = o.value(QStringLiteral("fs")).toDouble(1);
        v.t0 = o.value(QStringLiteral("t0")).toDouble(0);
        r.voies.push_back(v);
    }
    if (r.voies.empty())
        ecrire(QStringLiteral("Attention : en-tête sans voie — l'enregistrement n'a pas été fermé normalement (arrêt brutal d'Acoustimeter). "
                              "Les données sont dans le fichier mais ne peuvent pas être attribuées à une voie.\n"));

    // 1er passage : taille de chaque voie (evite les reallocations sur les gros fichiers)
    std::vector<size_t> total(r.voies.size(), 0);
    qsizetype p = h;
    while (p + 8 <= b.size()) {
        const uint32_t i = lu32(b.constData() + p), n = lu32(b.constData() + p + 4);
        if (i >= r.voies.size()) break;
        const qsizetype octets = qsizetype(n) * (r.voies[i].dtype == QLatin1String("u8") ? 1 : 4);
        total[i] += n; p += 8 + octets;
    }
    for (size_t k = 0; k < r.voies.size(); ++k) {
        if (r.voies[k].dtype == QLatin1String("u8")) r.voies[k].u8.reserve(total[k]);
        else r.voies[k].f32.reserve(total[k]);
    }
    // 2e passage : copie des blocs
    p = h;
    while (p + 8 <= b.size()) {
        const uint32_t i = lu32(b.constData() + p), n = lu32(b.constData() + p + 4);
        if (i >= r.voies.size()) {
            ecrire(QStringLiteral("Attention : bloc d'une voie inconnue (%1) à la position %2, lecture arrêtée.\n").arg(i).arg(p));
            break;
        }
        p += 8;
        Voie& v = r.voies[i];
        const bool u8 = v.dtype == QLatin1String("u8");
        const qsizetype taille = u8 ? 1 : 4;
        qsizetype nb = qsizetype(n);
        if (p + nb * taille > b.size()) { nb = (b.size() - p) / taille; ecrire(QStringLiteral("Attention : fichier tronqué (dernier bloc incomplet).\n")); }
        const char* d = b.constData() + p;
        if (u8) v.u8.insert(v.u8.end(), reinterpret_cast<const uint8_t*>(d), reinterpret_cast<const uint8_t*>(d) + nb);
        else { const size_t old = v.f32.size(); v.f32.resize(old + size_t(nb)); std::memcpy(v.f32.data() + old, d, size_t(nb) * 4); }
        p += nb * taille;
    }
    return true;
}

void resumeEnregistrement(const Enregistrement& r)
{
    QString s;
    s += QStringLiteral("\n=== Enregistrement Acoustimeter : %1\n").arg(r.fichier);
    s += QStringLiteral("  Titre : %1     Début : %2     Voies : %3     Événements : %4\n").arg(r.titre, r.debut).arg(r.voies.size()).arg(r.evenements.size());
    s += QStringLiteral("  ") + colonneTexte(QStringLiteral("voie"), 33) + colonneTexte(QStringLiteral("groupe"), 16) + colonneTexte(QStringLiteral("unité"), 7)
         + droite(QStringLiteral("fs (Hz)"), 12) + droite(QStringLiteral("t0 (s)"), 11) + droite(QStringLiteral("échantillons"), 13) + droite(QStringLiteral("durée (s)"), 11) + QLatin1Char('\n');
    for (const Voie& v : r.voies)
        s += QStringLiteral("  ") + colonneTexte(v.nom, 33) + colonneTexte(v.groupe, 16) + colonneTexte(v.unite, 7) + droite(nombre(v.fs), 12)
             + droite(nombre(v.t0, 4), 11) + droite(QString::number(v.taille()), 13) + droite(nombre(v.duree(), 4), 11) + QLatin1Char('\n');
    for (const Evenement& e : r.evenements)
        s += QStringLiteral("  t = %1 s  %2 %3\n").arg(e.t, 8, 'f', 3).arg(colonneTexte(e.type, 16), e.detail);
    if (!r.meta.isEmpty()) {
        QStringList l;
        for (auto it = r.meta.begin(); it != r.meta.end(); ++it)
            l << it.key() + QLatin1Char('=') + (it.value().isArray() || it.value().isObject() ? QStringLiteral("[...]") : it.value().toVariant().toString());
        s += QStringLiteral("  meta : ") + l.join(QStringLiteral("; ")) + QLatin1Char('\n');
    }
    ecrire(s + QLatin1Char('\n'));
}

bool calculePont(const Enregistrement& r, const QString& hautA, const QString& hautB, double fsSortie, Pont& p, QString& erreur)
{
    const int ia = r.indice(hautA), ib = r.indice(hautB);
    if (ia < 0 || ib < 0) { erreur = QStringLiteral("Voie « %1 » absente").arg(ia < 0 ? hautA : hautB); return false; }
    const Voie &a = r.voies[size_t(ia)], &b = r.voies[size_t(ib)];
    const size_t n = std::min(a.taille(), b.taille());
    const size_t k = size_t(std::max(1.0, std::round(a.fs / fsSortie))), m = n / k;
    p = Pont();
    p.signal.resize(m);
    for (size_t j = 0; j < m; ++j) {
        double s = 0;
        for (size_t i = j * k; i < (j + 1) * k; ++i) s += a.v(i) - b.v(i);   // -1 / 0 / +1
        p.signal[j] = float(s / double(k));
    }
    p.fs = a.fs / double(k);
    p.t0 = a.t0;
    p.description = QStringLiteral("%1 − %2, moyenne sur %3 échantillons").arg(a.nom, b.nom).arg(k);
    return true;
}

bool exportCsvVoie(const Voie& v, double debut, double fin, const QString& chemin)
{
    QSaveFile f(chemin);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const auto [i0, i1] = v.fenetre(debut, fin);
    QByteArray out = "\xEF\xBB\xBF";
    out += QStringLiteral("temps_s;%1_%2\n").arg(nomPropre(v.nom), nomPropre(v.unite)).toUtf8();
    out.reserve(out.size() + qsizetype(i1 - i0) * 24);
    char ligne[64];
    for (size_t i = i0; i < i1; ++i) {
        const int n = std::snprintf(ligne, sizeof ligne, "%.9g;%.7g\n", v.temps(i), v.v(i));
        for (int c = 0; c < n; ++c) if (ligne[c] == '.') ligne[c] = ',';
        out.append(ligne, n);
    }
    f.write(out);
    return f.commit();
}

bool ecritWav(const QString& chemin, const std::vector<double>& x, double fs)
{
    if (x.empty()) return false;
    double moy = 0;
    for (double v : x) moy += v;
    moy /= double(x.size());
    double mx = 1e-30;
    for (double v : x) mx = std::max(mx, std::abs(v - moy));
    std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = float((x[i] - moy) / mx * 0.99);
    const uint32_t f = uint32_t(std::lround(fs)), n = uint32_t(y.size() * 4);
    QSaveFile o(chemin);
    if (!o.open(QIODevice::WriteOnly)) return false;
    auto u32 = [&o](uint32_t v) { o.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&o](uint16_t v) { o.write(reinterpret_cast<const char*>(&v), 2); };
    o.write("RIFF"); u32(36 + n); o.write("WAVE");
    o.write("fmt "); u32(16); u16(3); u16(1); u32(f); u32(f * 4); u16(4); u16(32);   // 3 = IEEE float
    o.write("data"); u32(n); o.write(reinterpret_cast<const char*>(y.data()), n);
    return o.commit();
}

}  // namespace acous
