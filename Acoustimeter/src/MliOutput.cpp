#include "MliOutput.h"
#include <QSettings>

// ------------------------------------------------------------------ backend nul
class NullMliOutput : public MliOutput {
public:
    QString name() const override { return "aucun"; }
    bool open(const QString&, QString* e) override { *e = "Backend MLI non compile (voir CMake : ACOUSTIMETER_MLI_BACKEND)"; return false; }
    bool load(const MliSequence&, const QString&, QString* e) override { *e = "Backend MLI non compile"; return false; }
    bool start(QString* e) override { *e = "Backend MLI non compile"; return false; }
    void stop() override {}
    void close() override {}
};

#if defined(ACOUSTIMETER_MLI_NIDIGITAL)
// =============================================================================
// === A MODIFIER SI CARTE 6537 : remplacer cette classe par un backend NI-HSDIO
// === (niHSDIO_*) ou DAQmx DO cadence, en gardant la meme interface MliOutput.
// =============================================================================
// Backend PXIe-6570 / niDigital.  ATTENTION : non verifie contre le materiel.
// Prerequis : un projet Digital Pattern Editor (pin map, niveaux, timing, pattern
// contenant une instruction "source" lisant la source waveform "mliWave").
// Chemins lus dans QSettings (modifiables dans Outils > Mapping materiel) :
//   hw/pinmapFile, hw/levelsFile, hw/timingFile, hw/patternFile
#include <niDigital.h>
class NiDigitalMliOutput : public MliOutput {
    ViSession m_vi = VI_NULL;
    QString   m_startLabel = "start";
    bool chk(ViStatus s, QString* e, const char* w) {
        if (s >= VI_SUCCESS) return true;
        char msg[512] = {0}; niDigital_GetError(m_vi, &s, 512, msg);
        *e = QString("niDigital [%1] : %2").arg(w, QString::fromLocal8Bit(msg)); return false;
    }
public:
    QString name() const override { return "niDigital (PXIe-6570)"; }
    bool open(const QString& dev, QString* e) override {
        if (!chk(niDigital_InitWithOptions(dev.toLocal8Bit().constData(), VI_TRUE, VI_TRUE, "", &m_vi), e, "InitWithOptions")) return false;
        QSettings s;
        auto path = [&](const char* k){ return s.value(k).toString().toLocal8Bit(); };
        if (!chk(niDigital_LoadPinMap(m_vi, path("hw/pinmapFile").constData()), e, "LoadPinMap")) return false;
        if (!chk(niDigital_LoadLevels(m_vi, path("hw/levelsFile").constData()), e, "LoadLevels")) return false;
        if (!chk(niDigital_LoadTiming(m_vi, path("hw/timingFile").constData()), e, "LoadTiming")) return false;
        if (!chk(niDigital_LoadPattern(m_vi, path("hw/patternFile").constData()), e, "LoadPattern")) return false;
        return true;
    }
    bool load(const MliSequence& q, const QString&, QString* e) override {
        // Excel : niDigital_WriteSourceWaveform
        return chk(niDigital_WriteSourceWaveformBroadcastU32(m_vi, "mliWave", q.words.size(),
                   const_cast<ViUInt32*>(reinterpret_cast<const ViUInt32*>(q.words.constData()))), e, "WriteSourceWaveform");
    }
    bool start(QString* e) override {
        // Excel : niDigital_BurstPattern
        return chk(niDigital_BurstPattern(m_vi, "", m_startLabel.toLocal8Bit().constData(), VI_TRUE, VI_FALSE, 10.0), e, "BurstPattern");
    }
    void stop() override { if (m_vi != VI_NULL) niDigital_Abort(m_vi); }
    void close() override { if (m_vi != VI_NULL) { niDigital_Abort(m_vi); niDigital_close(m_vi); m_vi = VI_NULL; } }
};
MliOutput* MliOutput::create() { return new NiDigitalMliOutput; }
#else
MliOutput* MliOutput::create() { return new NullMliOutput; }
#endif
