// Fenetre de courbes simple (QPainter, sans Qt Charts) : plusieurs panneaux, zoom a la molette,
// deplacement a la souris, double-clic = vue complete, lecture des coordonnees sous la souris.
#pragma once
#include <QColor>
#include <QString>
#include <QWidget>
#include <memory>
#include <utility>
#include <vector>

struct Serie {
    enum class Style { Ligne, LignePoints, Escalier };
    QString nom;
    Style style = Style::Ligne;
    // abscisses : soit explicites (x croissants), soit regulieres (x0 + i * dx)
    std::vector<double> x;
    double x0 = 0, dx = 0;
    std::vector<float> y;
    double decalage = 0, echelle = 1;          // y affiche = y * echelle + decalage (trames MLI empilees)
    size_t taille() const { return y.size(); }
    double xi(size_t i) const { return x.empty() ? x0 + double(i) * dx : x[i]; }
    double yi(size_t i) const { return double(y[i]) * echelle + decalage; }
};

struct Marque { double x; QString texte; };   // evenement : trait vertical pointille

struct Panneau {
    QString titre, axeX;
    bool logX = false;
    bool polaire = false;                      // directivite : x = angle (deg), y = dB, de -40 (centre) a 0 (bord)
    bool yFixe = false;
    double yMin = 0, yMax = 1;
    std::vector<std::pair<double, QString>> etiquettesY;   // graduations nommees (trames MLI)
    std::vector<Serie> series;
    std::vector<Marque> marques;
    // vue courante (zoom)
    double vx0 = 0, vx1 = 1;
    bool vueInit = false;
};

class FenetreGraphes : public QWidget {
public:
    explicit FenetreGraphes(const QString& titre, int colonnes = 1, bool xLies = true, QWidget* parent = nullptr);
    Panneau& ajoute(const QString& titre, const QString& axeX);
    void limiteX(double x0, double x1);        // vue initiale imposee sur tous les panneaux non polaires
    void vueComplete();
    bool enregistrePng(const QString& chemin, QSize taille);
    std::vector<Panneau>& panneaux() { return m_p; }

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QRect cadre(int k) const;                  // rectangle du panneau k
    QRect zoneTrace(int k) const;              // zone des courbes du panneau k
    int panneauSous(QPoint p) const;
    void etendueX(const Panneau& p, double& x0, double& x1) const;
    void appliqueVue(int k, double x0, double x1);
    double versPixel(const Panneau& p, const QRect& r, double x) const;
    double depuisPixel(const Panneau& p, const QRect& r, double px) const;
    void dessine(QPainter& g, int k);
    void dessinePolaire(QPainter& g, int k);

    QString m_titre;
    int m_cols;
    bool m_lies;
    std::vector<Panneau> m_p;
    int m_glisse = -1;
    QPoint m_dernier;
    QPoint m_souris{-1, -1};
    std::vector<std::pair<double, double>> m_yVue;     // y affiches (pour la lecture des coordonnees)
};
