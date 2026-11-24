#include "fenetre_graphes.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
const QColor kCouleurs[] = {QColor(0x1f, 0x77, 0xb4), QColor(0xff, 0x7f, 0x0e), QColor(0x2c, 0xa0, 0x2c), QColor(0xd6, 0x27, 0x28),
                            QColor(0x94, 0x67, 0xbd), QColor(0x8c, 0x56, 0x4b), QColor(0xe3, 0x77, 0xc2), QColor(0x7f, 0x7f, 0x7f)};
constexpr int kEntete = 26, kPied = 20;
constexpr double kPi = 3.14159265358979323846;

std::vector<double> graduations(double a, double b, int n = 6)
{
    std::vector<double> t;
    if (!(b > a) || !std::isfinite(a) || !std::isfinite(b)) return t;
    const double brut = (b - a) / n, p = std::pow(10.0, std::floor(std::log10(brut))), r = brut / p;
    const double pas = (r < 1.5 ? 1 : r < 3 ? 2 : r < 7 ? 5 : 10) * p;
    for (double v = std::ceil(a / pas) * pas; v <= b + pas * 1e-9; v += pas) t.push_back(std::abs(v) < pas * 1e-9 ? 0.0 : v);
    return t;
}

QString texteNombre(double v)
{
    const double a = std::abs(v);
    if (a != 0 && (a < 1e-3 || a >= 1e6)) return QString::number(v, 'g', 3);
    return QString::number(v, 'g', 7);
}
}  // namespace

FenetreGraphes::FenetreGraphes(const QString& titre, int colonnes, bool xLies, QWidget* parent)
    : QWidget(parent), m_titre(titre), m_cols(std::max(1, colonnes)), m_lies(xLies)
{
    setWindowTitle(titre);
    setMouseTracking(true);
    setMinimumSize(600, 300);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);
}

Panneau& FenetreGraphes::ajoute(const QString& titre, const QString& axeX)
{
    m_p.emplace_back();
    m_p.back().titre = titre;
    m_p.back().axeX = axeX;
    return m_p.back();
}

QRect FenetreGraphes::cadre(int k) const
{
    const int n = int(m_p.size()), nc = std::min(m_cols, std::max(1, n)), nl = (n + nc - 1) / nc;
    const int w = width() / nc, h = (height() - kEntete - kPied) / std::max(1, nl);
    return QRect((k % nc) * w, kEntete + (k / nc) * h, w, h);
}

QRect FenetreGraphes::zoneTrace(int k) const
{
    const QRect c = cadre(k);
    if (m_p[size_t(k)].polaire) {
        const int s = std::min(c.width() - 80, c.height() - 80);
        return QRect(c.center().x() - s / 2, c.top() + 46 + (c.height() - 80 - s) / 2, s, s);
    }
    return c.adjusted(m_p[size_t(k)].etiquettesY.empty() ? 66 : 150, 24, -30, -38);
}

int FenetreGraphes::panneauSous(QPoint p) const
{
    for (int k = 0; k < int(m_p.size()); ++k)
        if (cadre(k).contains(p)) return k;
    return -1;
}

void FenetreGraphes::etendueX(const Panneau& p, double& x0, double& x1) const
{
    x0 = std::numeric_limits<double>::infinity();
    x1 = -x0;
    for (const Serie& s : p.series) {
        if (!s.taille()) continue;
        if (s.x.empty()) { x0 = std::min(x0, s.x0); x1 = std::max(x1, s.x0 + double(s.taille() - 1) * s.dx); continue; }
        for (double v : s.x)
            if (std::isfinite(v) && (!p.logX || v > 0)) { x0 = std::min(x0, v); x1 = std::max(x1, v); }
    }
    bool marqueAuBord = false;                         // les evenements (ex. fin) restent visibles
    for (const Marque& m : p.marques)
        if (std::isfinite(x0) && std::isfinite(m.x) && (!p.logX || m.x > 0)) {
            if (m.x >= x1) marqueAuBord = true;
            x0 = std::min(x0, m.x); x1 = std::max(x1, m.x);
        }
    if (marqueAuBord && x1 > x0 && !p.logX) x1 += (x1 - x0) * 0.04;
    if (!std::isfinite(x0)) { x0 = 0; x1 = 1; }
    if (x1 <= x0) { const double d = x0 ? std::abs(x0) * 0.05 : 0.5; x0 -= d; x1 += d; }
}

void FenetreGraphes::vueComplete()
{
    double u0 = std::numeric_limits<double>::infinity(), u1 = -u0;
    for (Panneau& p : m_p) {
        if (p.polaire) continue;
        etendueX(p, p.vx0, p.vx1);
        u0 = std::min(u0, p.vx0); u1 = std::max(u1, p.vx1);
        p.vueInit = true;
    }
    if (m_lies && std::isfinite(u0))
        for (Panneau& p : m_p) if (!p.polaire) { p.vx0 = u0; p.vx1 = u1; }
    update();
}

void FenetreGraphes::limiteX(double x0, double x1)
{
    vueComplete();
    for (Panneau& p : m_p) {
        if (p.polaire) continue;
        if (std::isfinite(x0)) p.vx0 = x0;
        if (std::isfinite(x1)) p.vx1 = x1;
    }
    update();
}

void FenetreGraphes::appliqueVue(int k, double x0, double x1)
{
    if (!(x1 > x0)) return;
    for (int j = 0; j < int(m_p.size()); ++j) {
        Panneau& p = m_p[size_t(j)];
        if (p.polaire || (j != k && !m_lies)) continue;
        p.vx0 = x0; p.vx1 = x1;
    }
    update();
}

double FenetreGraphes::versPixel(const Panneau& p, const QRect& r, double x) const
{
    if (p.logX) {
        const double a = std::log10(std::max(p.vx0, 1e-300)), b = std::log10(std::max(p.vx1, 1e-300));
        return r.left() + (std::log10(std::max(x, 1e-300)) - a) / (b - a) * r.width();
    }
    return r.left() + (x - p.vx0) / (p.vx1 - p.vx0) * r.width();
}

double FenetreGraphes::depuisPixel(const Panneau& p, const QRect& r, double px) const
{
    const double f = (px - r.left()) / double(r.width());
    if (p.logX) {
        const double a = std::log10(std::max(p.vx0, 1e-300)), b = std::log10(std::max(p.vx1, 1e-300));
        return std::pow(10.0, a + f * (b - a));
    }
    return p.vx0 + f * (p.vx1 - p.vx0);
}

static std::pair<size_t, size_t> visibles(const Serie& s, double x0, double x1)
{
    const size_t n = s.taille();
    if (!n) return {0, 0};
    size_t i0, i1;
    if (s.x.empty()) {
        if (s.dx <= 0) return {0, n};
        i0 = size_t(std::clamp(std::floor((x0 - s.x0) / s.dx) - 1, 0.0, double(n)));
        i1 = size_t(std::clamp(std::ceil((x1 - s.x0) / s.dx) + 2, 0.0, double(n)));
    } else {
        i0 = size_t(std::lower_bound(s.x.begin(), s.x.end(), x0) - s.x.begin());
        i1 = size_t(std::upper_bound(s.x.begin(), s.x.end(), x1) - s.x.begin());
        i0 = i0 ? i0 - 1 : 0;
        i1 = std::min(n, i1 + 1);
    }
    return {i0, std::max(i0, i1)};
}

void FenetreGraphes::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing, true);
    g.fillRect(rect(), Qt::white);
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() + 1);
    g.setFont(f);
    g.setPen(Qt::black);
    g.drawText(QRect(0, 0, width(), kEntete), Qt::AlignCenter, m_titre);
    g.setFont(font());
    for (auto& p : m_p)
        if (!p.vueInit) { vueComplete(); break; }
    m_yVue.assign(m_p.size(), {0, 1});
    for (int k = 0; k < int(m_p.size()); ++k) {
        if (m_p[size_t(k)].polaire) dessinePolaire(g, k);
        else dessine(g, k);
    }
    // pied : aide + coordonnees sous la souris
    QString pied = QStringLiteral("molette : zoom   ·   glisser : déplacer   ·   double-clic : vue complète");
    const int k = panneauSous(m_souris);
    if (k >= 0 && zoneTrace(k).contains(m_souris)) {
        const Panneau& p = m_p[size_t(k)];
        const QRect r = zoneTrace(k);
        if (p.polaire) {
            const QPointF d = QPointF(m_souris) - QPointF(r.center());
            double ang = std::atan2(d.x(), -d.y()) * 180 / kPi;
            if (ang < 0) ang += 360;
            const double db = std::hypot(d.x(), d.y()) / (r.width() / 2.0) * 40 - 40;
            pied = QStringLiteral("%1 :  angle = %2°   niveau = %3 dB").arg(p.titre).arg(ang, 0, 'f', 1).arg(db, 0, 'f', 1);
        } else {
            const auto [y0, y1] = m_yVue[size_t(k)];
            const double x = depuisPixel(p, r, m_souris.x()), y = y1 - (m_souris.y() - r.top()) / double(r.height()) * (y1 - y0);
            pied = QStringLiteral("%1 :  x = %2   y = %3").arg(p.titre, texteNombre(x), texteNombre(y));
        }
    }
    g.setPen(QColor(90, 90, 90));
    g.drawText(QRect(8, height() - kPied, width() - 16, kPied), Qt::AlignVCenter | Qt::AlignLeft, pied);
}

void FenetreGraphes::dessine(QPainter& g, int k)
{
    Panneau& p = m_p[size_t(k)];
    const QRect c = cadre(k), r = zoneTrace(k);
    if (r.width() < 20 || r.height() < 20) return;

    // ---- etendue verticale sur la partie visible
    double y0 = p.yMin, y1 = p.yMax;
    if (!p.yFixe) {
        y0 = std::numeric_limits<double>::infinity(); y1 = -y0;
        for (const Serie& s : p.series) {
            const auto [i0, i1] = visibles(s, p.vx0, p.vx1);
            for (size_t i = i0; i < i1; ++i) {
                const double v = s.yi(i);
                if (std::isfinite(v)) { y0 = std::min(y0, v); y1 = std::max(y1, v); }
            }
        }
        if (!std::isfinite(y0)) { y0 = 0; y1 = 1; }
        const double d = y1 > y0 ? (y1 - y0) * 0.06 : (y0 ? std::abs(y0) * 0.05 : 0.5);
        y0 -= d; y1 += d;
    }
    m_yVue[size_t(k)] = {y0, y1};
    auto py = [&](double v) { return r.bottom() - (v - y0) / (y1 - y0) * r.height(); };

    // ---- titre, grille, graduations
    g.setPen(Qt::black);
    QFont ft = font(); ft.setBold(true); g.setFont(ft);
    g.drawText(QRect(c.left(), c.top() + 2, c.width(), 20), Qt::AlignCenter, p.titre);
    g.setFont(font());
    const QFontMetrics fm(font());
    QPen grille(QColor(225, 225, 225)); grille.setWidthF(1);
    if (p.etiquettesY.empty()) {
        for (double v : graduations(y0, y1)) {
            const double y = py(v);
            g.setPen(grille); g.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
            g.setPen(QColor(60, 60, 60));
            g.drawText(QRectF(c.left(), y - 8, r.left() - c.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter, texteNombre(v));
        }
    } else {
        for (const auto& [v, t] : p.etiquettesY) {
            g.setPen(QColor(60, 60, 60));
            g.drawText(QRectF(c.left(), py(v) - 8, r.left() - c.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter, fm.elidedText(t, Qt::ElideMiddle, r.left() - c.left() - 8));
        }
    }
    std::vector<double> gx;
    if (p.logX && p.vx0 > 0) {
        for (double d = std::pow(10.0, std::floor(std::log10(p.vx0))); d <= p.vx1; d *= 10)
            for (double m : {1.0, 2.0, 5.0})
                if (d * m >= p.vx0 && d * m <= p.vx1) gx.push_back(d * m);
        if (gx.size() < 3) gx = graduations(p.vx0, p.vx1);
    } else {
        gx = graduations(p.vx0, p.vx1, std::max(3, r.width() / 90));
    }
    for (double v : gx) {
        const double x = versPixel(p, r, v);
        g.setPen(grille); g.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        g.setPen(QColor(60, 60, 60));
        g.drawText(QRectF(x - 45, r.bottom() + 3, 90, 16), Qt::AlignHCenter | Qt::AlignTop, texteNombre(v));
    }
    g.drawText(QRect(r.left(), r.bottom() + 19, r.width(), 16), Qt::AlignHCenter | Qt::AlignTop, p.axeX);
    g.setPen(QColor(120, 120, 120));
    g.drawRect(r);

    // ---- courbes
    g.save();
    g.setClipRect(r.adjusted(0, -1, 1, 1));
    for (size_t si = 0; si < p.series.size(); ++si) {
        const Serie& s = p.series[si];
        const auto [i0, i1] = visibles(s, p.vx0, p.vx1);
        if (i1 <= i0) continue;
        QPen pen(kCouleurs[si % 8]); pen.setWidthF(1.2);
        g.setPen(pen);
        const size_t n = i1 - i0;
        if (n > size_t(3 * r.width())) {
            // beaucoup de points : min et max par colonne de pixels (les pics restent visibles)
            QPolygonF l;
            int col = std::numeric_limits<int>::min();
            double mn = 0, mx = 0;
            for (size_t i = i0; i < i1; ++i) {
                const double v = s.yi(i);
                if (!std::isfinite(v)) continue;
                const int px = int(std::floor(versPixel(p, r, s.xi(i))));
                if (px != col) {
                    if (col != std::numeric_limits<int>::min()) { l << QPointF(col, py(mn)) << QPointF(col, py(mx)); }
                    col = px; mn = mx = v;
                } else { mn = std::min(mn, v); mx = std::max(mx, v); }
            }
            if (col != std::numeric_limits<int>::min()) l << QPointF(col, py(mn)) << QPointF(col, py(mx));
            g.setRenderHint(QPainter::Antialiasing, false);
            g.drawPolyline(l);
            g.setRenderHint(QPainter::Antialiasing, true);
        } else {
            QPolygonF l;
            auto vide = [&]() { if (l.size() > 1) g.drawPolyline(l); else if (l.size() == 1) g.drawPoint(l[0]); l.clear(); };
            for (size_t i = i0; i < i1; ++i) {
                const double v = s.yi(i);
                if (!std::isfinite(v)) { vide(); continue; }
                const QPointF q(versPixel(p, r, s.xi(i)), py(v));
                if (s.style == Serie::Style::Escalier && !l.isEmpty()) l << QPointF(q.x(), l.back().y());
                l << q;
            }
            if (s.style == Serie::Style::Escalier && !l.isEmpty() && s.x.empty() && s.dx > 0)
                l << QPointF(versPixel(p, r, s.xi(i1 - 1) + s.dx), l.back().y());
            const QPolygonF tous = l;
            vide();
            if (s.style == Serie::Style::LignePoints && tous.size() < r.width() / 4) {
                g.setBrush(pen.color());
                for (const QPointF& q : tous) g.drawEllipse(q, 2.0, 2.0);
                g.setBrush(Qt::NoBrush);
            }
        }
    }
    // ---- evenements
    QPen pm(QColor(150, 150, 150)); pm.setStyle(Qt::DashLine);
    for (const Marque& m : p.marques) {
        if (m.x < p.vx0 || m.x > p.vx1) continue;
        const double x = versPixel(p, r, m.x);
        g.setPen(pm); g.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        g.setPen(QColor(100, 100, 100));
        g.drawText(QPointF(x + 3, r.top() + 12), m.texte);
    }
    g.restore();

    // ---- legende
    if (p.series.size() > 1 && p.etiquettesY.empty()) {
        int lw = 0;
        for (const Serie& s : p.series) lw = std::max(lw, fm.horizontalAdvance(s.nom));
        const int lh = fm.height(), n = int(p.series.size());
        QRect lg(r.right() - lw - 34, r.top() + 6, lw + 28, n * lh + 8);
        g.setPen(QColor(190, 190, 190)); g.setBrush(QColor(255, 255, 255, 225)); g.drawRect(lg); g.setBrush(Qt::NoBrush);
        for (int i = 0; i < n; ++i) {
            const int y = lg.top() + 4 + i * lh + lh / 2;
            QPen pl(kCouleurs[i % 8]); pl.setWidthF(2); g.setPen(pl);
            g.drawLine(lg.left() + 5, y, lg.left() + 19, y);
            g.setPen(Qt::black);
            g.drawText(QPointF(lg.left() + 23, y + fm.ascent() / 2.0 - 1), p.series[size_t(i)].nom);
        }
    }
}

void FenetreGraphes::dessinePolaire(QPainter& g, int k)
{
    const Panneau& p = m_p[size_t(k)];
    const QRect c = cadre(k), r = zoneTrace(k);
    const QPointF o = QRectF(r).center();
    const double R = r.width() / 2.0;
    QFont ft = font(); ft.setBold(true); g.setFont(ft);
    g.setPen(Qt::black);
    g.drawText(QRect(c.left(), c.top() + 2, c.width(), 20), Qt::AlignCenter, p.titre);
    g.setFont(font());
    for (int db = 0; db >= -30; db -= 10) {               // cercles : 0, -10, -20, -30 dB
        const double rr = (db + 40) / 40.0 * R;
        g.setPen(QColor(215, 215, 215)); g.drawEllipse(o, rr, rr);
        g.setPen(QColor(110, 110, 110)); g.drawText(QPointF(o.x() + 3, o.y() - rr + 12), QStringLiteral("%1 dB").arg(db));
    }
    for (int a = 0; a < 360; a += 30) {                    // rayons : 0° en haut, sens horaire
        const double t = a * kPi / 180;
        const QPointF e(o.x() + R * std::sin(t), o.y() - R * std::cos(t));
        g.setPen(QColor(215, 215, 215)); g.drawLine(o, e);
        g.setPen(QColor(90, 90, 90));
        g.drawText(QRectF(o.x() + (R + 14) * std::sin(t) - 20, o.y() - (R + 14) * std::cos(t) - 8, 40, 16), Qt::AlignCenter, QStringLiteral("%1°").arg(a));
    }
    for (size_t si = 0; si < p.series.size(); ++si) {
        const Serie& s = p.series[si];
        QPolygonF l;
        for (size_t i = 0; i < s.taille(); ++i) {
            const double v = s.yi(i);
            if (!std::isfinite(v)) continue;
            const double rr = (std::clamp(v, -40.0, 0.0) + 40) / 40 * R, t = s.xi(i) * kPi / 180;
            l << QPointF(o.x() + rr * std::sin(t), o.y() - rr * std::cos(t));
        }
        if (l.size() > 2) l << l.front();
        QPen pen(kCouleurs[si % 8]); pen.setWidthF(1.6); g.setPen(pen);
        g.drawPolyline(l);
    }
}

void FenetreGraphes::wheelEvent(QWheelEvent* e)
{
    const QPoint pos = e->position().toPoint();
    const int k = panneauSous(pos);
    if (k < 0 || m_p[size_t(k)].polaire) return;
    const Panneau& p = m_p[size_t(k)];
    const QRect r = zoneTrace(k);
    const double f = e->angleDelta().y() > 0 ? 0.8 : 1.25, mx = std::clamp(pos.x(), r.left(), r.right());
    appliqueVue(k, depuisPixel(p, r, mx + (r.left() - mx) * f), depuisPixel(p, r, mx + (r.right() - mx) * f));
}

void FenetreGraphes::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    m_glisse = panneauSous(e->pos());
    m_dernier = e->pos();
    if (m_glisse >= 0 && m_p[size_t(m_glisse)].polaire) m_glisse = -1;
    if (m_glisse >= 0) setCursor(Qt::ClosedHandCursor);
}

void FenetreGraphes::mouseMoveEvent(QMouseEvent* e)
{
    m_souris = e->pos();
    if (m_glisse >= 0) {
        const Panneau& p = m_p[size_t(m_glisse)];
        const QRect r = zoneTrace(m_glisse);
        const int d = e->pos().x() - m_dernier.x();
        m_dernier = e->pos();
        appliqueVue(m_glisse, depuisPixel(p, r, r.left() - d), depuisPixel(p, r, r.right() - d));
    }
    update();
}

void FenetreGraphes::mouseReleaseEvent(QMouseEvent*)
{
    m_glisse = -1;
    unsetCursor();
}

void FenetreGraphes::mouseDoubleClickEvent(QMouseEvent*) { vueComplete(); }

void FenetreGraphes::leaveEvent(QEvent*)
{
    m_souris = QPoint(-1, -1);
    update();
}

bool FenetreGraphes::enregistrePng(const QString& chemin, QSize taille)
{
    resize(taille);
    QPixmap pm(taille);
    pm.fill(Qt::white);
    render(&pm);
    return pm.save(chemin);
}
