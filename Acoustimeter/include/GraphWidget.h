#pragma once
// GraphWidget : affichage de donnees REELLES uniquement (aucun generateur de signal).
// Les donnees arrivent par pushSamples() / pushMiniValue() depuis HardwareManager.
#include <QWidget>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <QColor>
#include <deque>
#include <vector>

class GraphWidget : public QWidget
{
    Q_OBJECT
public:
    enum GraphType { TimeDomain, FFT, Levels, Spectrogram, MiniPlot };
    explicit GraphWidget(GraphType type, QWidget* parent = nullptr);

    static QColor colorFor(const QString& key);          // "h1".."hN", "a1".."aN"
    void setVoieActive(const QString& key, bool active);
    void pushSamples(const QString& key, const QVector<double>& volts, double fs);
    void setSaturated(const QString& key, bool sat);
    void clearData();
    bool isDark() const;

    void setMiniPlotColor(const QColor& c)  { m_miniColor = c; }
    void setMiniPlotUnit(const QString& u)  { m_miniUnit  = u; }
    void setMiniPlotLabel(const QString& l) { m_miniLabel = l; update(); }
    void pushMiniValue(double v);
    void clearMini() { m_miniData.clear(); update(); }

protected:
    void paintEvent(QPaintEvent*) override;
private slots:
    void onTimer();
private:
    struct Chan {
        std::vector<double> ring = std::vector<double>(RING, 0.0);
        int head = 0, count = 0;
        double fs = 0.0;
        qint64 lastMs = 0;
        bool sat = false, dirty = false;
        std::vector<double> spec;      // dBV (crete) par bin, 0..fs/2
        double rmsDb = -200.0;
    };
    static constexpr int RING = 16384;
    static constexpr double DB_FLOOR = -140.0;

    bool hasFresh(const Chan& c) const;
    std::vector<double> latest(const Chan& c, int n) const;
    QStringList activeKeys() const;
    void analyse(Chan& c);
    void drawPlaceholder(QPainter& p, const QString& msg);
    void drawTimeDomain(QPainter& p);
    void drawFFT(QPainter& p);
    void drawLevels(QPainter& p);
    void drawSpectrogram(QPainter& p);
    void drawMiniPlot(QPainter& p);
    void drawGrid(QPainter& p,int cols,int rows,int oL,int oR,int oT,int oB);

    GraphType m_type;
    QTimer* m_timer;
    QMap<QString, bool> m_voies;
    QMap<QString, Chan> m_chan;
    std::deque<std::vector<float>> m_spectro;     // colonnes (dBV) de la voie de reference
    QString m_specRef;
    double  m_specFs = 0.0;

    std::deque<double> m_miniData;
    QColor  m_miniColor = QColor("#00d4ff");
    QString m_miniUnit, m_miniLabel;
    int     m_miniMaxPts = 240;
};
