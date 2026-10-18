#include "ConfigWindow.h"
#include "HydroWidget.h"
#include "AmplifierWidget.h"
#include "ToolsWidget.h"
#include "StyleManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QMenuBar>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ConfigWindow::ConfigWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("ACOUSTIMETER — Configuration");
    setMinimumSize(900, 640); resize(1100, 750);
    setupUi();
}

void ConfigWindow::setupUi()
{
    auto* mf = menuBar()->addMenu("Fichier");
    connect(mf->addAction("Sauvegarder..."), &QAction::triggered, this, &ConfigWindow::saveNamed);
    connect(mf->addAction("Charger..."),     &QAction::triggered, this, &ConfigWindow::loadNamed);
    connect(mf->addAction("Exporter..."),    &QAction::triggered, this, &ConfigWindow::exportTo);
    connect(mf->addAction("Importer..."),    &QAction::triggered, this, &ConfigWindow::importFrom);
    auto* central = new QWidget(this); central->setAutoFillBackground(true); setCentralWidget(central);
    auto* mainLayout = new QHBoxLayout(central);
    mainLayout->setSpacing(0); mainLayout->setContentsMargins(0,0,0,0);
    setupLeftPanel();
    mainLayout->addWidget(m_leftPanel);
    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget, 1);
    m_tools = new ToolsWidget(this);
    connect(m_tools, &ToolsWidget::hydroModelsChanged, this, [this]() { for (auto* h : m_hydros) h->refreshModels(); });
    connect(m_tools, &ToolsWidget::saveConfigRequested,   this, &ConfigWindow::saveNamed);
    connect(m_tools, &ToolsWidget::loadConfigRequested,   this, &ConfigWindow::loadNamed);
    connect(m_tools, &ToolsWidget::deleteConfigRequested, this, &ConfigWindow::deleteNamed);
    connect(m_tools, &ToolsWidget::exportConfigRequested, this, &ConfigWindow::exportTo);
    connect(m_tools, &ToolsWidget::importConfigRequested, this, &ConfigWindow::importFrom);
    connect(m_tools, &ToolsWidget::hardwareMappingChanged, this, &ConfigWindow::hardwareMappingChanged);
    syncTabs();
}

void ConfigWindow::setupLeftPanel()
{
    m_leftPanel = new QWidget(this); m_leftPanel->setFixedWidth(220);
    m_leftPanel->setObjectName("panelFrame");
    m_leftPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_leftPanel->setStyleSheet("QWidget#panelFrame{background:#161b22;border-right:1px solid #30363d;}");
    auto* layout = new QVBoxLayout(m_leftPanel);
    layout->setContentsMargins(12,14,12,14); layout->setSpacing(12);
    auto addSpin = [&](const QString& lbl, QSpinBox*& spin, int def) {
        auto* l = new QLabel(lbl); l->setStyleSheet("font-size:13px;color:#8b949e;font-weight:bold;");
        auto* row = new QHBoxLayout();
        auto* bm = new QPushButton("−"); bm->setFixedSize(26,26);
        spin = new QSpinBox(); spin->setRange(0, 999); spin->setValue(def); spin->setMinimumHeight(28);
        auto* bp = new QPushButton("+"); bp->setFixedSize(26,26);
        row->addWidget(bm); row->addWidget(spin); row->addWidget(bp);
        QSpinBox* sp = spin;
        connect(bm, &QPushButton::clicked, [sp]() { sp->setValue(sp->value() - 1); });
        connect(bp, &QPushButton::clicked, [sp]() { sp->setValue(sp->value() + 1); });
        layout->addWidget(l); layout->addLayout(row);
    };
    addSpin("Hydrophones",    m_spinHydros, 4);
    addSpin("Amplificateurs", m_spinAmplis, 2);
    auto* btnVal = new QPushButton("▶  VALIDER");
    btnVal->setProperty("cssClass", "connect"); btnVal->setMinimumHeight(34);
    connect(btnVal, &QPushButton::clicked, this, &ConfigWindow::onValidate);
    layout->addWidget(btnVal);
    auto* note = new QLabel("VALIDER crée ou retire les onglets sans effacer les valeurs saisies, puis applique la configuration au matériel.");
    note->setWordWrap(true); note->setStyleSheet("font-size:11px;color:#6e7681;");
    layout->addWidget(note);
    auto* sep = new QFrame(); sep->setFrameShape(QFrame::HLine); sep->setStyleSheet("color:#30363d;"); layout->addWidget(sep);
    auto* lc = new QLabel("Fichier config"); lc->setStyleSheet("font-size:13px;color:#8b949e;font-weight:bold;");
    layout->addWidget(lc);
    auto* bs = new QPushButton("Sauvegarder JSON"); bs->setProperty("cssClass","primary"); bs->setMinimumHeight(30);
    auto* bl = new QPushButton("Charger JSON");     bl->setProperty("cssClass","warning"); bl->setMinimumHeight(30);
    auto* be = new QPushButton("Exporter");         be->setProperty("cssClass","primary"); be->setMinimumHeight(30);
    connect(bs, &QPushButton::clicked, this, &ConfigWindow::saveNamed);
    connect(bl, &QPushButton::clicked, this, &ConfigWindow::loadNamed);
    connect(be, &QPushButton::clicked, this, &ConfigWindow::exportTo);
    layout->addWidget(bs); layout->addWidget(bl); layout->addWidget(be);
    layout->addStretch();
}

void ConfigWindow::addHydro()
{
    const int i = m_hydros.size() + 1;
    auto* w = new HydroWidget(i, this);
    connect(w, &HydroWidget::createRequested, this, [this]() { m_spinHydros->setValue(m_spinHydros->value() + 1); syncTabs(); });
    connect(w, &HydroWidget::applyRequested,  this, &ConfigWindow::onValidate);
    connect(w, &HydroWidget::deleteRequested, this, &ConfigWindow::removeHydro);
    m_hydros << w;
}

void ConfigWindow::addAmp()
{
    const int i = m_amps.size() + 1;
    auto* w = new AmplifierWidget(i, this);
    connect(w, &AmplifierWidget::createRequested, this, [this]() { m_spinAmplis->setValue(m_spinAmplis->value() + 1); syncTabs(); });
    connect(w, &AmplifierWidget::applyRequested,  this, &ConfigWindow::onValidate);
    connect(w, &AmplifierWidget::deleteRequested, this, &ConfigWindow::removeAmp);
    connect(w, &AmplifierWidget::generateMliRequested, this, &ConfigWindow::generateMliRequested);
    connect(w, &AmplifierWidget::voieToggled, this, &ConfigWindow::ampVoieToggled);
    m_amps << w;
}

void ConfigWindow::removeHydro(int index)
{
    if (index < 1 || index > m_hydros.size()) return;
    delete m_hydros.takeAt(index - 1);
    m_spinHydros->setValue(m_hydros.size());
    syncTabs(); emit configChanged(config());
}

void ConfigWindow::removeAmp(int index)
{
    if (index < 1 || index > m_amps.size()) return;
    emit ampVoieToggled(index, false);
    delete m_amps.takeAt(index - 1);
    m_spinAmplis->setValue(m_amps.size());
    syncTabs(); emit configChanged(config());
}

// Ajuste le nombre d'onglets SANS detruire ceux qui existent deja (les valeurs saisies sont conservees).
void ConfigWindow::syncTabs()
{
    if (m_syncing) return; m_syncing = true;
    while (m_hydros.size() < m_spinHydros->value()) addHydro();
    while (m_hydros.size() > m_spinHydros->value()) delete m_hydros.takeLast();
    while (m_amps.size() < m_spinAmplis->value()) addAmp();
    while (m_amps.size() > m_spinAmplis->value()) { emit ampVoieToggled(m_amps.size(), false); delete m_amps.takeLast(); }
    retitle();
    m_syncing = false;
}

void ConfigWindow::retitle()
{
    const int cur = m_tabWidget->currentIndex();
    while (m_tabWidget->count() > 0) m_tabWidget->removeTab(0);        // retire sans supprimer les widgets
    for (int i = 0; i < m_hydros.size(); ++i) { m_hydros[i]->setIndex(i + 1); m_tabWidget->addTab(m_hydros[i], QString("Hydro %1").arg(i + 1)); }
    for (int i = 0; i < m_amps.size(); ++i)   { m_amps[i]->setIndex(i + 1);   m_tabWidget->addTab(m_amps[i],   QString("Amp %1").arg(i + 1)); }
    m_tabWidget->addTab(m_tools, "⚙ Outils");
    m_tabWidget->setCurrentIndex(qBound(0, cur, m_tabWidget->count() - 1));
}

void ConfigWindow::onValidate()
{
    syncTabs();
    emit configChanged(config());
}

void ConfigWindow::setMliResult(int ampId, bool ok, const QString& message, const QVector<quint8>& preview)
{
    if (ampId >= 1 && ampId <= m_amps.size()) m_amps[ampId - 1]->setMliResult(ok, message, preview);
}

SystemConfig ConfigWindow::config() const
{
    SystemConfig c;
    for (auto* h : m_hydros) c.hydrophones << h->config();
    for (auto* a : m_amps)   c.amplifiers  << a->config();
    c.sv = m_tools->svConfig();
    return c;
}

void ConfigWindow::setConfig(const SystemConfig& c)
{
    m_spinHydros->setValue(c.hydrophones.size()); m_spinAmplis->setValue(c.amplifiers.size());
    syncTabs();
    for (int i = 0; i < c.hydrophones.size() && i < m_hydros.size(); ++i) m_hydros[i]->loadConfig(c.hydrophones[i]);
    for (int i = 0; i < c.amplifiers.size() && i < m_amps.size(); ++i)    m_amps[i]->loadConfig(c.amplifiers[i]);
    m_tools->loadSv(c.sv);
}

// --------------------------------------------------------------------- JSON
QString ConfigWindow::configDir() const
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/configs";
    QDir().mkpath(d); return d;
}

bool ConfigWindow::writeFile(const QString& path) const
{
    const SystemConfig c = config();
    QJsonArray hs, as;
    for (const auto& h : c.hydrophones)
        hs << QJsonObject{{"id",h.id},{"channel",h.channel},{"gain",h.gain},{"sampleRate",h.sampleRate},{"dynamic",h.dynamic},
                          {"distance",h.distance},{"depth",h.depth},{"model",h.model},{"sensitivityFile",h.sensitivityFile}};
    for (const auto& a : c.amplifiers)
        as << QJsonObject{{"id",a.id},{"wavFile",a.wavFile},{"filterFile",a.filterFile},{"carrierType",a.carrierType},
                          {"carrierFreq",a.carrierFreq},{"amplitude",a.amplitude},{"offset",a.offset},{"mliPin",a.mliPin},
                          {"tempProbeCount",a.tempProbeCount},{"tempType",a.tempType},{"tempAcqFreq",a.tempAcqFreq},{"tempLabel",a.tempLabel},
                          {"voltChannelCount",a.voltChannelCount},{"voltDivision",a.voltDivision},{"voltAcqFreq",a.voltAcqFreq},{"voltLabel",a.voltLabel},
                          {"currChannelCount",a.currChannelCount},{"currDivision",a.currDivision},{"currAcqFreq",a.currAcqFreq},{"currLabel",a.currLabel}};
    const SvConfig& s = c.sv;
    QJsonObject sv{{"listenChannel",s.listenChannel},{"gain",s.gain},{"sampleRate",s.sampleRate},{"pitchMin",s.pitchMin},{"pitchMax",s.pitchMax},
                   {"rollMin",s.rollMin},{"rollMax",s.rollMax},{"angularResolution",s.angularResolution},{"measureTime",s.measureTime},
                   {"freqMin",s.freqMin},{"freqMax",s.freqMax},{"freqStep",s.freqStep},{"hydroDepth",s.hydroDepth},
                   {"transducerDepth",s.transducerDepth},{"distance",s.distance}};
    QJsonObject root{{"version",1},{"hydrophones",hs},{"amplifiers",as},{"sv",sv}};
    QFile f(path); if (!f.open(QIODevice::WriteOnly)) return false;
    return f.write(QJsonDocument(root).toJson()) > 0;
}

bool ConfigWindow::readFile(const QString& path)
{
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonParseError pe; const auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject r = doc.object(); SystemConfig c;
    for (const auto& v : r["hydrophones"].toArray()) { const auto o = v.toObject(); HydrophoneConfig h;
        h.id=o["id"].toInt(); h.channel=o["channel"].toInt(1); h.gain=o["gain"].toDouble(20); h.sampleRate=o["sampleRate"].toInt(96000);
        h.dynamic=o["dynamic"].toDouble(10); h.distance=o["distance"].toDouble(1); h.depth=o["depth"].toDouble(0.5);
        h.model=o["model"].toString(); h.sensitivityFile=o["sensitivityFile"].toString(); c.hydrophones << h; }
    for (const auto& v : r["amplifiers"].toArray()) { const auto o = v.toObject(); AmplifierConfig a;
        a.id=o["id"].toInt(); a.wavFile=o["wavFile"].toString(); a.filterFile=o["filterFile"].toString();
        a.carrierType=o["carrierType"].toString("Triangle"); a.carrierFreq=o["carrierFreq"].toDouble(38400);
        a.amplitude=o["amplitude"].toDouble(1); a.offset=o["offset"].toDouble(0); a.mliPin=o["mliPin"].toString();
        a.tempProbeCount=o["tempProbeCount"].toInt(); a.tempType=o["tempType"].toString("K"); a.tempAcqFreq=o["tempAcqFreq"].toDouble(1); a.tempLabel=o["tempLabel"].toString();
        a.voltChannelCount=o["voltChannelCount"].toInt(); a.voltDivision=o["voltDivision"].toString("1/10"); a.voltAcqFreq=o["voltAcqFreq"].toDouble(10); a.voltLabel=o["voltLabel"].toString();
        a.currChannelCount=o["currChannelCount"].toInt(); a.currDivision=o["currDivision"].toString("1/10"); a.currAcqFreq=o["currAcqFreq"].toDouble(10); a.currLabel=o["currLabel"].toString();
        c.amplifiers << a; }
    const auto s = r["sv"].toObject(); SvConfig& sv = c.sv;
    sv.listenChannel=s["listenChannel"].toInt(1); sv.gain=s["gain"].toDouble(20); sv.sampleRate=s["sampleRate"].toInt(96000);
    sv.pitchMin=s["pitchMin"].toDouble(0); sv.pitchMax=s["pitchMax"].toDouble(360); sv.rollMin=s["rollMin"].toDouble(0); sv.rollMax=s["rollMax"].toDouble(90);
    sv.angularResolution=s["angularResolution"].toDouble(1); sv.measureTime=s["measureTime"].toDouble(100);
    sv.freqMin=s["freqMin"].toDouble(20000); sv.freqMax=s["freqMax"].toDouble(80000); sv.freqStep=s["freqStep"].toDouble(1000);
    sv.hydroDepth=s["hydroDepth"].toDouble(0.5); sv.transducerDepth=s["transducerDepth"].toDouble(1); sv.distance=s["distance"].toDouble(1.5);
    setConfig(c); emit configChanged(config());
    return true;
}

void ConfigWindow::saveNamed()
{
    bool ok; const QString n = QInputDialog::getText(this, "Sauvegarder", "Nom de la configuration :", QLineEdit::Normal, "config", &ok).trimmed();
    if (!ok || n.isEmpty()) return;
    const QString p = configDir() + "/" + n + ".json";
    if (QFile::exists(p) && QMessageBox::question(this, "Sauvegarder", "Remplacer « " + n + " » ?") != QMessageBox::Yes) return;
    if (!writeFile(p)) QMessageBox::critical(this, "Sauvegarder", "Écriture impossible : " + p);
}

void ConfigWindow::loadNamed()
{
    const QStringList files = QDir(configDir()).entryList({"*.json"}, QDir::Files, QDir::Name);
    if (files.isEmpty()) { QMessageBox::information(this, "Charger", "Aucune configuration sauvegardée."); return; }
    bool ok; const QString n = QInputDialog::getItem(this, "Charger", "Configuration :", files, 0, false, &ok);
    if (ok && !readFile(configDir() + "/" + n)) QMessageBox::critical(this, "Charger", "Fichier illisible : " + n);
}

void ConfigWindow::deleteNamed()
{
    const QStringList files = QDir(configDir()).entryList({"*.json"}, QDir::Files, QDir::Name);
    if (files.isEmpty()) { QMessageBox::information(this, "Supprimer", "Aucune configuration sauvegardée."); return; }
    bool ok; const QString n = QInputDialog::getItem(this, "Supprimer", "Configuration :", files, 0, false, &ok);
    if (ok && QMessageBox::question(this, "Supprimer", "Supprimer « " + n + " » ?") == QMessageBox::Yes) QFile::remove(configDir() + "/" + n);
}

void ConfigWindow::exportTo()
{
    const QString p = QFileDialog::getSaveFileName(this, "Exporter la configuration", "config.json", "JSON (*.json)");
    if (!p.isEmpty() && !writeFile(p)) QMessageBox::critical(this, "Exporter", "Écriture impossible.");
}

void ConfigWindow::importFrom()
{
    const QString p = QFileDialog::getOpenFileName(this, "Importer une configuration", "", "JSON (*.json)");
    if (!p.isEmpty() && !readFile(p)) QMessageBox::critical(this, "Importer", "Fichier JSON invalide.");
}
