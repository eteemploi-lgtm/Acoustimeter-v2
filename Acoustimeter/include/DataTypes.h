#pragma once
#include <QString>
#include <QStringList>
#include <QList>

enum class UserProfile { Operator, Administrator };

struct ConnectionConfig {
    QString chassis;
    int     slot       = 1;
    QString interface_;
    UserProfile profile = UserProfile::Operator;
    bool    offline    = false;   // true = ouvert sans materiel (aucune mesure)
};

struct HydrophoneConfig {
    int     id = 1;
    int     channel = 1;            // numero de voie 1..6 sur la PXIe-4480
    double  gain = 20.0;            // gain preampli en dB
    int     sampleRate = 96000;
    double  dynamic = 10.0;         // +/- volts
    double  distance = 1.0;
    double  depth = 0.5;
    QString model;
    QString sensitivityFile;
};

struct AmplifierConfig {
    int     id = 1;
    QString wavFile;
    QString filterFile;
    QString carrierType = "Triangle";
    double  carrierFreq = 38400.0;
    double  amplitude = 1.0;
    double  offset = 0.0;
    QString mliPin;                 // ligne/pin de la carte numerique pour cette voie
    int     tempProbeCount = 2;
    QString tempType = "K";
    double  tempAcqFreq = 1.0;
    QString tempLabel;
    int     voltChannelCount = 2;
    QString voltDivision = "1/10";
    double  voltAcqFreq = 10.0;
    QString voltLabel;
    int     currChannelCount = 0;
    QString currDivision = "1/10";
    double  currAcqFreq = 10.0;
    QString currLabel;
};

struct SvConfig {
    int    listenChannel = 1;
    int    hydrophoneId  = 0;
    double gain          = 20.0;
    int    sampleRate    = 96000;
    double pitchMin = 0,   pitchMax = 360;
    double rollMin  = 0,   rollMax  = 90;
    double angularResolution = 1.0;
    double measureTime       = 100.0;
    double freqMin  = 20000, freqMax = 80000;
    double freqStep = 1000;
    double hydroDepth      = 0.5;
    double transducerDepth = 1.0;
    double distance        = 1.5;
};

struct SystemConfig {
    ConnectionConfig        connection;
    QList<HydrophoneConfig> hydrophones;
    QList<AmplifierConfig>  amplifiers;
    SvConfig                sv;
};
