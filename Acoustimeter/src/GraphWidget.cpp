#include "GraphWidget.h"
#include "StyleManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QtMath>
#include <complex>
#include <algorithm>
#include <cmath>

static const QColor HYDRO_PAL[] = {
    QColor("#00d4ff"), QColor("#00e676"), QColor("#ffaa00"), QColor("#ff6b9d"),
    QColor("#7c9cff"), QColor("#ffd54f"), QColor("#4dd0e1"), QColor("#ff8a65")};
static const QColor AMP_PAL[] = {
    QColor("#c792ea"), QColor("#89ddff"), QColor("#a5d6a7"), QColor("#ffcc80"),
    QColor("#f48fb1"), QColor("#80cbc4"), QColor("#b39ddb"), QColor("#e6ee9c")};

QColor GraphWidget::colorFor(const QString& key)
{
    const int n = key.mid(1).toInt();
    const int i = qMax(0, n - 1) % 8;
    return key.startsWith('a') ? AMP_PAL[i] : HYDRO_PAL[i];
}

GraphWidget::GraphWidget(GraphType type, QWidget* parent) : QWidget(parent), m_type(type)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GraphWidget::onTimer);
    m_timer->start(50);
    setMinimumSize(200, 80);
}

bool GraphWidget::isDark() const { return StyleManager::instance().isDark(); }
void GraphWidget::setVoieActive(const QString& key, bool a) { m_voies[key] = a; update(); }
void GraphWidget::setSaturated(const QString& key, bool s) { m_chan[key].sat = s; }

void GraphWidget::clearData()
{
    m_chan.clear(); m_spectro.clear(); m_specRef.clear(); update();
}

void GraphWidget::pushSamples(const QString& key, const QVector<double>& v, double fs)
{
    if (m_type == MiniPlot || v.isEmpty()) return;
    Chan& c = m_chan[key];
    for (double x : v) { c.ring[c.head] = x; c.head = (c.head + 1) % RING; }
    c.count = qMin(RING, c.count + int(v.size()));
    c.fs = fs; c.lastMs = QDateTime::currentMSecsSinceEpoch(); c.dirty = true;
}

bool GraphWidget::hasFresh(const Chan& c) const
{
    return c.count > 0 && (QDateTime::currentMSecsSinceEpoch() - c.lastMs) < 2500;
}

std::vector<double> GraphWidget::latest(const Chan& c, int n) const
{
    n = qMin(n, c.count);
    std::vector<double> out(n);
    for (int i = 0; i < n; ++i) out[i] = c.ring[(c.head - n + i + RING * 2) % RING];
    return out;
}

QStringList GraphWidget::activeKeys() const
{
    QStringList k;
    for (auto it = m_chan.begin(); it != m_chan.end(); ++it)
        if (m_voies.value(it.key(), true) && hasFresh(it.value())) k << it.key();
    std::sort(k.begin(), k.end(), [](const QString& a, const QString& b){ return a.mid(1).toInt() < b.mid(1).toInt(); });
    return k;
}

static void fftInPlace(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit; if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * M_PI / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
}

void GraphWidget::analyse(Chan& c)
{
    int n = 4096;
    while (n > c.count && n > 1024) n >>= 1;
    if (c.count < 1024) return;
    const std::vector<double> x = latest(c, n);
    if (m_type == Levels) {
        double s = 0; for (double v : x) s += v * v;
        c.rmsDb = 20.0 * std::log10(std::max(std::sqrt(s / n), 1e-12));
        return;
    }
    std::vector<std::complex<double>> a(n); double wsum = 0;
    for (int i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));    // Hann
        a[i] = x[i] * w; wsum += w;
    }
    fftInPlace(a);
    c.spec.assign(n / 2, DB_FLOOR);
    for (int k = 1; k < n / 2; ++k)
        c.spec[k] = std::max(DB_FLOOR, 20.0 * std::log10(std::max(2.0 * std::abs(a[k]) / wsum, 1e-12)));
}

void GraphWidget::onTimer()
{
    if (m_type == TimeDomain || m_type == MiniPlot) { update(); return; }
    bool any = false;
    for (auto it = m_chan.begin(); it != m_chan.end(); ++it)
        if (it.value().dirty) { analyse(it.value()); it.value().dirty = false; any = true; }
    if (m_type == Spectrogram && any) {
        const QStringList k = activeKeys();
        if (!k.isEmpty()) {
            if (m_specRef.isEmpty() || !k.contains(m_specRef)) { m_specRef = k.first(); m_spectro.clear(); }
            const Chan& c = m_chan[m_specRef];
            if (!c.spec.empty()) {
                const int rows = 256; std::vector<float> col(rows);
                for (int r = 0; r < rows; ++r) {
                    const int a = r * int(c.spec.size()) / rows, b = qMax(a + 1, (r + 1) * int(c.spec.size()) / rows);
                    double m = DB_FLOOR; for (int i = a; i < b; ++i) m = std::max(m, c.spec[i]);
                    col[r] = float(m);
                }
                m_spectro.push_back(col); m_specFs = c.fs;
                while (m_spectro.size() > 400) m_spectro.pop_front();
            }
        }
    }
    update();
}

static QColor thermal(double v)
{
    v = qBound(0.0, v, 1.0);
    if      (v < 0.20) return QColor::fromHsvF(0.67, 1.0, v * 5.0);
    else if (v < 0.40) return QColor::fromHsvF(0.55, 1.0, 1.0);
    else if (v < 0.60) return QColor::fromHsvF(0.38, 1.0, 1.0);
    else if (v < 0.80) return QColor::fromHsvF(0.17, 1.0, 1.0);
    return QColor::fromHsvF(0.0, 1.0, 1.0);
}

void GraphWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, m_type != Levels && m_type != Spectrogram);
    p.fillRect(rect(), isDark() ? QColor("#0d1117") : QColor("#f8fafc"));
    switch (m_type) {
        case TimeDomain:  drawTimeDomain(p);  break;
        case FFT:         drawFFT(p);         break;
        case Levels:      drawLevels(p);      break;
        case Spectrogram: drawSpectrogram(p); break;
        case MiniPlot:    drawMiniPlot(p);    break;
    }
}

void GraphWidget::drawPlaceholder(QPainter& p, const QString& msg)
{
    p.setPen(isDark() ? QColor("#6e7681") : QColor("#718096"));
    p.setFont(QFont("Courier New", 10));
    p.drawText(rect(), Qt::AlignCenter, msg);
}

void GraphWidget::drawGrid(QPainter& p, int cols, int rows, int oL, int oR, int oT, int oB)
{
    p.setPen(QPen(isDark() ? QColor("#1a2030") : QColor("#e2e8f0"), 0.5));
    const int W = width() - oL - oR, H = height() - oT - oB;
    for (int i = 1; i < rows; i++) p.drawLine(oL, oT + H * i / rows, oL + W, oT + H * i / rows);
    for (int i = 1; i < cols; i++) p.drawLine(oL + W * i / cols, oT, oL + W * i / cols, oT + H);
}

static QString fmtFreq(double hz) { return hz >= 1000 ? QString::number(hz / 1000.0, 'f', hz >= 10000 ? 0 : 1) + "k" : QString::number(hz, 'f', 0); }

void GraphWidget::drawTimeDomain(QPainter& p)
{
    const QStringList keys = activeKeys();
    if (keys.isEmpty()) { drawPlaceholder(p, "En attente de donnees hydrophones (NI)"); return; }
    const int OL = 44, OR = 6, OT = 8, OB = 18;
    drawGrid(p, 8, 4, OL, OR, OT, OB);
    const int W = width() - OL - OR, H = height() - OT - OB;
    const double slotH = double(H) / keys.size();
    const int N = 1024;
    double fs0 = m_chan[keys.first()].fs;

    p.setPen(isDark() ? QColor("#6e7681") : QColor("#718096")); p.setFont(QFont("Courier New", 8));
    for (int i = 0; i <= 4; ++i)
        p.drawText(OL + W * i / 4 - 12, height() - 2, QString::number(1e3 * N / fs0 * i / 4.0, 'f', 2) + "ms");

    for (int idx = 0; idx < keys.size(); ++idx) {
        const Chan& c = m_chan[keys[idx]];
        const std::vector<double> x = latest(c, N);
        if (x.size() < 8) continue;
        double pk = 1e-9; for (double v : x) pk = std::max(pk, std::abs(v));
        const double yC = OT + slotH * (idx + 0.5), amp = slotH * 0.42;
        const QColor col = c.sat ? QColor("#ff4444") : colorFor(keys[idx]);
        p.setPen(QPen(col.darker(200), 0.5, Qt::DotLine)); p.drawLine(OL, int(yC), OL + W, int(yC));
        p.setPen(QPen(col, 1.4));
        QPainterPath path;
        for (size_t i = 0; i < x.size(); ++i) {
            const double px = OL + double(i) / (x.size() - 1) * W, py = yC - x[i] / pk * amp;
            i == 0 ? path.moveTo(px, py) : path.lineTo(px, py);
        }
        p.drawPath(path);
        p.setPen(col); p.setFont(QFont("Courier New", 9, QFont::Bold));
        p.drawText(OL + 4, int(yC - amp - 1), keys[idx].toUpper() + (c.sat ? "  SATURATION" : ""));
        p.setFont(QFont("Courier New", 8));
        p.drawText(2, int(yC + 4), QString("±%1V").arg(pk, 0, 'g', 3));
    }
}

void GraphWidget::drawFFT(QPainter& p)
{
    const QStringList keys = activeKeys();
    bool have = false; for (const auto& k : keys) if (!m_chan[k].spec.empty()) have = true;
    if (!have) { drawPlaceholder(p, "En attente de donnees hydrophones (NI)"); return; }
    const int OL = 44, OR = 6, OT = 6, OB = 18;
    drawGrid(p, 8, 7, OL, OR, OT, OB);
    const int W = width() - OL - OR, H = height() - OT - OB;
    p.setPen(isDark() ? QColor("#6e7681") : QColor("#718096")); p.setFont(QFont("Courier New", 8));
    for (int i = 0; i <= 7; ++i) p.drawText(2, OT + H * i / 7 + 4, QString::number(-20 * i));
    const double nyq = m_chan[keys.first()].fs / 2.0;
    for (int i = 0; i <= 8; ++i) p.drawText(OL + W * i / 8 - 10, height() - 2, fmtFreq(nyq * i / 8.0));
    p.setFont(QFont("Courier New", 9, QFont::Bold));
    int lx = OL + 6;
    for (const auto& k : keys) {
        const Chan& c = m_chan[k]; if (c.spec.empty()) continue;
        const QColor col = colorFor(k);
        p.setPen(QPen(col, 1.2)); QPainterPath path;
        const int nb = int(c.spec.size());
        for (int i = 0; i < W; ++i) {
            const int a = i * nb / W, b = qMax(a + 1, (i + 1) * nb / W);
            double m = DB_FLOOR; for (int j = a; j < b; ++j) m = std::max(m, c.spec[j]);
            const double y = OT + H * (1.0 - (m - DB_FLOOR) / (0.0 - DB_FLOOR));
            i == 0 ? path.moveTo(OL + i, y) : path.lineTo(OL + i, y);
        }
        p.drawPath(path);
        p.setPen(col); p.drawText(lx, OT + 12, k.toUpper()); lx += 36;
    }
    p.setPen(isDark() ? QColor("#6e7681") : QColor("#718096")); p.setFont(QFont("Courier New", 8));
    p.drawText(OL + W - 40, OT + 12, "dBV");
}

void GraphWidget::drawLevels(QPainter& p)
{
    const QStringList keys = activeKeys();
    if (keys.isEmpty()) { drawPlaceholder(p, "En attente de donnees hydrophones (NI)"); return; }
    const int OL = 44, OR = 8, OT = 20, OB = 22;
    const int W = width() - OL - OR, H = height() - OT - OB;
    if (W <= 0 || H <= 0) return;
    const QColor bg = isDark() ? QColor("#1a2030") : QColor("#e2e8f0");
    p.setPen(QPen(bg, 0.5)); for (int i = 0; i <= 6; ++i) p.drawLine(OL, OT + H * i / 6, OL + W, OT + H * i / 6);
    p.setPen(isDark() ? QColor("#6e7681") : QColor("#718096")); p.setFont(QFont("Courier New", 8));
    for (int i = 0; i <= 6; ++i) p.drawText(2, OT + H * i / 6 + 4, QString::number(-20 * i));
    const int n = keys.size(), gap = 8;
    const int barW = qMin(60, qMax(10, (W - gap * (n + 1)) / n));
    const int x0 = OL + (W - (barW * n + gap * (n + 1))) / 2;
    for (int i = 0; i < n; ++i) {
        const Chan& c = m_chan[keys[i]];
        const int x = x0 + gap + i * (barW + gap);
        const double lv = qBound(0.0, (c.rmsDb - (-120.0)) / 120.0, 1.0);
        const int bh = int(lv * H);
        p.fillRect(x, OT, barW, H, bg);
        const QColor col = c.sat ? QColor("#ff4444") : colorFor(keys[i]);
        QLinearGradient g(0, OT + H - bh, 0, OT + H); g.setColorAt(0, col); g.setColorAt(1, col.darker(180));
        p.fillRect(x, OT + H - bh, barW, bh, g);
        p.setPen(col); p.setFont(QFont("Courier New", 9, QFont::Bold));
        p.drawText(QRect(x - 6, OT + H - bh - 16, barW + 12, 14), Qt::AlignCenter, QString::number(c.rmsDb, 'f', 1));
        p.drawText(QRect(x - 6, OT + H + 4, barW + 12, 14), Qt::AlignCenter, keys[i].toUpper() + (c.sat ? "!" : ""));
    }
    p.setPen(isDark() ? QColor("#6e7681") : QColor("#718096")); p.setFont(QFont("Courier New", 8));
    p.drawText(OL, OT - 6, "RMS dBV (apres gain preampli) — rouge = saturation > 95 %");
}

void GraphWidget::drawSpectrogram(QPainter& p)
{
    if (m_spectro.empty()) { drawPlaceholder(p, "En attente de donnees hydrophones (NI)"); return; }
    const int OL = 44, OR = 22, OT = 6, OB = 18;
    const int W = width() - OL - OR, H = height() - OT - OB;
    if (W <= 0 || H <= 0) return;
    const int cols = int(m_spectro.size()), rows = int(m_spectro.front().size());
    for (int x = 0; x < W; ++x) {
        const auto& col = m_spectro[size_t(x) * cols / W];
        for (int y = 0; y < H; ++y)
            p.fillRect(OL + x, OT + H - 1 - y, 1, 1, thermal((col[y * rows / H] + 120.0) / 100.0));
    }
    p.setPen(isDark() ? QColor("#8b949e") : QColor("#4a5568")); p.setFont(QFont("Courier New", 8));
    for (int i = 0; i <= 4; ++i) p.drawText(2, OT + H - H * i / 4 + (i == 0 ? -2 : 4), fmtFreq(m_specFs / 2.0 * i / 4.0));
    p.drawText(OL, height() - 2, QString("%1 — ref. %2").arg("-t").arg(m_specRef.toUpper()));
    p.drawText(OL + W - 8, height() - 2, "0s");
    const int bx = OL + W + 4;
    for (int y = 0; y < H; ++y) p.fillRect(bx, OT + y, 14, 1, thermal(1.0 - double(y) / H));
    p.setPen(QPen(QColor("#444c56"), 1)); p.drawRect(bx, OT, 14, H);
}

void GraphWidget::pushMiniValue(double v)
{
    m_miniData.push_back(v);
    while (int(m_miniData.size()) > m_miniMaxPts) m_miniData.pop_front();
}

void GraphWidget::drawMiniPlot(QPainter& p)
{
    const int OL = 40, OR = 4, OT = 18, OB = 16;
    const int W = width() - OL - OR, H = height() - OT - OB;
    if (W <= 0 || H <= 0) return;
    p.setPen(isDark() ? QColor("#6e7681") : QColor("#a0aec0")); p.setFont(QFont("Courier New", 8));
    p.drawText(OL, height() - 2, m_miniLabel);
    if (m_miniData.empty()) {
        p.setFont(QFont("Courier New", 12, QFont::Bold));
        p.drawText(QRect(0, 2, OL - 3, OT + 2), Qt::AlignRight | Qt::AlignVCenter, QString::fromUtf8("\xE2\x80\x94"));
        p.setFont(QFont("Courier New", 8)); p.drawText(OL + 4, OT + H / 2, "pas de mesure");
        return;
    }
    double lo = *std::min_element(m_miniData.begin(), m_miniData.end());
    double hi = *std::max_element(m_miniData.begin(), m_miniData.end());
    if (hi - lo < 1.0) { const double m = (hi + lo) / 2; lo = m - 0.5; hi = m + 0.5; }
    const double pad = (hi - lo) * 0.1; lo -= pad; hi += pad;
    p.setPen(QPen(isDark() ? QColor("#1a2030") : QColor("#e2e8f0"), 0.5));
    for (int i = 1; i < 4; i++) p.drawLine(OL, OT + H * i / 4, OL + W, OT + H * i / 4);
    const double cur = m_miniData.back();
    p.setPen(m_miniColor); p.setFont(QFont("Courier New", 12, QFont::Bold));
    p.drawText(QRect(0, 2, OL - 3, OT + 2), Qt::AlignRight | Qt::AlignVCenter, QString::number(cur, 'f', 1));
    p.setPen(isDark() ? QColor("#8b949e") : QColor("#718096")); p.setFont(QFont("Courier New", 9));
    p.drawText(2, OT - 3, m_miniUnit);
    p.setFont(QFont("Courier New", 7));
    p.drawText(2, OT + 7, QString::number(hi, 'f', 1)); p.drawText(2, OT + H - 1, QString::number(lo, 'f', 1));
    p.setPen(QPen(m_miniColor, 1.5));
    QPainterPath path; const int n = int(m_miniData.size());
    for (int i = 0; i < n; ++i) {
        const double nx = OL + double(i) / (m_miniMaxPts - 1) * W;
        const double ny = qBound(double(OT), OT + H - (m_miniData[i] - lo) / (hi - lo) * H, double(OT + H));
        i == 0 ? path.moveTo(nx, ny) : path.lineTo(nx, ny);
    }
    p.drawPath(path);
    const double cx = OL + double(n - 1) / (m_miniMaxPts - 1) * W;
    const double cy = qBound(double(OT), OT + H - (cur - lo) / (hi - lo) * H, double(OT + H));
    p.setPen(Qt::NoPen); p.setBrush(m_miniColor); p.drawEllipse(QPointF(cx, cy), 3.5, 3.5);
}
