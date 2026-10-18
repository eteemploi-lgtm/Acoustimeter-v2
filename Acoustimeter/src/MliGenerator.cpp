#include "MliGenerator.h"
#include <QFile>
#include <QRegularExpression>
#include <QDataStream>
#include <QTextStream>
#include <QtMath>
#include <algorithm>
#include <cmath>

bool MliGenerator::readWav(const QString& path, QVector<double>* s, double* fs, QString* err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { *err = "Fichier WAV introuvable : " + path; return false; }
    QByteArray d = f.readAll();
    if (d.size() < 44 || d.mid(0,4) != "RIFF" || d.mid(8,4) != "WAVE") { *err = "WAV invalide : " + path; return false; }
    auto u16 = [&](int o){ return quint16(quint8(d[o]) | (quint8(d[o+1])<<8)); };
    auto u32 = [&](int o){ return quint32(quint8(d[o]) | (quint8(d[o+1])<<8) | (quint8(d[o+2])<<16) | (quint32(quint8(d[o+3]))<<24)); };
    int pos = 12; int fmtTag=0, ch=1, bits=16; quint32 rate=0; int dataPos=-1; quint32 dataLen=0;
    while (pos + 8 <= d.size()) {
        QByteArray id = d.mid(pos,4); quint32 len = u32(pos+4);
        if (id == "fmt ") { fmtTag=u16(pos+8); ch=u16(pos+10); rate=u32(pos+12); bits=u16(pos+22); }
        else if (id == "data") { dataPos=pos+8; dataLen=qMin<quint32>(len, d.size()-dataPos); break; }
        pos += 8 + len + (len & 1);
    }
    if (dataPos < 0 || rate == 0 || ch < 1) { *err = "WAV : en-tetes incomplets"; return false; }
    if (!((fmtTag == 1 && (bits==16 || bits==24 || bits==32 || bits==8)) || (fmtTag == 3 && bits == 32))) {
        *err = QString("WAV : format non supporte (tag %1, %2 bits)").arg(fmtTag).arg(bits); return false; }
    const int bps = bits/8; const int frames = dataLen / (bps*ch);
    s->resize(frames);
    for (int i=0;i<frames;i++) {
        int o = dataPos + i*bps*ch; double v=0;       // premiere voie seulement
        if (fmtTag==3) { quint32 r=u32(o); float fl; memcpy(&fl,&r,4); v=fl; }
        else if (bits==8)  v = (quint8(d[o]) - 128) / 128.0;
        else if (bits==16) v = qint16(u16(o)) / 32768.0;
        else if (bits==24) { qint32 x = (quint8(d[o])<<8)|(quint8(d[o+1])<<16)|(quint32(quint8(d[o+2]))<<24); v = (x>>8)/8388608.0; }
        else               v = qint32(u32(o)) / 2147483648.0;
        (*s)[i] = v;
    }
    *fs = rate; return true;
}

bool MliGenerator::readFilter(const QString& path, QVector<double>* taps, QString* err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { *err = "Filtre introuvable : " + path; return false; }
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        for (const QString& tok : line.split(QRegularExpression("[;,\\s]+"), Qt::SkipEmptyParts)) {
            bool ok=false; double v = tok.toDouble(&ok);
            if (ok) taps->push_back(v);
        }
    }
    if (taps->isEmpty() || taps->size() > 2048) { *err = "Filtre : 1 a 2048 coefficients attendus (CSV de nombres)"; return false; }
    return true;
}

bool MliGenerator::generate(const AmplifierConfig& cfg, double maxDuration, MliSequence* out, QString* err)
{
    if (cfg.carrierFreq < 1.0 || cfg.carrierFreq > 10e6) { *err = "Frequence porteuse hors plage (1 Hz - 10 MHz)"; return false; }
    if (cfg.amplitude < 0.0 || cfg.amplitude > 2.0)      { *err = "Amplitude hors plage (0 - 2)"; return false; }
    if (cfg.offset < -1.0 || cfg.offset > 1.0)           { *err = "Offset hors plage (-1 - +1)"; return false; }
    if (cfg.wavFile.trimmed().isEmpty())                 { *err = "Aucun fichier WAV charge pour cette voie"; return false; }

    QVector<double> sig; double fsSig = 0;
    if (!readWav(cfg.wavFile, &sig, &fsSig, err)) return false;
    double peak = 0; for (double v : sig) peak = std::max(peak, std::abs(v));
    if (peak > 0) for (double& v : sig) v /= peak;                  // normalise a +/-1

    if (!cfg.filterFile.trimmed().isEmpty()) {                      // filtre FIR optionnel
        QVector<double> taps;
        if (!readFilter(cfg.filterFile, &taps, err)) return false;
        QVector<double> y(sig.size(), 0.0);
        for (int n=0;n<sig.size();n++) { double acc=0;
            for (int k=0;k<taps.size() && k<=n;k++) acc += taps[k]*sig[n-k];
            y[n]=acc; }
        double p2=0; for (double v : y) p2=std::max(p2,std::abs(v));
        if (p2>0) for (int i=0;i<y.size();i++) sig[i]=y[i]/p2;
    }

    const double clk = cfg.carrierFreq * STEPS_PER_CARRIER;
    const double dur = std::min(maxDuration, sig.size()/fsSig);
    const qint64 N = qint64(dur * clk);
    if (N < STEPS_PER_CARRIER) { *err = "Signal trop court"; return false; }
    if (N > 400000000LL)      { *err = "Sequence trop longue pour la memoire"; return false; }

    out->ampId = cfg.id; out->clockHz = clk;
    out->bits.resize(int(N));
    const bool tri = (cfg.carrierType == "Triangle");
    for (qint64 i=0;i<N;i++) {
        const double ph = double(i % STEPS_PER_CARRIER) / STEPS_PER_CARRIER;            // 0..1
        double car = tri ? (ph < 0.5 ? 4.0*ph - 1.0 : 3.0 - 4.0*ph) : (2.0*ph - 1.0);  // -1..1
        car = car * cfg.amplitude + cfg.offset;
        const double t = double(i) / clk;
        const int si = qMin(sig.size()-1, int(t * fsSig));
        out->bits[int(i)] = (sig[si] > car) ? 1 : 0;
    }
    const int nw = int((N + 31) / 32);
    out->words.fill(0, nw);
    for (qint64 i=0;i<N;i++) if (out->bits[int(i)]) out->words[int(i/32)] |= (1u << (i % 32));
    return true;
}
