// Lecture et export des fichiers .bin d'Acoustimeter (Qt 6, C++17).
//   ACMB     : fichier de la fenetre Mesure (tableau points x colonnes, float64)
//   ACOUSBIN : enregistrement de la Supervision ou des trames MLI (voies echantillonnees)
#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <utility>
#include <vector>

namespace acous {

enum class Type { Inconnu, Mesure, Enregistrement };
Type typeFichier(const QString& chemin);              // d'apres la signature (premiers octets)

// ---------------------------------------------------------------------------------- mesure (ACMB)
struct Mesure {
    QString fichier;
    std::vector<std::pair<QString, QString>> entete;  // cles dans l'ordre du fichier
    QStringList colonnes;
    int nPoints = 0;
    std::vector<double> donnees;                       // point par point : donnees[i * colonnes.size() + j]
    std::vector<std::pair<int, QString>> remarques;    // (numero de point a partir de 1, texte)

    QString cle(const QString& c, const QString& defaut = QStringLiteral("—")) const;
    int indice(const QString& nom) const;              // nom exact, sinon debut du nom ; -1 si absente
    double valeur(int i, int j) const { return donnees[size_t(i) * colonnes.size() + j]; }
    std::vector<double> colonne(int j) const;
    std::vector<double> colonne(const QString& nom) const;   // vide si absente
};
bool lireMesure(const QString& chemin, Mesure& m, QString& erreur);
void resumeMesure(const Mesure& m);
bool exportCsvMesure(const Mesure& m, const QString& chemin);  // ';' et virgule decimale, NaN -> vide

// ---------------------------------------------------------------------------------- enregistrement (ACOUSBIN)
struct Voie {
    QString nom, groupe, unite, dtype;                 // dtype : "f32" ou "u8"
    double fs = 1, t0 = 0;
    std::vector<float> f32;                            // valeurs si dtype f32
    std::vector<uint8_t> u8;                           // valeurs si dtype u8 (trames MLI 0/1)
    size_t taille() const { return dtype == QLatin1String("u8") ? u8.size() : f32.size(); }
    double v(size_t i) const { return dtype == QLatin1String("u8") ? double(u8[i]) : double(f32[i]); }
    double temps(size_t i) const { return t0 + double(i) / fs; }
    double duree() const { return double(taille()) / fs; }
    std::pair<size_t, size_t> fenetre(double debut, double fin) const;   // [i0, i1[ des echantillons dans [debut, fin]
};
struct Evenement { double t = 0; QString type, detail; };
struct Pont { std::vector<float> signal; double fs = 0, t0 = 0; QString description; };

struct Enregistrement {
    QString fichier, titre, debut;
    QJsonObject meta;
    std::vector<Evenement> evenements;
    std::vector<Voie> voies;
    int indice(const QString& nom) const;              // nom exact, debut du nom, puis morceau du nom ; -1 si absente
    std::vector<int> selection(const QStringList& noms) const;   // vide -> toutes
};
bool lireEnregistrement(const QString& chemin, Enregistrement& r, QString& erreur);
void resumeEnregistrement(const Enregistrement& r);
// trames MLI : (haut bras A) - (haut bras B), puis moyenne par paquets jusqu'a fsSortie (filtre passe-bas simple)
bool calculePont(const Enregistrement& r, const QString& hautA, const QString& hautB, double fsSortie, Pont& p, QString& erreur);
bool exportCsvVoie(const Voie& v, double debut, double fin, const QString& chemin);
bool ecritWav(const QString& chemin, const std::vector<double>& x, double fs);   // float 32 bits, normalise

QString nomPropre(const QString& s);                  // pour les noms de fichiers
void ecrire(const QString& texte);                     // console en UTF-8 (Windows compris)

}  // namespace acous
