#pragma once
// Sortie numerique de la MLI. Une seule classe a adapter selon la carte :
//   - PXIe-6570  -> API niDigital   (ACOUSTIMETER_MLI_NIDIGITAL)
//   - PXIe-6537  -> NI-HSDIO / DAQmx DO (a ecrire dans HsdioMliOutput)
// Voir MliOutput.cpp : section "=== A MODIFIER SI CARTE 6537 ===".
#include <QString>
#include "MliGenerator.h"

class MliOutput
{
public:
    virtual ~MliOutput() = default;
    virtual QString name() const = 0;
    virtual bool open(const QString& deviceName, QString* err) = 0;
    virtual bool load(const MliSequence& seq, const QString& pin, QString* err) = 0;
    virtual bool start(QString* err) = 0;
    virtual void stop() = 0;          // arret + sorties a l'etat sur
    virtual void close() = 0;
    static MliOutput* create();       // instancie le backend compile
};
