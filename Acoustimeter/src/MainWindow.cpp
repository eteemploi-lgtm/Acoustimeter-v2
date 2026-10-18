#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "GraphWidget.h"
#include "ConfigWindow.h"
#include "StyleManager.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QStyle>
#include <QTextEdit>
#include <QToolBar>
#include <QDialog>
#include <QDateTime>
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>

static const qint64 STALE_MS = 2500;

MainWindow::MainWindow(const ConnectionConfig& cfg, HardwareManager* hw, QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_hw(hw), m_connConfig(cfg)
{
    ui->setupUi(this);
    setStyleSheet(StyleManager::instance().mainStyleSheet());

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir().mkpath(dir);
    m_logPath = dir + "/acoustimeter_" + QDate::currentDate().toString("yyyyMMdd") + ".log";

    // Fenetre de configuration unique (creee masquee : sa configuration pilote le materiel)
    m_configWindow = new ConfigWindow(nullptr);
    m_configWindow->setStyleSheet(StyleManager::instance().configStyleSheet());
    m_configWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_configWindow, &ConfigWindow::configChanged,        this, &MainWindow::onConfigChanged);
    connect(m_configWindow, &ConfigWindow::generateMliRequested, m_hw, &HardwareManager::generateMli);
    connect(m_configWindow, &ConfigWindow::ampVoieToggled,       this, &MainWindow::onAmpVoieToggled);
    connect(m_configWindow, &ConfigWindow::hardwareMappingChanged, this, [this]() { applyTempLimit(); m_hw->applyConfig(m_cfg); });
    m_cfg = m_configWindow->config();
    m_cfg.connection = cfg;

    setupMenuBar();
    setupVoieBar();
    setupCharts();
    setupSidebar();
    setupLogView();
    setupStatusBar();

    connect(m_hw, &HardwareManager::devicesDetected,     this, &MainWindow::onDevicesDetected);
    connect(m_hw, &HardwareManager::channelsConfigured,  this, &MainWindow::onChannelsConfigured);
    connect(m_hw, &HardwareManager::temperatureUpdated,  this, &MainWindow::onTemperature);
    connect(m_hw, &HardwareManager::slowUpdated,         this, &MainWindow::onSlow);
    connect(m_hw, &HardwareManager::acousticBlock,       this, &MainWindow::onAcoustic);
    connect(m_hw, &HardwareManager::saturation,          this, &MainWindow::onSaturation);
    connect(m_hw, &HardwareManager::errorOccurred,       this, &MainWindow::onHwError);
    connect(m_hw, &HardwareManager::logMessage,          this, &MainWindow::appendLog);
    connect(m_hw, &HardwareManager::mliGenerated,        this, &MainWindow::onMliGenerated);
    connect(m_hw, &HardwareManager::emissionStateChanged,this, &MainWindow::onEmissionState);

    m_watchTimer = new QTimer(this);
    connect(m_watchTimer, &QTimer::timeout, this, &MainWindow::watchdog);
    m_watchTimer->start(1000);

    updateNiBadge();
    if (m_connConfig.offline) {
        appendLog("warn", "MODE HORS LIGNE : aucune carte NI, aucune mesure, émission impossible");
    } else {
        appendLog("ok", QString("Connexion matérielle : %1 carte(s) NI").arg(m_hw->devices().size()));
        for (const auto& d : m_hw->devices()) appendLog("info", QString("%1 : %2").arg(d.name, d.productType));
        applyTempLimit();
        m_hw->applyConfig(m_cfg);             // demarre la surveillance (temperatures, tensions, courants)
    }
    appendLog("info", "Configuration par défaut chargée : ouvrir CONFIG pour l'ajuster puis VALIDER");
}

MainWindow::~MainWindow()
{
    if (m_configWindow) { m_configWindow->close(); delete m_configWindow; m_configWindow = nullptr; }
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    m_hw->emergencyStop();                    // arret controle : emission puis acquisitions
    if (m_configWindow) m_configWindow->close();
    e->accept();
}

// ----------------------------------------------------------------------------- menus / barre
void MainWindow::showAbout()
{
    QDialog* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("À propos — ACOUSTIMETER");
    dlg->setFixedSize(420, 260);
    dlg->setStyleSheet(StyleManager::instance().appStyleSheet());
    auto* l = new QVBoxLayout(dlg);
    auto addRow = [&](const QString& lbl, const QString& val) {
        auto* hl = new QHBoxLayout();
        auto* lb = new QLabel(lbl); lb->setStyleSheet("color:#8b949e;font-size:13px;");
        auto* vl = new QLabel(val); vl->setStyleSheet("color:#00d4ff;font-size:13px;font-weight:bold;");
        vl->setAlignment(Qt::AlignRight); hl->addWidget(lb); hl->addWidget(vl); l->addLayout(hl); };
    addRow("Version", "1.0 (production)");
    addRow("NI-DAQmx", HardwareManager::niCompiled() ? "compilé" : "NON compilé");
    addRow("Cartes détectées", QString::number(m_hw->devices().size()));
    addRow("Mode", m_connConfig.offline ? "HORS LIGNE" : "matériel réel");
    addRow("Qt", QT_VERSION_STR);
    auto* ok = new QPushButton("Fermer"); ok->setProperty("cssClass", "primary");
    connect(ok, &QPushButton::clicked, dlg, &QDialog::accept);
    l->addWidget(ok);
    dlg->show();
}

void MainWindow::setupMenuBar()
{
    auto* mFile = menuBar()->addMenu("Fichier");
    connect(mFile->addAction("Exporter données (CSV)..."), &QAction::triggered, this, &MainWindow::exportCSV);
    mFile->addSeparator();
    connect(mFile->addAction("Quitter"), &QAction::triggered, this, &QWidget::close);

    auto* mAcq = menuBar()->addMenu("Acquisition");
    connect(mAcq->addAction("Démarrer / arrêter l'acquisition hydrophones"), &QAction::triggered, this, &MainWindow::toggleAcquisition);
    connect(mAcq->addAction("Démarrer / arrêter l'émission"), &QAction::triggered, this, &MainWindow::toggleEmission);
    mAcq->addSeparator();
    connect(mAcq->addAction("ARRÊT D'URGENCE"), &QAction::triggered, this, &MainWindow::emergencyStop);
    mAcq->addSeparator();
    auto* sv = mAcq->addAction("Mesure Sv (non implémentée)"); sv->setEnabled(false);

    auto* mTools = menuBar()->addMenu("Outils");
    connect(mTools->addAction("Calibration (auto-calibration NI)"), &QAction::triggered, m_hw, &HardwareManager::selfCalibrate);
    connect(mTools->addAction("Diagnostic matériel"), &QAction::triggered, this, &MainWindow::showDiagnostic);
    connect(mTools->addAction("Configuration..."), &QAction::triggered, this, &MainWindow::openConfig);

    auto* mHelp = menuBar()->addMenu("Aide");
    connect(mHelp->addAction("◐  Thème clair / sombre"), &QAction::triggered, this, &MainWindow::toggleTheme);
    mHelp->addSeparator();
    connect(mHelp->addAction("À propos..."), &QAction::triggered, this, &MainWindow::showAbout);

    auto* tb = addToolBar("main");
    tb->setMovable(false);
    auto* logoLbl = new QLabel("  ACOUSTIMETER  ");
    logoLbl->setStyleSheet("color:#00d4ff;font-size:16px;font-weight:bold;letter-spacing:2px;");
    tb->addWidget(logoLbl); tb->addSeparator();

    m_niBadge = new QLabel(); m_niBadge->setTextFormat(Qt::RichText);
    tb->addWidget(m_niBadge); tb->addSeparator();

    m_ledLabel = new QLabel("●"); m_ledLabel->setStyleSheet("color:#444c56;font-size:16px;"); m_ledLabel->setFixedWidth(22);
    tb->addWidget(m_ledLabel);

    m_acqButton = new QPushButton("▶  ACQUISITION"); m_acqButton->setProperty("cssClass", "primary");
    m_acqButton->setFixedHeight(30); m_acqButton->setMinimumWidth(150);
    connect(m_acqButton, &QPushButton::clicked, this, &MainWindow::toggleAcquisition);
    tb->addWidget(m_acqButton);

    m_emitButton = new QPushButton("▶  DÉMARRER ÉMISSION"); m_emitButton->setProperty("cssClass", "connect");
    m_emitButton->setFixedHeight(30); m_emitButton->setMinimumWidth(190);
    connect(m_emitButton, &QPushButton::clicked, this, &MainWindow::toggleEmission);
    tb->addWidget(m_emitButton);

    auto* stop = new QPushButton("⛔  ARRÊT D'URGENCE"); stop->setProperty("cssClass", "danger");
    stop->setFixedHeight(30); connect(stop, &QPushButton::clicked, this, &MainWindow::emergencyStop);
    tb->addWidget(stop); tb->addSeparator();

    auto* spacer = new QWidget(); spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    auto* themeBtn = new QPushButton("◐"); themeBtn->setObjectName("themeBtn"); themeBtn->setFixedWidth(34);
    connect(themeBtn, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    tb->addWidget(themeBtn); tb->addSeparator();

    const QString profStr = (m_connConfig.profile == UserProfile::Administrator) ? "🔑 Admin" : "👤 Opérateur";
    auto* userLabel = new QLabel(QString("  %1  ·  <span style='color:#00d4ff'>%2</span>  ").arg(profStr, m_connConfig.chassis));
    userLabel->setTextFormat(Qt::RichText); userLabel->setStyleSheet("color:#8b949e;font-size:13px;");
    tb->addWidget(userLabel);

    auto* cfgBtn = new QPushButton("⚙  CONFIG"); cfgBtn->setProperty("cssClass", "primary");
    cfgBtn->setFixedHeight(30); cfgBtn->setMinimumWidth(100);
    connect(cfgBtn, &QPushButton::clicked, this, &MainWindow::openConfig);
    tb->addWidget(cfgBtn); tb->addSeparator();

    m_ledTimer = new QTimer(this);
    connect(m_ledTimer, &QTimer::timeout, this, &MainWindow::blinkLed);
}

void MainWindow::updateNiBadge()
{
    if (m_connConfig.offline || !m_hw->online())
        m_niBadge->setText("<span style='color:#ff4444;font-weight:bold'>  NI : HORS LIGNE  </span>");
    else
        m_niBadge->setText(QString("<span style='color:#00e676;font-weight:bold'>  NI : %1 carte(s) — données réelles  </span>").arg(m_hw->devices().size()));
}

void MainWindow::setButtonRunning(QPushButton* b, bool running, const QString& idle, const QString& busy)
{
    if (running) {
        b->setText(busy);
        b->setStyleSheet("background:#1a0000;border:1px solid #ff4444;color:#ff4444;font-size:14px;font-weight:bold;min-height:30px;border-radius:2px;");
    } else {
        b->setText(idle); b->setStyleSheet("");
        b->style()->unpolish(b); b->style()->polish(b);
    }
}

// ----------------------------------------------------------------------------- acquisition / emission
void MainWindow::toggleAcquisition()
{
    if (m_hw->acquiring()) {
        m_hw->stopAcquisition();
        setButtonRunning(m_acqButton, false, "▶  ACQUISITION", "");
        m_graphTime->clearData(); m_graphFFT->clearData(); m_graphLevels->clearData(); m_graphSpec->clearData();
    } else {
        if (m_connConfig.offline || !m_hw->online()) { appendLog("err", "Acquisition impossible : aucune carte NI"); return; }
        m_hw->startAcquisition();
        if (m_hw->acquiring()) setButtonRunning(m_acqButton, true, "", "■  ARRÊTER ACQUISITION");
    }
}

void MainWindow::toggleEmission()
{
    if (m_hw->emitting()) { m_hw->stopEmission(); return; }
    QString why;
    if (!m_hw->checkSafetyForEmission(&why)) { appendLog("err", "Émission REFUSÉE : " + why); return; }
    m_hw->startEmission();                   // le worker revalide les conditions, l'etat revient par signal
}

void MainWindow::emergencyStop()
{
    m_hw->emergencyStop();
    setButtonRunning(m_acqButton, false, "▶  ACQUISITION", "");
    appendLog("err", "ARRÊT D'URGENCE déclenché par l'opérateur");
}

void MainWindow::onEmissionState(bool on)
{
    setButtonRunning(m_emitButton, on, "▶  DÉMARRER ÉMISSION", "■  ARRÊTER ÉMISSION");
    if (on) m_ledTimer->start(500);
    else { m_ledTimer->stop(); m_ledLabel->setStyleSheet("color:#444c56;font-size:16px;"); }
    refreshSidebarStatus();
}

void MainWindow::blinkLed()
{
    m_ledOn = !m_ledOn;
    m_ledLabel->setStyleSheet(m_ledOn ? "color:#00e676;font-size:16px;" : "color:#444c56;font-size:16px;");
}

void MainWindow::onAmpVoieToggled(int id, bool on) { m_hw->setAmpActive(id, on); refreshSidebarStatus(); }

void MainWindow::onMliGenerated(int id, bool ok, const QString& msg, const QVector<quint8>& pv)
{
    appendLog(ok ? "ok" : "err", QString("Amp %1 : %2").arg(id).arg(msg));
    if (m_configWindow) m_configWindow->setMliResult(id, ok, msg, pv);
    refreshSidebarStatus();
}

void MainWindow::applyTempLimit() { m_hw->setTempLimit(QSettings().value("hw/tempLimit", 80.0).toDouble()); }

// ----------------------------------------------------------------------------- materiel -> IHM
void MainWindow::onDevicesDetected(const QVector<NiDevice>&) { updateNiBadge(); }

void MainWindow::onChannelsConfigured()
{
    m_miniByLabel.clear();
    GraphWidget* minis[] = {m_miniTemp1, m_miniTemp2, m_miniV1, m_miniV2, m_miniI1, m_miniI2};
    QGroupBox* boxes[]   = {m_boxT1, m_boxT2, m_boxV1, m_boxV2, m_boxI1, m_boxI2};
    for (auto* g : minis) g->clearMini();
    auto assign = [&](const QStringList& labels, int base, const QString& prefix) {
        for (int i = 0; i < 2; ++i) {
            boxes[base + i]->setTitle(i < labels.size() ? QString("%1 — %2").arg(prefix, labels[i]) : prefix + " (non assignée)");
            if (i < labels.size()) { minis[base + i]->setMiniPlotLabel(labels[i]); m_miniByLabel[labels[i]] = minis[base + i]; }
        }
    };
    assign(m_hw->tempLabels(), 0, "Température");
    assign(m_hw->voltLabels(), 2, "Tension");
    assign(m_hw->currLabels(), 4, "Courant");
}

void MainWindow::onTemperature(const QString& label, double c)
{
    if (auto* g = m_miniByLabel.value(label)) g->pushMiniValue(c);
    m_history.push_back({QDateTime::currentMSecsSinceEpoch(), label, 'T', c});
    if (m_history.size() > 200000) m_history.erase(m_history.begin(), m_history.begin() + 10000);
}

void MainWindow::onSlow(const QString& label, double v, QChar kind)
{
    if (auto* g = m_miniByLabel.value(label)) g->pushMiniValue(v);
    m_history.push_back({QDateTime::currentMSecsSinceEpoch(), label, kind, v});
    if (m_history.size() > 200000) m_history.erase(m_history.begin(), m_history.begin() + 10000);
}

void MainWindow::onAcoustic(const QString& key, const QVector<double>& v, double fs)
{
    m_lastData[key] = QDateTime::currentMSecsSinceEpoch();
    m_graphTime->pushSamples(key, v, fs); m_graphFFT->pushSamples(key, v, fs);
    m_graphLevels->pushSamples(key, v, fs); m_graphSpec->pushSamples(key, v, fs);
}

void MainWindow::onSaturation(const QString& key, bool over)
{
    for (auto* g : {m_graphTime, m_graphFFT, m_graphLevels, m_graphSpec}) g->setSaturated(key, over);
    if (over && !m_sat.value(key)) appendLog("err", "SATURATION > 95 % de la dynamique : " + key.toUpper());
    m_sat[key] = over;
}

void MainWindow::onHwError(const QString& text)
{
    appendLog("err", text);
    updateNiBadge();
    if (!m_hw->acquiring()) setButtonRunning(m_acqButton, false, "▶  ACQUISITION", "");
}

void MainWindow::watchdog() { refreshSidebarStatus(); }

// ----------------------------------------------------------------------------- configuration
void MainWindow::onConfigChanged(const SystemConfig& cfg)
{
    m_cfg = cfg; m_cfg.connection = m_connConfig;
    if (m_hw->acquiring()) { m_hw->stopAcquisition(); setButtonRunning(m_acqButton, false, "▶  ACQUISITION", ""); appendLog("warn", "Configuration modifiée : acquisition arrêtée, relancer ACQUISITION"); }
    if (m_hw->emitting())  { m_hw->stopEmission(); appendLog("warn", "Configuration modifiée : émission arrêtée"); }
    rebuildVoieBar(); rebuildSidebarLists(); refreshSidebarStatus();
    appendLog("ok", QString("Configuration appliquée : %1 hydrophone(s), %2 amplificateur(s)").arg(cfg.hydrophones.size()).arg(cfg.amplifiers.size()));
    if (!m_connConfig.offline) m_hw->applyConfig(m_cfg);
}

void MainWindow::openConfig()
{
    m_configWindow->show(); m_configWindow->raise(); m_configWindow->activateWindow();
}

void MainWindow::toggleTheme()
{
    auto& sm = StyleManager::instance();
    sm.setTheme(sm.isDark() ? StyleManager::Light : StyleManager::Dark);
    setStyleSheet(sm.mainStyleSheet());
    if (m_configWindow) m_configWindow->setStyleSheet(sm.configStyleSheet());
    update();
}

// ----------------------------------------------------------------------------- barre des voies
void MainWindow::setupVoieBar() { rebuildVoieBar(); }

void MainWindow::rebuildVoieBar()
{
    auto* layout = qobject_cast<QHBoxLayout*>(ui->voieBar->layout());
    while (layout->count() > 0) { auto* it = layout->takeAt(0); if (auto* w = it->widget()) w->deleteLater(); delete it; }
    m_voieBtns.clear();
    auto addLbl = [&](const QString& t) { auto* l = new QLabel(t); l->setStyleSheet("color:#6e7681;font-size:13px;font-weight:bold;"); layout->addWidget(l); };
    auto addBtn = [&](const QString& key, const QString& label) {
        const QColor qc = GraphWidget::colorFor(key);
        const QString color = qc.name();
        const QString bgChecked = QString("rgba(%1,%2,%3,40)").arg(qc.red()).arg(qc.green()).arg(qc.blue());
        auto* btn = new QPushButton(label); btn->setCheckable(true);
        btn->setChecked(m_voies.value(key, true));
        btn->setFixedHeight(26); btn->setMinimumWidth(44);
        btn->setStyleSheet(QString("QPushButton{border:1px solid %1;color:%1;background:transparent;font-size:13px;font-weight:bold;border-radius:2px;padding:2px 8px;}"
                                   "QPushButton:checked{background:%2;}QPushButton:!checked{color:#444c56;border-color:#444c56;}").arg(color, bgChecked));
        btn->setProperty("voieKey", key);
        connect(btn, &QPushButton::toggled, this, &MainWindow::onToggleVoie);
        layout->addWidget(btn); m_voieBtns[key] = btn;
    };
    auto addSep = [&]() { auto* s = new QFrame(); s->setFrameShape(QFrame::VLine); s->setStyleSheet("color:#30363d;"); layout->addWidget(s); };
    if (m_cfg.hydrophones.isEmpty() && m_cfg.amplifiers.isEmpty()) {
        addLbl("Aucune voie : ouvrir CONFIG puis VALIDER"); layout->addStretch(); return;
    }
    addLbl("HYDROS :"); for (const auto& h : m_cfg.hydrophones) addBtn(QString("h%1").arg(h.id), QString("H%1").arg(h.id));
    addSep();
    addLbl("AMPLIS :"); for (const auto& a : m_cfg.amplifiers)  addBtn(QString("a%1").arg(a.id), QString("A%1").arg(a.id));
    addSep();
    const double fs = m_cfg.hydrophones.isEmpty() ? 0 : m_cfg.hydrophones.first().sampleRate;
    const double mli = m_cfg.amplifiers.isEmpty() ? 0 : m_cfg.amplifiers.first().carrierFreq;
    auto* info = new QLabel(QString("Fs <span style='color:#00d4ff;font-weight:bold'>%1 kHz</span> &nbsp;|&nbsp; Porteuse MLI <span style='color:#00e676;font-weight:bold'>%2 kHz</span>")
                            .arg(fs / 1000.0, 0, 'f', 1).arg(mli / 1000.0, 0, 'f', 1));
    info->setTextFormat(Qt::RichText); info->setStyleSheet("font-size:13px;color:#8b949e;");
    layout->addWidget(info); layout->addStretch();
}

void MainWindow::onToggleVoie()
{
    auto* btn = qobject_cast<QPushButton*>(sender()); if (!btn) return;
    const QString key = btn->property("voieKey").toString(); const bool on = btn->isChecked();
    m_voies[key] = on;
    for (auto* g : {m_graphTime, m_graphFFT, m_graphLevels, m_graphSpec}) g->setVoieActive(key, on);
}

// ----------------------------------------------------------------------------- graphes / barre laterale
void MainWindow::setupCharts()
{
    auto* grid = qobject_cast<QGridLayout*>(ui->chartsContainer->layout());
    auto makeBox = [](const QString& title, GraphWidget* gw) {
        auto* box = new QGroupBox(title);
        auto* l = new QVBoxLayout(box); l->setContentsMargins(4, 18, 4, 4); l->addWidget(gw); return box; };
    m_graphTime   = new GraphWidget(GraphWidget::TimeDomain,  this);
    m_graphFFT    = new GraphWidget(GraphWidget::FFT,         this);
    m_graphLevels = new GraphWidget(GraphWidget::Levels,      this);
    m_graphSpec   = new GraphWidget(GraphWidget::Spectrogram, this);
    grid->addWidget(makeBox("Signal temporel — hydrophones (V)", m_graphTime), 0, 0);
    grid->addWidget(makeBox("Spectrogramme — fréq. / temps",     m_graphSpec), 0, 1);
    grid->addWidget(makeBox("FFT (dBV)",                         m_graphFFT),  1, 0);
    grid->addWidget(makeBox("Niveaux — RMS dBV",                 m_graphLevels), 1, 1);
    grid->setRowStretch(0, 1); grid->setRowStretch(1, 1);
}

void MainWindow::setupSidebar()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:#161b22;border-left:1px solid #30363d;");
    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setSpacing(0); layout->setContentsMargins(0, 0, 0, 0);

    auto addListBox = [&](const QString& title, QVBoxLayout*& inner) {
        auto* box = new QGroupBox(title);
        inner = new QVBoxLayout(box); inner->setSpacing(3); inner->setContentsMargins(8, 18, 8, 6);
        layout->addWidget(box); };
    auto addMini = [&](const QString& title, GraphWidget*& gw, QGroupBox*& box, const QColor& col, const QString& unit) {
        box = new QGroupBox(title); box->setFixedHeight(96);
        auto* l = new QVBoxLayout(box); l->setContentsMargins(2, 16, 2, 2);
        gw = new GraphWidget(GraphWidget::MiniPlot, this);
        gw->setMiniPlotColor(col); gw->setMiniPlotUnit(unit);
        l->addWidget(gw); layout->addWidget(box); };

    addListBox("Amplificateurs", m_ampLayout);
    addListBox("Hydrophones", m_hydroLayout);
    addMini("Température (non assignée)", m_miniTemp1, m_boxT1, QColor("#00e676"), "°C");
    addMini("Température (non assignée)", m_miniTemp2, m_boxT2, QColor("#ffaa00"), "°C");
    addMini("Tension (non assignée)",     m_miniV1,    m_boxV1, QColor("#00d4ff"), "V");
    addMini("Tension (non assignée)",     m_miniV2,    m_boxV2, QColor("#89ddff"), "V");
    addMini("Courant (non assigné)",      m_miniI1,    m_boxI1, QColor("#ffaa00"), "A");
    addMini("Courant (non assigné)",      m_miniI2,    m_boxI2, QColor("#c792ea"), "A");
    QVBoxLayout* svL = nullptr; addListBox("Mesure Sv", svL);
    auto* hl = new QHBoxLayout();
    auto* l1 = new QLabel("État"); l1->setStyleSheet("font-size:13px;color:#8b949e;");
    auto* l2 = new QLabel("NON DÉMARRÉE"); l2->setStyleSheet("font-weight:bold;font-size:13px;color:#8b949e;"); l2->setAlignment(Qt::AlignRight);
    hl->addWidget(l1); hl->addWidget(l2); svL->addLayout(hl);

    layout->addStretch();
    scroll->setWidget(container);
    auto* sl = qobject_cast<QVBoxLayout*>(ui->sidebar->layout());
    ui->sidebar->setStyleSheet("background:#161b22;border-left:1px solid #30363d;");
    sl->addWidget(scroll);
    rebuildSidebarLists();
}

static void clearLayout(QLayout* l)
{
    while (l->count() > 0) {
        QLayoutItem* it = l->takeAt(0);
        if (auto* w = it->widget()) w->deleteLater();
        if (auto* sub = it->layout()) { clearLayout(sub); delete sub; }
        delete it;
    }
}

void MainWindow::rebuildSidebarLists()
{
    clearLayout(m_ampLayout); clearLayout(m_hydroLayout); m_stateLbl.clear();
    auto addRow = [&](QVBoxLayout* lay, const QString& key, const QString& name) {
        auto* hl = new QHBoxLayout();
        auto* l = new QLabel(name); l->setStyleSheet("font-size:13px;color:#8b949e;");
        auto* v = new QLabel("—"); v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hl->addWidget(l); hl->addWidget(v); lay->addLayout(hl); m_stateLbl[key] = v; };
    for (const auto& a : m_cfg.amplifiers)  addRow(m_ampLayout,   QString("a%1").arg(a.id), QString("AMP %1").arg(a.id));
    for (const auto& h : m_cfg.hydrophones) addRow(m_hydroLayout, QString("h%1").arg(h.id), QString("Hydro %1 (voie %2)").arg(h.id).arg(h.channel));
    if (m_cfg.amplifiers.isEmpty())  { auto* l = new QLabel("aucun"); l->setStyleSheet("color:#6e7681;"); m_ampLayout->addWidget(l); }
    if (m_cfg.hydrophones.isEmpty()) { auto* l = new QLabel("aucun"); l->setStyleSheet("color:#6e7681;"); m_hydroLayout->addWidget(l); }
}

void MainWindow::refreshSidebarStatus()
{
    auto set = [&](const QString& key, const QString& text, const QString& color) {
        if (auto* l = m_stateLbl.value(key)) { l->setText(text); l->setStyleSheet(QString("font-weight:bold;font-size:13px;color:%1;").arg(color)); } };
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto& a : m_cfg.amplifiers) {
        const QString k = QString("a%1").arg(a.id);
        if (m_hw->emitting() && m_hw->isAmpActive(a.id)) set(k, "ÉMISSION", "#00e676");
        else if (m_hw->isAmpActive(a.id))               set(k, "ACTIVÉE", "#00d4ff");
        else if (m_hw->isMliReady(a.id))                set(k, "MLI PRÊTE", "#ffaa00");
        else                                            set(k, "INACTIVE", "#8b949e");
    }
    for (const auto& h : m_cfg.hydrophones) {
        const QString k = QString("h%1").arg(h.id);
        if (!m_hw->acquiring())                                      set(k, "ARRÊTÉ", "#8b949e");
        else if (now - m_lastData.value(k, 0) > STALE_MS)            set(k, "SANS SIGNAL", "#ff4444");
        else if (m_sat.value(k))                                     set(k, "SATURÉ", "#ff4444");
        else                                                         set(k, "ACQ", "#00e676");
    }
}

// ----------------------------------------------------------------------------- journal / export / diagnostic
void MainWindow::setupLogView() { ui->logView->setReadOnly(true); ui->logView->document()->setMaximumBlockCount(500); }

void MainWindow::appendLog(const QString& level, const QString& msg)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    const QString color = level == "ok" ? "#00e676" : level == "warn" ? "#ffaa00" : level == "info" ? "#00d4ff" : "#ff4444";
    ui->logView->append(QString("<span style='color:#6e7681;font-size:12px'>%1</span>&nbsp;&nbsp;"
                                "<span style='color:%2;font-size:13px'>%3</span>")
                        .arg(ts, color, msg.toHtmlEscaped()));
    QFile f(m_logPath);                         // journal persistant (un fichier par jour)
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream(&f) << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " [" << level.toUpper() << "] " << msg << "\n";
    }
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(m_connConfig.offline ? "HORS LIGNE — aucune donnée réelle"
        : QString("Connecté — %1 · données réelles NI · journal : %2").arg(m_connConfig.chassis, m_logPath));
}

void MainWindow::exportCSV()
{
    if (m_history.empty()) { QMessageBox::information(this, "Export", "Aucune donnée à exporter (temperatures, tensions, courants)."); return; }
    const QString path = QFileDialog::getSaveFileName(this, "Exporter les données", "acoustimeter_export.csv", "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::critical(this, "Export", "Impossible d'écrire : " + path); return; }
    QTextStream ts(&f);
    ts << "timestamp;voie;type;valeur\n";
    for (const auto& r : m_history)
        ts << QDateTime::fromMSecsSinceEpoch(r.ms).toString("yyyy-MM-dd hh:mm:ss.zzz") << ';' << r.label << ';' << r.kind << ';' << QString::number(r.value, 'g', 10) << '\n';
    f.close();
    QMessageBox::information(this, "Export", QString("%1 lignes exportées :\n%2").arg(m_history.size()).arg(path));
}

void MainWindow::showDiagnostic()
{
    QString t = QString("NI-DAQmx : %1\n\n").arg(HardwareManager::niCompiled() ? "compilé" : "NON compilé");
    if (m_hw->devices().isEmpty()) t += "Aucune carte détectée.\n";
    for (const auto& d : m_hw->devices()) t += QString("%1 : %2 (S/N %3)\n").arg(d.name, d.productType, d.serial);
    t += QString("\nCarte numérique MLI : %1\n").arg(m_hw->digitalDevice().isEmpty() ? "absente" : m_hw->digitalDevice());
    t += QString("Journal : %1").arg(m_logPath);
    QMessageBox::information(this, "Diagnostic matériel", t);
}
