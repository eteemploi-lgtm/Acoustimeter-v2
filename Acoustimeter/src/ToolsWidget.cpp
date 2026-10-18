#include "ToolsWidget.h"
#include "HydroModels.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>

ToolsWidget::ToolsWidget(QWidget* parent) : QScrollArea(parent)
{ setWidgetResizable(true); setFrameShape(QFrame::NoFrame); setupUi(); }

void ToolsWidget::setupUi()
{
    auto* c = new QWidget(this); setWidget(c);
    auto* l = new QVBoxLayout(c); l->setContentsMargins(10,10,10,10); l->setSpacing(8);
    setupHydroMgmt(l); setupConfigMgmt(l); setupHardware(l); setupSvParams(l); setupSvFilter(l);
    l->addStretch();
}

void ToolsWidget::setupHydroMgmt(QVBoxLayout* l)
{
    auto* grp = new QGroupBox("Gestion hydrophones (base des modèles)");
    auto* r = new QHBoxLayout(grp);
    m_comboModels = new QComboBox(); m_comboModels->addItems(HydroModels::list()); m_comboModels->setMinimumWidth(180);
    auto* bc = new QPushButton("Créer");     bc->setProperty("cssClass","primary");
    auto* bm = new QPushButton("Modifier");  bm->setProperty("cssClass","warning");
    auto* bd = new QPushButton("Supprimer"); bd->setProperty("cssClass","danger");
    connect(bc, &QPushButton::clicked, this, [this]() {
        bool ok; const QString n = QInputDialog::getText(this, "Créer hydrophone", "Nom du modèle :", QLineEdit::Normal, "", &ok).trimmed();
        if (!ok || n.isEmpty()) return;
        QStringList l = HydroModels::list(); if (l.contains(n)) { QMessageBox::warning(this, "Créer", "Ce modèle existe déjà."); return; }
        l << n; HydroModels::save(l); m_comboModels->clear(); m_comboModels->addItems(l); m_comboModels->setCurrentText(n); emit hydroModelsChanged(); });
    connect(bm, &QPushButton::clicked, this, [this]() {
        const QString old = m_comboModels->currentText(); if (old.isEmpty()) return;
        bool ok; const QString n = QInputDialog::getText(this, "Modifier hydrophone", "Nouveau nom :", QLineEdit::Normal, old, &ok).trimmed();
        if (!ok || n.isEmpty() || n == old) return;
        QStringList l = HydroModels::list(); const int i = l.indexOf(old); if (i >= 0) l[i] = n;
        HydroModels::save(l); m_comboModels->clear(); m_comboModels->addItems(l); m_comboModels->setCurrentText(n); emit hydroModelsChanged(); });
    connect(bd, &QPushButton::clicked, this, [this]() {
        const QString n = m_comboModels->currentText(); if (n.isEmpty()) return;
        if (QMessageBox::question(this, "Supprimer", "Supprimer le modèle « " + n + " » ?") != QMessageBox::Yes) return;
        QStringList l = HydroModels::list(); l.removeAll(n); HydroModels::save(l);
        m_comboModels->clear(); m_comboModels->addItems(l); emit hydroModelsChanged(); });
    r->addWidget(m_comboModels); r->addWidget(bc); r->addWidget(bm); r->addWidget(bd); r->addStretch();
    l->addWidget(grp);
}

void ToolsWidget::setupConfigMgmt(QVBoxLayout* l)
{
    auto* grp = new QGroupBox("Gestion configuration (JSON)");
    auto* vl = new QVBoxLayout(grp);
    auto mk = [](const QString& t, const QString& c){ auto* b = new QPushButton(t); b->setProperty("cssClass", c); return b; };
    auto* bs = mk("Sauvegarder","primary"); auto* bl = mk("Charger","warning"); auto* bd = mk("Supprimer","danger");
    auto* be = mk("Exporter","primary");    auto* bi = mk("Importer","warning");
    connect(bs, &QPushButton::clicked, this, &ToolsWidget::saveConfigRequested);
    connect(bl, &QPushButton::clicked, this, &ToolsWidget::loadConfigRequested);
    connect(bd, &QPushButton::clicked, this, &ToolsWidget::deleteConfigRequested);
    connect(be, &QPushButton::clicked, this, &ToolsWidget::exportConfigRequested);
    connect(bi, &QPushButton::clicked, this, &ToolsWidget::importConfigRequested);
    auto* r1 = new QHBoxLayout(); r1->addWidget(bs); r1->addWidget(bl); r1->addWidget(bd); r1->addStretch();
    auto* r2 = new QHBoxLayout(); r2->addWidget(be); r2->addWidget(bi); r2->addStretch();
    vl->addLayout(r1); vl->addLayout(r2);
    l->addWidget(grp);
}

// Mapping materiel : tout ce qui depend du cablage / des cartes, sans recompiler.
void ToolsWidget::setupHardware(QVBoxLayout* l)
{
    auto* grp = new QGroupBox("Mapping matériel (cartes NI)");
    auto* g = new QGridLayout(grp); g->setSpacing(6);
    QSettings s;
    auto* eV = new QLineEdit(s.value("hw/voltChannels").toString());
    eV->setPlaceholderText("ex. Dev3/ai0,Dev3/ai1  (carte non définie dans l'Excel)");
    auto* eI = new QLineEdit(s.value("hw/currChannels").toString());
    eI->setPlaceholderText("ex. Dev3/ai2,Dev3/ai3");
    auto* sT = new QDoubleSpinBox(); sT->setRange(20, 500); sT->setValue(s.value("hw/tempLimit", 80.0).toDouble()); sT->setSuffix(" °C");
    sT->setToolTip("Au-dessus de cette température, l'émission est coupée");
    auto* ePm = new QLineEdit(s.value("hw/pinmapFile").toString());
    auto* eLv = new QLineEdit(s.value("hw/levelsFile").toString());
    auto* eTm = new QLineEdit(s.value("hw/timingFile").toString());
    auto* ePt = new QLineEdit(s.value("hw/patternFile").toString());
    int r = 0;
    g->addWidget(new QLabel("Voies tension"), r, 0);  g->addWidget(eV, r++, 1);
    g->addWidget(new QLabel("Voies courant"), r, 0);  g->addWidget(eI, r++, 1);
    g->addWidget(new QLabel("Limite température"), r, 0); g->addWidget(sT, r++, 1);
    g->addWidget(new QLabel("MLI : pin map"), r, 0);  g->addWidget(ePm, r++, 1);
    g->addWidget(new QLabel("MLI : niveaux"), r, 0);  g->addWidget(eLv, r++, 1);
    g->addWidget(new QLabel("MLI : timing"), r, 0);   g->addWidget(eTm, r++, 1);
    g->addWidget(new QLabel("MLI : pattern"), r, 0);  g->addWidget(ePt, r++, 1);
    auto bind = [this](QLineEdit* e, const char* key) {
        connect(e, &QLineEdit::editingFinished, this, [this, e, key]() { QSettings().setValue(key, e->text().trimmed()); emit hardwareMappingChanged(); }); };
    bind(eV, "hw/voltChannels"); bind(eI, "hw/currChannels");
    bind(ePm, "hw/pinmapFile"); bind(eLv, "hw/levelsFile"); bind(eTm, "hw/timingFile"); bind(ePt, "hw/patternFile");
    connect(sT, &QDoubleSpinBox::valueChanged, this, [this](double v) { QSettings().setValue("hw/tempLimit", v); emit hardwareMappingChanged(); });
    l->addWidget(grp);
}

void ToolsWidget::setupSvParams(QVBoxLayout* l)
{
    auto* grp = new QGroupBox("Mesure Sv — Paramètres");
    auto* g = new QGridLayout(grp); g->setSpacing(6);
    auto dbl = [](double lo, double hi, double v, const QString& s = "", int d = 1) {
        auto* x = new QDoubleSpinBox(); x->setRange(lo, hi); x->setValue(v); x->setDecimals(d);
        if (!s.isEmpty()) x->setSuffix(s); return x; };
    m_spinListenCh = new QSpinBox(); m_spinListenCh->setRange(1, 6); m_spinListenCh->setValue(1);
    m_comboHydro = new QComboBox(); m_comboHydro->addItems(HydroModels::list());
    m_spinGain = dbl(0, 80, 20, " dB");
    m_spinFs = new QSpinBox(); m_spinFs->setRange(1000, 204800); m_spinFs->setValue(96000); m_spinFs->setSuffix(" Hz");
    m_spinPMin = dbl(0, 360, 0, " °"); m_spinPMax = dbl(0, 360, 360, " °");
    m_spinRMin = dbl(0, 90, 0, " °");  m_spinRMax = dbl(0, 90, 90, " °");
    m_spinAngRes = dbl(0.1, 10, 1, " °"); m_spinMeasT = dbl(1, 10000, 100, " ms", 0);
    m_spinFMin = dbl(1, 1e6, 20000, " Hz", 0); m_spinFMax = dbl(1, 1e6, 80000, " Hz", 0);
    m_spinFStep = dbl(1, 1e5, 1000, " Hz", 0);
    m_spinHD = dbl(0, 1000, 0.5, " m", 2); m_spinTD = dbl(0, 1000, 1.0, " m", 2); m_spinDist = dbl(0, 1000, 1.5, " m", 2);
    int r = 0;
    auto a2 = [&](const QString& l1, QWidget* w1, const QString& l2, QWidget* w2) {
        g->addWidget(new QLabel(l1), r, 0); g->addWidget(w1, r, 1); g->addWidget(new QLabel(l2), r, 2); g->addWidget(w2, r, 3); r++; };
    auto aR = [&](const QString& lbl, QDoubleSpinBox* a, QDoubleSpinBox* b) {
        auto* hl = new QHBoxLayout(); hl->addWidget(a); hl->addWidget(new QLabel("→")); hl->addWidget(b);
        auto* w = new QWidget(); w->setLayout(hl);
        g->addWidget(new QLabel(lbl), r, 0); g->addWidget(w, r, 1, 1, 3); r++; };
    a2("Voie écoute", m_spinListenCh, "Hydrophone", m_comboHydro);
    a2("Gain", m_spinGain, "Freq. acq.", m_spinFs);
    aR("Plage gîte (°)", m_spinPMin, m_spinPMax);
    aR("Plage site (°)", m_spinRMin, m_spinRMax);
    a2("Résolution ang.", m_spinAngRes, "Temps mesure", m_spinMeasT);
    aR("Bande fréq. (Hz)", m_spinFMin, m_spinFMax);
    a2("Pas fréquentiel", m_spinFStep, "Immersion hydro", m_spinHD);
    a2("Immersion transd.", m_spinTD, "Distance h-t", m_spinDist);
    auto* note = new QLabel("Paramètres enregistrés avec la configuration. Le balayage Sv automatisé n'est pas encore implémenté.");
    note->setWordWrap(true); note->setStyleSheet("color:#8b949e;");
    g->addWidget(note, r, 0, 1, 4);
    l->addWidget(grp);
}

void ToolsWidget::setupSvFilter(QVBoxLayout* l)
{
    auto* grp = new QGroupBox("Filtre Sv");
    auto* g = new QGridLayout(grp); g->setSpacing(6);
    auto* en = new QLineEdit(); en->setPlaceholderText("nom du filtre");
    auto* bc = new QPushButton("Créer filtre"); auto* bs = new QPushButton("Sauvegarder");
    bc->setEnabled(false); bs->setEnabled(false);
    bc->setToolTip("Non implémenté dans cette version"); bs->setToolTip("Non implémenté dans cette version");
    g->addWidget(new QLabel("Nom filtre"), 0, 0); g->addWidget(en, 0, 1, 1, 2);
    g->addWidget(bc, 1, 0); g->addWidget(bs, 1, 1);
    l->addWidget(grp);
}

SvConfig ToolsWidget::svConfig() const
{
    SvConfig c;
    c.listenChannel = m_spinListenCh->value(); c.gain = m_spinGain->value(); c.sampleRate = m_spinFs->value();
    c.pitchMin = m_spinPMin->value(); c.pitchMax = m_spinPMax->value(); c.rollMin = m_spinRMin->value(); c.rollMax = m_spinRMax->value();
    c.angularResolution = m_spinAngRes->value(); c.measureTime = m_spinMeasT->value();
    c.freqMin = m_spinFMin->value(); c.freqMax = m_spinFMax->value(); c.freqStep = m_spinFStep->value();
    c.hydroDepth = m_spinHD->value(); c.transducerDepth = m_spinTD->value(); c.distance = m_spinDist->value();
    return c;
}

void ToolsWidget::loadSv(const SvConfig& c)
{
    m_spinListenCh->setValue(c.listenChannel); m_spinGain->setValue(c.gain); m_spinFs->setValue(c.sampleRate);
    m_spinPMin->setValue(c.pitchMin); m_spinPMax->setValue(c.pitchMax); m_spinRMin->setValue(c.rollMin); m_spinRMax->setValue(c.rollMax);
    m_spinAngRes->setValue(c.angularResolution); m_spinMeasT->setValue(c.measureTime);
    m_spinFMin->setValue(c.freqMin); m_spinFMax->setValue(c.freqMax); m_spinFStep->setValue(c.freqStep);
    m_spinHD->setValue(c.hydroDepth); m_spinTD->setValue(c.transducerDepth); m_spinDist->setValue(c.distance);
}
