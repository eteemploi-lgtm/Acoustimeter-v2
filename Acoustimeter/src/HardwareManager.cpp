#include "HardwareManager.h"
#include "MliOutput.h"
#include <QMetaType>
#include <QSettings>
#include <QtMath>
#include <algorithm>
#include <cmath>

#ifdef ACOUSTIMETER_WITH_NI
#include <NIDAQmx.h>
// Codes NI utilises localement (a verifier dans NIDAQmx.h installe : "SamplesNotYetAvailable")
static const int kErrSamplesNotYetAvailable = -200284;
#endif

// ======================================================================= Worker
HardwareWorker::HardwareWorker(QObject* parent) : QObject(parent) {}

void HardwareWorker::failSafe(const QString& why)
{
    if (m_emitting) {
        if (m_mli) m_mli->stop();
        m_emitting = false;
        emit emissionStateChanged(false);
        emit logMessage("err", "EMISSION COUPEE (securite) : " + why);
    }
}

#ifdef ACOUSTIMETER_WITH_NI
// -------------------------------------------------------------- avec NI-DAQmx
bool HardwareWorker::check(int err, const char* where)
{
    if (!DAQmxFailed(err)) return true;
    char buf[2048] = {0};
    DAQmxGetExtendedErrorInfo(buf, sizeof(buf));
    emit errorOccurred(QString("NI-DAQmx [%1] code %2 : %3")
                       .arg(where).arg(err).arg(QString::fromLocal8Bit(buf).trimmed()));
    return false;
}

void HardwareWorker::clearTask(void*& t)
{
    if (!t) return;
    DAQmxStopTask((TaskHandle)t);
    DAQmxClearTask((TaskHandle)t);
    t = nullptr;
}

void HardwareWorker::detectDevices()
{
    QVector<NiDevice> out;
    char names[8192] = {0};
    if (!check(DAQmxGetSysDevNames(names, sizeof(names)), "GetSysDevNames")) { emit devicesDetected(out); return; }
    const QStringList list = QString::fromLocal8Bit(names).split(",", Qt::SkipEmptyParts);
    for (QString n : list) {
        n = n.trimmed();
        NiDevice d; d.name = n;
        char prod[256] = {0};
        if (DAQmxGetDevProductType(n.toLocal8Bit().constData(), prod, sizeof(prod)) == 0)
            d.productType = QString::fromLocal8Bit(prod);
        uInt32 sn = 0;
        if (DAQmxGetDevSerialNum(n.toLocal8Bit().constData(), &sn) == 0)
            d.serial = QString::number(sn, 16).toUpper();
        out.push_back(d);
    }
    emit devicesDetected(out);
}

static int32 tcTypeToNi(const QString& t)
{
    if (t == "J") return DAQmx_Val_J_Type_TC;
    if (t == "T") return DAQmx_Val_T_Type_TC;
    if (t == "E") return DAQmx_Val_E_Type_TC;
    if (t == "N") return DAQmx_Val_N_Type_TC;
    if (t == "R") return DAQmx_Val_R_Type_TC;
    if (t == "S") return DAQmx_Val_S_Type_TC;
    if (t == "B") return DAQmx_Val_B_Type_TC;
    return DAQmx_Val_K_Type_TC;
}

void HardwareWorker::startTemperature(const QVector<TempChannelCfg>& chans, double rateHz)
{
    if (m_tempTimer) m_tempTimer->stop();
    clearTask(m_tempTask);
    if (chans.isEmpty()) return;
    if (rateHz < 0.01 || rateHz > 100.0) { emit errorOccurred("Frequence temperature hors plage (0.01 - 100 Hz)"); return; }
    TaskHandle t = nullptr;
    if (!check(DAQmxCreateTask("temperature", &t), "CreateTask(temperature)")) return;
    m_tempTask = t; m_tempCfg = chans;
    for (const auto& c : chans) {
        // Excel : DAQmx Create AI Temp TC Channel / AI Temp TC Type / CJC source Built-In
        if (!check(DAQmxCreateAIThrmcplChan(t, c.physicalChannel.toLocal8Bit().constData(),
                   c.label.toLocal8Bit().constData(), c.minC, c.maxC, DAQmx_Val_DegC,
                   tcTypeToNi(c.tcType), DAQmx_Val_BuiltIn, 0.0, ""), "CreateAIThrmcplChan")) { clearTask(m_tempTask); return; }
    }
    // Excel : AI ADCLowpass CutoffFreq -> a ajouter ici apres verification du nom exact dans NIDAQmx.h
    //         (PowerShell : Select-String -Path <NIDAQmx.h> -Pattern "Lowpass")
    if (!check(DAQmxCfgSampClkTiming(t, "", rateHz, DAQmx_Val_Rising, DAQmx_Val_ContSamps, 100), "CfgSampClkTiming(temp)")) { clearTask(m_tempTask); return; }
    if (!check(DAQmxStartTask(t), "StartTask(temp)")) { clearTask(m_tempTask); return; }
    if (!m_tempTimer) { m_tempTimer = new QTimer(this); connect(m_tempTimer, &QTimer::timeout, this, &HardwareWorker::pollTemperature); }
    m_tempTimer->start(qMax(100, int(400.0 / rateHz)));
    emit logMessage("ok", QString("Surveillance temperature : %1 voie(s), %2 Hz").arg(chans.size()).arg(rateHz));
}

void HardwareWorker::startSlow(const QVector<SlowChannelCfg>& chans, double rateHz)
{
    if (m_slowTimer) m_slowTimer->stop();
    clearTask(m_slowTask);
    if (chans.isEmpty()) return;
    if (rateHz < 0.01 || rateHz > 10000.0) { emit errorOccurred("Frequence tension/courant hors plage"); return; }
    TaskHandle t = nullptr;
    if (!check(DAQmxCreateTask("slow", &t), "CreateTask(slow)")) return;
    m_slowTask = t; m_slowCfg = chans;
    for (const auto& c : chans)
        if (!check(DAQmxCreateAIVoltageChan(t, c.physicalChannel.toLocal8Bit().constData(), c.label.toLocal8Bit().constData(),
                   DAQmx_Val_Cfg_Default, -10.0, 10.0, DAQmx_Val_Volts, nullptr), "CreateAIVoltageChan(slow)")) { clearTask(m_slowTask); return; }
    if (!check(DAQmxCfgSampClkTiming(t, "", rateHz, DAQmx_Val_Rising, DAQmx_Val_ContSamps, 1000), "CfgSampClkTiming(slow)")) { clearTask(m_slowTask); return; }
    if (!check(DAQmxStartTask(t), "StartTask(slow)")) { clearTask(m_slowTask); return; }
    if (!m_slowTimer) { m_slowTimer = new QTimer(this); connect(m_slowTimer, &QTimer::timeout, this, &HardwareWorker::pollSlow); }
    m_slowTimer->start(qMax(100, int(400.0 / rateHz)));
    emit logMessage("ok", QString("Mesures tension/courant : %1 voie(s), %2 Hz").arg(chans.size()).arg(rateHz));
}

void HardwareWorker::stopMonitoring()
{
    if (m_tempTimer) m_tempTimer->stop();
    if (m_slowTimer) m_slowTimer->stop();
    clearTask(m_tempTask);
    clearTask(m_slowTask);
}

void HardwareWorker::pollTemperature()
{
    if (!m_tempTask) return;
    const int n = m_tempCfg.size();
    QVector<float64> buf(n * 1000);
    int32 read = 0;
    int err = DAQmxReadAnalogF64((TaskHandle)m_tempTask, -1, 0.0, DAQmx_Val_GroupByChannel, buf.data(), buf.size(), &read, nullptr);
    if (err == kErrSamplesNotYetAvailable) return;
    if (!check(err, "ReadAnalogF64(temp)")) { failSafe("erreur lecture temperature"); stopMonitoring(); return; }
    if (read <= 0) return;
    for (int c = 0; c < n; ++c) {
        const double v = buf[c * read + (read - 1)];
        if (qIsNaN(v) || qIsInf(v)) {
            emit errorOccurred(QString("Voie %1 : valeur invalide (thermocouple deconnecte ?)").arg(m_tempCfg[c].label));
            failSafe("thermocouple invalide : " + m_tempCfg[c].label);
            continue;
        }
        if (v > m_tempLimit) failSafe(QString("%1 = %2 C > limite %3 C").arg(m_tempCfg[c].label).arg(v, 0, 'f', 1).arg(m_tempLimit));
        emit temperatureUpdated(m_tempCfg[c].label, v);
    }
}

void HardwareWorker::pollSlow()
{
    if (!m_slowTask) return;
    const int n = m_slowCfg.size();
    QVector<float64> buf(n * 2000);
    int32 read = 0;
    int err = DAQmxReadAnalogF64((TaskHandle)m_slowTask, -1, 0.0, DAQmx_Val_GroupByChannel, buf.data(), buf.size(), &read, nullptr);
    if (err == kErrSamplesNotYetAvailable) return;
    if (!check(err, "ReadAnalogF64(slow)")) { stopMonitoring(); return; }
    if (read <= 0) return;
    for (int c = 0; c < n; ++c)
        emit slowUpdated(m_slowCfg[c].label, buf[c * read + (read - 1)] * m_slowCfg[c].factor, m_slowCfg[c].kind);
}

void HardwareWorker::startAcoustic(const QVector<AcousticChannelCfg>& chans)
{
    stopAcoustic();
    if (chans.isEmpty()) return;
    TaskHandle t = nullptr;
    if (!check(DAQmxCreateTask("acoustic", &t), "CreateTask(acoustic)")) return;
    m_acqTask = t; m_acqCfg = chans;
    const double fs = chans.first().sampleRate;
    for (const auto& c : chans) {
        if (c.rangeV <= 0.0) { emit errorOccurred("Dynamique invalide : " + c.label); clearTask(m_acqTask); return; }
        // Excel : Physical Channel
        if (!check(DAQmxCreateAIVoltageChan(t, c.physicalChannel.toLocal8Bit().constData(), c.key.toLocal8Bit().constData(),
                   DAQmx_Val_Cfg_Default, -c.rangeV, c.rangeV, DAQmx_Val_Volts, nullptr), "CreateAIVoltageChan(acoustic)")) { clearTask(m_acqTask); return; }
    }
    // Excel : DAQmx Cfg Samp Clk Timing
    if (!check(DAQmxCfgSampClkTiming(t, "", fs, DAQmx_Val_Rising, DAQmx_Val_ContSamps, (uInt64)(fs)), "CfgSampClkTiming(acoustic)")) { clearTask(m_acqTask); return; }
    float64 actual = fs;
    if (DAQmxGetSampClkRate(t, &actual) != 0) actual = fs;      // cadence reellement appliquee (coercition NI)
    m_acqRate = actual;
    // Excel : DAQmx Create Custom Scale -> echelle appliquee en logiciel (gain preampli retire dans pollAcoustic)
    if (!check(DAQmxStartTask(t), "StartTask(acoustic)")) { clearTask(m_acqTask); return; }
    if (!m_acqTimer) { m_acqTimer = new QTimer(this); connect(m_acqTimer, &QTimer::timeout, this, &HardwareWorker::pollAcoustic); }
    m_acqTimer->start(30);
    emit logMessage("ok", QString("Acquisition hydrophones : %1 voie(s), %2 Hz (demande %3 Hz)").arg(chans.size()).arg(actual).arg(fs));
}

void HardwareWorker::stopAcoustic()
{
    if (m_acqTimer) m_acqTimer->stop();
    if (m_acqTask) { clearTask(m_acqTask); emit logMessage("warn", "Acquisition hydrophones arretee"); }
}

void HardwareWorker::pollAcoustic()
{
    if (!m_acqTask) return;
    const int n = m_acqCfg.size();
    const int cap = 65536;
    QVector<float64> buf(n * cap);
    int32 read = 0;
    int err = DAQmxReadAnalogF64((TaskHandle)m_acqTask, -1, 0.0, DAQmx_Val_GroupByChannel, buf.data(), buf.size(), &read, nullptr);
    if (err == kErrSamplesNotYetAvailable) return;
    // Excel : Read -> Overwriting check. Une erreur de lecture (dont ecrasement / debordement) arrete tout.
    if (!check(err, "ReadAnalogF64(acoustic)")) { stopAcoustic(); return; }
    if (read <= 0) return;
    for (int c = 0; c < n; ++c) {
        QVector<double> s(read); double peak = 0.0;
        const double g = std::pow(10.0, -m_acqCfg[c].gainDb / 20.0);
        for (int i = 0; i < read; ++i) { const double raw = buf[c * read + i]; peak = std::max(peak, std::abs(raw)); s[i] = raw * g; }
        emit saturation(m_acqCfg[c].key, peak >= 0.95 * m_acqCfg[c].rangeV);     // voyant rouge > 95 % de la dynamique
        emit acousticBlock(m_acqCfg[c].key, s, m_acqRate);
    }
}

void HardwareWorker::selfCalibrate(const QString& dev)
{
    emit logMessage("info", "Auto-calibration " + dev + " en cours...");
    // Excel : DAQmx Perform Self Calibration (auto-zero)
    if (check(DAQmxSelfCal(dev.toLocal8Bit().constData()), "SelfCal")) emit logMessage("ok", "Auto-calibration terminee : " + dev);
}

#else
// ------------------------------------------------------------------ sans NI
static const char* NO_NI = "Application compilee sans NI-DAQmx (ACOUSTIMETER_WITH_NI=OFF)";
bool HardwareWorker::check(int, const char*) { return false; }
void HardwareWorker::clearTask(void*&) {}
void HardwareWorker::detectDevices() { emit devicesDetected({}); emit errorOccurred(NO_NI); }
void HardwareWorker::startTemperature(const QVector<TempChannelCfg>&, double) { emit errorOccurred(NO_NI); }
void HardwareWorker::startSlow(const QVector<SlowChannelCfg>&, double) {}
void HardwareWorker::stopMonitoring() {}
void HardwareWorker::pollTemperature() {}
void HardwareWorker::pollSlow() {}
void HardwareWorker::startAcoustic(const QVector<AcousticChannelCfg>&) { emit errorOccurred(NO_NI); }
void HardwareWorker::stopAcoustic() {}
void HardwareWorker::pollAcoustic() {}
void HardwareWorker::selfCalibrate(const QString&) { emit errorOccurred(NO_NI); }
#endif

// ------------------------------------------------- MLI et emission (commun)
void HardwareWorker::generateMli(const AmplifierConfig& cfg)
{
    MliSequence seq; QString err;
    if (!MliGenerator::generate(cfg, 1.0, &seq, &err)) {
        emit mliGenerated(cfg.id, false, err, {});
        return;
    }
    QVector<quint8> prev = seq.bits.mid(0, qMin(seq.bits.size(), 4 * MliGenerator::STEPS_PER_CARRIER));
    m_seq[cfg.id] = seq;
    m_pin[cfg.id] = cfg.mliPin.isEmpty() ? QString("MLI%1").arg(cfg.id) : cfg.mliPin;
    emit mliGenerated(cfg.id, true,
        QString("MLI generee : %1 echantillons, horloge %2 MHz").arg(seq.bits.size()).arg(seq.clockHz / 1e6, 0, 'f', 3), prev);
}

void HardwareWorker::startEmission(const QVector<int>& ampIds, const QString& digitalDevice)
{
    if (m_emitting) return;
#ifdef ACOUSTIMETER_WITH_NI
    if (!m_tempTask) { emit logMessage("err", "Emission refusee : surveillance thermique inactive"); return; }
#endif
    if (ampIds.isEmpty()) { emit logMessage("err", "Emission refusee : aucune voie amplificateur activee"); return; }
    if (!m_mli) m_mli = MliOutput::create();
    QString err;
    if (!m_mli->open(digitalDevice, &err)) { emit logMessage("err", "Sortie MLI : " + err); return; }
    for (int id : ampIds) {
        if (!m_seq.contains(id)) { emit logMessage("err", QString("Amp %1 : MLI non generee").arg(id)); m_mli->close(); return; }
        if (!m_mli->load(m_seq[id], m_pin.value(id), &err)) { emit logMessage("err", QString("Amp %1 : %2").arg(id).arg(err)); m_mli->close(); return; }
    }
    if (!m_mli->start(&err)) { emit logMessage("err", "Demarrage MLI : " + err); m_mli->stop(); m_mli->close(); return; }
    m_emitting = true;
    emit emissionStateChanged(true);
    emit logMessage("ok", QString("Emission demarree (%1 voie(s)) via %2").arg(ampIds.size()).arg(m_mli->name()));
}

void HardwareWorker::stopEmission()
{
    if (!m_mli) return;
    m_mli->stop();
    m_mli->close();
    if (m_emitting) { m_emitting = false; emit emissionStateChanged(false); emit logMessage("warn", "Emission arretee"); }
}

void HardwareWorker::setTempLimit(double c) { m_tempLimit = c; }

void HardwareWorker::shutdown()
{
    stopEmission();
    stopAcoustic();
    stopMonitoring();
    delete m_mli; m_mli = nullptr;
}

// ======================================================================= Manager
bool HardwareManager::niCompiled()
{
#ifdef ACOUSTIMETER_WITH_NI
    return true;
#else
    return false;
#endif
}

HardwareManager::HardwareManager(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<NiDevice>("NiDevice");
    qRegisterMetaType<QVector<NiDevice>>("QVector<NiDevice>");
    qRegisterMetaType<QVector<TempChannelCfg>>("QVector<TempChannelCfg>");
    qRegisterMetaType<QVector<SlowChannelCfg>>("QVector<SlowChannelCfg>");
    qRegisterMetaType<QVector<AcousticChannelCfg>>("QVector<AcousticChannelCfg>");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<QVector<quint8>>("QVector<quint8>");
    qRegisterMetaType<QVector<int>>("QVector<int>");
    qRegisterMetaType<AmplifierConfig>("AmplifierConfig");

    m_worker = new HardwareWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &HardwareManager::reqDetect,        m_worker, &HardwareWorker::detectDevices);
    connect(this, &HardwareManager::reqStartTemp,     m_worker, &HardwareWorker::startTemperature);
    connect(this, &HardwareManager::reqStartSlow,     m_worker, &HardwareWorker::startSlow);
    connect(this, &HardwareManager::reqStopMon,       m_worker, &HardwareWorker::stopMonitoring);
    connect(this, &HardwareManager::reqStartAcq,      m_worker, &HardwareWorker::startAcoustic);
    connect(this, &HardwareManager::reqStopAcq,       m_worker, &HardwareWorker::stopAcoustic);
    connect(this, &HardwareManager::reqGenMli,        m_worker, &HardwareWorker::generateMli);
    connect(this, &HardwareManager::reqStartEmission, m_worker, &HardwareWorker::startEmission);
    connect(this, &HardwareManager::reqStopEmission,  m_worker, &HardwareWorker::stopEmission);
    connect(this, &HardwareManager::reqSelfCal,       m_worker, &HardwareWorker::selfCalibrate);
    connect(this, &HardwareManager::reqTempLimit,     m_worker, &HardwareWorker::setTempLimit);

    connect(m_worker, &HardwareWorker::devicesDetected, this, [this](const QVector<NiDevice>& d) {
        m_devices = d; emit devicesDetected(d); });
    connect(m_worker, &HardwareWorker::temperatureUpdated, this, [this](const QString& l, double c){ m_tempRunning = true; emit temperatureUpdated(l, c); });
    connect(m_worker, &HardwareWorker::slowUpdated,        this, &HardwareManager::slowUpdated);
    connect(m_worker, &HardwareWorker::acousticBlock,      this, &HardwareManager::acousticBlock);
    connect(m_worker, &HardwareWorker::saturation,         this, &HardwareManager::saturation);
    connect(m_worker, &HardwareWorker::logMessage,         this, &HardwareManager::logMessage);
    connect(m_worker, &HardwareWorker::mliGenerated, this,
            [this](int id, bool ok, const QString& msg, const QVector<quint8>& pv) {
        if (ok) m_mliReady.insert(id); else { m_mliReady.remove(id); m_activeAmps.remove(id); }
        emit mliGenerated(id, ok, msg, pv); });
    connect(m_worker, &HardwareWorker::emissionStateChanged, this, [this](bool on){ m_emitting = on; emit emissionStateChanged(on); });
    connect(m_worker, &HardwareWorker::errorOccurred, this, [this](const QString& t) {
        m_commError = true; emit errorOccurred(t); });
    m_thread.start();
}

HardwareManager::~HardwareManager()
{
    QMetaObject::invokeMethod(m_worker, "shutdown", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

QString HardwareManager::deviceOf(const QString& contains) const
{
    for (const auto& d : m_devices) if (d.productType.contains(contains)) return d.name;
    return QString();
}

QString HardwareManager::digitalDevice() const
{
    QString d = deviceOf("6570");
    return d.isEmpty() ? deviceOf("6537") : d;
}

void HardwareManager::detect() { m_commError = false; emit reqDetect(); }

void HardwareManager::applyConfig(const SystemConfig& cfg)
{
    m_cfg = cfg;
    m_commError = false;
    emit reqStopMon();
    m_tempLabels.clear(); m_voltLabels.clear(); m_currLabels.clear();

    // ---- Temperatures : PXIe-4353 ----
    const QString dev4353 = deviceOf("4353");
    QVector<TempChannelCfg> temps; double tRate = 0; int idx = 0;
    for (const auto& a : cfg.amplifiers) {
        for (int j = 0; j < a.tempProbeCount; ++j) {
            TempChannelCfg t;
            t.physicalChannel = QString("%1/ai%2").arg(dev4353).arg(idx++);
            const QString base = a.tempLabel.isEmpty() ? QString("T_AMP%1").arg(a.id) : a.tempLabel;
            t.label = a.tempProbeCount > 1 ? QString("%1_%2").arg(base).arg(j + 1) : base;
            t.tcType = a.tempType;
            temps << t; m_tempLabels << t.label;
            if (tRate == 0) tRate = a.tempAcqFreq; else if (!qFuzzyCompare(tRate, a.tempAcqFreq))
                emit logMessage("warn", "Frequences temperature differentes : la premiere est appliquee (" + QString::number(tRate) + " Hz)");
        }
    }
    if (!temps.isEmpty()) {
        if (dev4353.isEmpty()) emit logMessage("err", "PXIe-4353 absente : temperatures indisponibles (emission interdite)");
        else if (idx > 32)     emit logMessage("err", "Plus de 32 sondes demandees (PXIe-4353 : 32 voies)");
        else emit reqStartTemp(temps, tRate);
    }

    // ---- Tensions / courants : voies physiques lues dans QSettings (Outils > Mapping materiel) ----
    QSettings st;
    auto build = [&](bool volt) {
        const QStringList phys = st.value(volt ? "hw/voltChannels" : "hw/currChannels").toString()
                                   .split(",", Qt::SkipEmptyParts);
        QVector<SlowChannelCfg> out; double rate = 0; int k = 0; bool warned = false;
        for (const auto& a : cfg.amplifiers) {
            const int n = volt ? a.voltChannelCount : a.currChannelCount;
            const QString div = volt ? a.voltDivision : a.currDivision;
            const double f = div == "1/100" ? 100.0 : div == "1/1000" ? 1000.0 : 10.0;
            for (int j = 0; j < n; ++j) {
                if (k >= phys.size()) { if (!warned) { emit logMessage("warn", QString("%1 : voies physiques non assignees (Outils > Mapping materiel)").arg(volt ? "Tensions" : "Courants")); warned = true; } continue; }
                SlowChannelCfg c; c.physicalChannel = phys[k++].trimmed(); c.kind = volt ? 'V' : 'I'; c.factor = f;
                const QString base = volt ? a.voltLabel : a.currLabel;
                c.label = (base.isEmpty() ? QString("%1_AMP%2").arg(volt ? "V" : "I").arg(a.id) : base)
                          + (n > 1 ? QString("_%1").arg(j + 1) : QString());
                out << c; (volt ? m_voltLabels : m_currLabels) << c.label;
                if (rate == 0) rate = volt ? a.voltAcqFreq : a.currAcqFreq;
            }
        }
        return qMakePair(out, rate);
    };
    auto v = build(true), i = build(false);
    QVector<SlowChannelCfg> slow = v.first + i.first;
    if (!slow.isEmpty()) emit reqStartSlow(slow, v.second > 0 ? v.second : i.second);
    emit channelsConfigured();
}

void HardwareManager::startAcquisition()
{
    const QString dev = deviceOf("4480");
    if (dev.isEmpty()) { emit errorOccurred("PXIe-4480 absente : acquisition hydrophones impossible"); return; }
    QVector<AcousticChannelCfg> ch;
    for (const auto& h : m_cfg.hydrophones) {
        if (h.channel < 1 || h.channel > 6) { emit errorOccurred(QString("Hydro %1 : voie %2 hors plage (1-6 sur PXIe-4480)").arg(h.id).arg(h.channel)); return; }
        if (h.sampleRate < 1000 || h.sampleRate > 204800) { emit errorOccurred(QString("Hydro %1 : frequence hors plage PXIe-4480 (max 204,8 kHz)").arg(h.id)); return; }
        AcousticChannelCfg c;
        c.physicalChannel = QString("%1/ai%2").arg(dev).arg(h.channel - 1);
        c.key = QString("h%1").arg(h.id); c.label = QString("Hydro %1").arg(h.id);
        c.sampleRate = h.sampleRate; c.rangeV = h.dynamic; c.gainDb = h.gain;
        if (!ch.isEmpty() && ch.first().sampleRate != c.sampleRate)
            emit logMessage("warn", "Frequences differentes entre hydrophones : celle de Hydro 1 est appliquee a toutes les voies");
        ch << c;
    }
    if (ch.isEmpty()) { emit errorOccurred("Aucun hydrophone configure"); return; }
    m_acquiring = true;
    emit reqStartAcq(ch);
}

void HardwareManager::stopAcquisition() { m_acquiring = false; emit reqStopAcq(); }
void HardwareManager::generateMli(const AmplifierConfig& cfg) { emit reqGenMli(cfg); }

void HardwareManager::setAmpActive(int id, bool on)
{
    if (on && !m_mliReady.contains(id)) { emit errorOccurred(QString("Amp %1 : generer la MLI avant d'activer la voie").arg(id)); return; }
    if (on) m_activeAmps.insert(id); else m_activeAmps.remove(id);
    emit logMessage("info", QString("Amp %1 : voie %2").arg(id).arg(on ? "activee" : "desactivee"));
}

bool HardwareManager::checkSafetyForEmission(QString* reason) const
{
    auto fail = [&](const QString& r) { if (reason) *reason = r; return false; };
    if (!niCompiled())            return fail("build sans NI-DAQmx");
    if (m_devices.isEmpty())      return fail("aucune carte NI detectee");
    if (digitalDevice().isEmpty()) return fail("carte numerique MLI (6570/6537) absente");
    if (deviceOf("4353").isEmpty()) return fail("PXIe-4353 absente : pas de surveillance thermique");
    if (!m_tempRunning)           return fail("aucune mesure de temperature recue");
    if (m_commError)              return fail("erreur de communication NI active (relancer la detection)");
    if (m_activeAmps.isEmpty())   return fail("aucune voie amplificateur activee (Configuration > Activer voie)");
    return true;
}

void HardwareManager::startEmission()
{
    QString why;
    if (!checkSafetyForEmission(&why)) { emit errorOccurred("Emission refusee : " + why); return; }
    QVector<int> ids; for (int id : m_activeAmps) ids << id;
    emit reqStartEmission(ids, digitalDevice());
}

void HardwareManager::stopEmission() { emit reqStopEmission(); }

void HardwareManager::emergencyStop()
{
    emit reqStopEmission();
    emit reqStopAcq(); m_acquiring = false;
}

void HardwareManager::selfCalibrate()
{
    for (const auto& d : m_devices)
        if (d.productType.contains("4480") || d.productType.contains("4353")) emit reqSelfCal(d.name);
}

void HardwareManager::setTempLimit(double c) { emit reqTempLimit(c); }
