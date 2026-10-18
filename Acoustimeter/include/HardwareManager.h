#pragma once
// HardwareManager : toute la communication avec le materiel NI (NI-DAQmx + sortie MLI).
// Les appels NI s'executent dans un thread dedie ; la GUI ne parle au materiel que par signaux.
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QThread>
#include <QTimer>
#include <QSet>
#include <QHash>
#include <atomic>
#include "DataTypes.h"
#include "MliGenerator.h"

class MliOutput;

struct NiDevice {
    QString name;          // nom NI MAX, ex. "Dev1"
    QString productType;   // ex. "PXIe-4353"
    QString serial;
};

struct TempChannelCfg {
    QString physicalChannel;   // "Dev2/ai0"
    QString label;
    QString tcType = "K";
    double  minC = -50.0, maxC = 400.0;
};

struct SlowChannelCfg {        // tension ou courant lent (DAQmx analogique)
    QString physicalChannel;
    QString label;
    QChar   kind = 'V';        // 'V' ou 'I'
    double  factor = 1.0;      // diviseur de sonde / echelle
};

struct AcousticChannelCfg {
    QString physicalChannel;   // "Dev3/ai0"
    QString key;               // "h1"
    QString label;             // "Hydro 1"
    double  sampleRate = 96000.0;
    double  rangeV = 10.0;
    double  gainDb = 0.0;      // gain preampli retire pour afficher la tension a l'hydrophone
};

Q_DECLARE_METATYPE(NiDevice)
Q_DECLARE_METATYPE(TempChannelCfg)
Q_DECLARE_METATYPE(SlowChannelCfg)
Q_DECLARE_METATYPE(AcousticChannelCfg)

// ---------------------------------------------------------------- Worker (thread NI)
class HardwareWorker : public QObject
{
    Q_OBJECT
public:
    explicit HardwareWorker(QObject* parent = nullptr);
public slots:
    void detectDevices();
    void startTemperature(const QVector<TempChannelCfg>& chans, double rateHz);
    void startSlow(const QVector<SlowChannelCfg>& chans, double rateHz);
    void stopMonitoring();
    void startAcoustic(const QVector<AcousticChannelCfg>& chans);
    void stopAcoustic();
    void generateMli(const AmplifierConfig& cfg);
    void startEmission(const QVector<int>& ampIds, const QString& digitalDevice);
    void stopEmission();
    void setTempLimit(double c);
    void selfCalibrate(const QString& deviceName);
    void shutdown();
private slots:
    void pollTemperature();
    void pollSlow();
    void pollAcoustic();
signals:
    void devicesDetected(const QVector<NiDevice>& devices);
    void temperatureUpdated(const QString& label, double celsius);
    void slowUpdated(const QString& label, double value, QChar kind);
    void acousticBlock(const QString& key, const QVector<double>& volts, double sampleRate);
    void saturation(const QString& key, bool over95);
    void mliGenerated(int ampId, bool ok, const QString& message, const QVector<quint8>& preview);
    void emissionStateChanged(bool on);
    void logMessage(const QString& level, const QString& text);
    void errorOccurred(const QString& text);
private:
    bool check(int err, const char* where);
    void clearTask(void*& t);
    void failSafe(const QString& why);          // coupe l'emission cote worker

    void* m_tempTask = nullptr;
    void* m_slowTask = nullptr;
    void* m_acqTask  = nullptr;
    QTimer* m_tempTimer = nullptr;
    QTimer* m_slowTimer = nullptr;
    QTimer* m_acqTimer  = nullptr;
    QVector<TempChannelCfg>     m_tempCfg;
    QVector<SlowChannelCfg>     m_slowCfg;
    QVector<AcousticChannelCfg> m_acqCfg;
    QHash<int, MliSequence> m_seq;
    QHash<int, QString>     m_pin;
    MliOutput* m_mli = nullptr;
    bool   m_emitting = false;
    double m_tempLimit = 80.0;
    double m_acqRate = 0.0;
};

// ---------------------------------------------------------------- Manager (cote GUI)
class HardwareManager : public QObject
{
    Q_OBJECT
public:
    explicit HardwareManager(QObject* parent = nullptr);
    ~HardwareManager() override;

    static bool niCompiled();
    const QVector<NiDevice>& devices() const { return m_devices; }
    QString deviceOf(const QString& productContains) const;   // "4353" -> "Dev2"
    QString digitalDevice() const;                            // 6570 ou 6537
    bool online() const { return !m_devices.isEmpty(); }
    bool acquiring() const { return m_acquiring; }
    bool emitting() const { return m_emitting; }
    bool isMliReady(int ampId) const { return m_mliReady.contains(ampId); }
    bool isAmpActive(int ampId) const { return m_activeAmps.contains(ampId); }
    QStringList tempLabels() const { return m_tempLabels; }
    QStringList voltLabels() const { return m_voltLabels; }
    QStringList currLabels() const { return m_currLabels; }
    bool checkSafetyForEmission(QString* reason) const;

public slots:
    void detect();
    void applyConfig(const SystemConfig& cfg);   // (re)demarre la surveillance (T, V, I)
    void startAcquisition();                     // hydrophones (PXIe-4480)
    void stopAcquisition();
    void generateMli(const AmplifierConfig& cfg);
    void setAmpActive(int ampId, bool on);
    void startEmission();
    void stopEmission();
    void emergencyStop();
    void selfCalibrate();
    void setTempLimit(double c);

signals:
    void devicesDetected(const QVector<NiDevice>&);
    void temperatureUpdated(const QString& label, double celsius);
    void slowUpdated(const QString& label, double value, QChar kind);
    void acousticBlock(const QString& key, const QVector<double>& volts, double sampleRate);
    void saturation(const QString& key, bool over95);
    void mliGenerated(int ampId, bool ok, const QString& message, const QVector<quint8>& preview);
    void emissionStateChanged(bool on);
    void channelsConfigured();
    void logMessage(const QString& level, const QString& text);
    void errorOccurred(const QString& text);

    void reqDetect();
    void reqStartTemp(const QVector<TempChannelCfg>&, double);
    void reqStartSlow(const QVector<SlowChannelCfg>&, double);
    void reqStopMon();
    void reqStartAcq(const QVector<AcousticChannelCfg>&);
    void reqStopAcq();
    void reqGenMli(const AmplifierConfig&);
    void reqStartEmission(const QVector<int>&, const QString&);
    void reqStopEmission();
    void reqSelfCal(const QString&);
    void reqTempLimit(double);

private:
    QThread m_thread;
    HardwareWorker* m_worker = nullptr;
    QVector<NiDevice> m_devices;
    SystemConfig m_cfg;
    QSet<int> m_mliReady, m_activeAmps;
    QStringList m_tempLabels, m_voltLabels, m_currLabels;
    bool m_acquiring = false, m_emitting = false, m_commError = false, m_tempRunning = false;
};
