#pragma once
// Calcul de la sequence MLI (modulation de largeur d'impulsion) :
// bit = (signal > porteuse). Calcul pur, sans dependance NI, execute dans le thread materiel.
#include <QString>
#include <QVector>
#include <QtGlobal>
#include "DataTypes.h"

struct MliSequence {
    int     ampId = 0;
    double  clockHz = 0.0;          // frequence d'horloge de la carte numerique
    QVector<quint8> bits;           // un bit par element (0/1)
    QVector<quint32> words;         // memes bits, empaquetes 32 par mot (LSB en premier)
};

class MliGenerator
{
public:
    // maxDuration : duree maximale generee (s). Retourne false + message si impossible.
    static bool generate(const AmplifierConfig& cfg, double maxDuration,
                         MliSequence* out, QString* error);
    static bool readWav(const QString& path, QVector<double>* samples, double* fs, QString* error);
    static bool readFilter(const QString& path, QVector<double>* taps, QString* error);
    static constexpr int STEPS_PER_CARRIER = 64;   // resolution de la porteuse
};
