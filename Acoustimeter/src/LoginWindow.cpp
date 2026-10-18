#include "LoginWindow.h"
#include "ui_LoginWindow.h"
#include "StyleManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidgetItem>
#include <QApplication>
#include <QScreen>

LoginWindow::LoginWindow(HardwareManager* hw, QWidget* parent)
    : QDialog(parent), ui(new Ui::LoginWindow), m_hw(hw)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setStyleSheet(StyleManager::instance().loginStyleSheet());
    setupUi();
    setupConnections();
    updateVisaString();
}
LoginWindow::~LoginWindow() { delete ui; }

void LoginWindow::setupUi()
{
    auto* lay = new QHBoxLayout(ui->profileContainer);
    lay->setSpacing(8); lay->setContentsMargins(0,0,0,0);
    m_btnOp    = new QPushButton("👤  Opérateur",      ui->profileContainer);
    m_btnAdmin = new QPushButton("🔑  Administrateur", ui->profileContainer);
    m_btnOp->setCheckable(true); m_btnOp->setChecked(true); m_btnAdmin->setCheckable(true);
    m_btnOp->setMinimumHeight(32); m_btnAdmin->setMinimumHeight(32);
    m_btnOp->setStyleSheet("border-color:#00d4ff;color:#00d4ff;background:#001a2e;");
    lay->addWidget(m_btnOp); lay->addWidget(m_btnAdmin);
    ui->btnConnect->setProperty("cssClass", "connect");
    ui->btnConnect->setText("▶  Détecter le matériel NI");

    m_btnOffline = new QPushButton("Ouvrir sans matériel (aucune mesure)", this);
    m_btnOffline->setMinimumHeight(30); m_btnOffline->setVisible(false);
    ui->bodyLayout->addWidget(m_btnOffline);
    connect(m_btnOffline, &QPushButton::clicked, this, [this]() {
        fillConfig(); m_config.offline = true; emit connectionEstablished(m_config); accept(); });

    m_timeout = new QTimer(this); m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, &LoginWindow::onTimeout);
    ui->progressWidget->setVisible(false);
    if (auto* s = QApplication::primaryScreen()) move(s->geometry().center() - rect().center());
}

void LoginWindow::setupConnections()
{
    connect(ui->comboChassis,   &QComboBox::currentIndexChanged, this, [this](int){ updateVisaString(); });
    connect(ui->spinSlot,       &QSpinBox::valueChanged,         this, [this](int){ updateVisaString(); });
    connect(ui->comboInterface, &QComboBox::currentIndexChanged, this, [this](int){ updateVisaString(); });
    connect(ui->btnConnect,     &QPushButton::clicked,           this, &LoginWindow::onConnectClicked);
    connect(m_btnOp,            &QPushButton::clicked,           this, &LoginWindow::onProfileToggled);
    connect(m_btnAdmin,         &QPushButton::clicked,           this, &LoginWindow::onProfileToggled);
    connect(m_hw, &HardwareManager::devicesDetected, this, &LoginWindow::onDevices);
}

void LoginWindow::updateVisaString()
{
    // Information seulement : NI-DAQmx enumere les cartes par leur nom NI MAX (Dev1, Dev2...).
    ui->visaPlaceholder->setText(QString("%1 · slot %2 · %3  (info — détection par NI-DAQmx)")
        .arg(ui->comboChassis->currentText()).arg(ui->spinSlot->value()).arg(ui->comboInterface->currentText()));
}

void LoginWindow::onProfileToggled()
{
    const bool adminSel = (sender() == m_btnAdmin);
    m_btnOp->setChecked(!adminSel); m_btnAdmin->setChecked(adminSel);
    m_btnOp->setStyleSheet(adminSel ? "" : "border-color:#00d4ff;color:#00d4ff;background:#001a2e;");
    m_btnAdmin->setStyleSheet(adminSel ? "border-color:#c792ea;color:#c792ea;background:#1a0a2e;" : "");
    ui->pwdWidget->setVisible(adminSel);
    adjustSize();
}

void LoginWindow::addStep(const QString& text, bool ok, bool warn)
{
    auto* item = new QListWidgetItem((ok ? "✓  " : warn ? "⚠  " : "✗  ") + text);
    item->setForeground(QColor(ok ? "#00e676" : warn ? "#ffaa00" : "#ff4444"));
    ui->listSteps->addItem(item); ui->listSteps->scrollToBottom();
}

void LoginWindow::fillConfig()
{
    m_config.chassis    = ui->comboChassis->currentText();
    m_config.slot       = ui->spinSlot->value();
    m_config.interface_ = ui->comboInterface->currentText();
    m_config.profile    = m_btnAdmin->isChecked() ? UserProfile::Administrator : UserProfile::Operator;
    m_config.offline    = false;
}

void LoginWindow::onConnectClicked()
{
    if (m_connected) { fillConfig(); emit connectionEstablished(m_config); accept(); return; }
    if (m_waiting) return;
    ui->btnConnect->setEnabled(false);
    m_btnOffline->setVisible(false);
    ui->progressWidget->setVisible(true);
    ui->listSteps->clear(); ui->progressBar->setValue(10);
    ui->lblProgressPct->setText("10%"); ui->lblProgressMsg->setText("Vérification du pilote NI-DAQmx...");
    ui->lblStatus->setText("Détection en cours..."); ui->lblStatus->setStyleSheet("color:#ffaa00;");

    const bool drv = HardwareManager::niCompiled();
    addStep(drv ? "Application compilée avec NI-DAQmx" : "Application compilée SANS NI-DAQmx (ACOUSTIMETER_WITH_NI=OFF)", drv);
    if (!drv) { finishFail("Build sans NI-DAQmx : aucune mesure possible"); return; }

    m_waiting = true;
    ui->progressBar->setValue(40); ui->lblProgressPct->setText("40%");
    ui->lblProgressMsg->setText("Énumération des cartes NI (NI-DAQmx)...");
    m_timeout->start(20000);
    m_hw->detect();
}

void LoginWindow::onTimeout()
{
    if (!m_waiting) return;
    m_waiting = false;
    addStep("Pas de réponse de NI-DAQmx après 20 s", false);
    finishFail("NI-DAQmx ne répond pas (NI MAX voit-il le châssis ?)");
}

void LoginWindow::onDevices(const QVector<NiDevice>& d)
{
    if (!m_waiting) return;
    m_waiting = false; m_timeout->stop();
    ui->progressBar->setValue(80); ui->lblProgressPct->setText("80%");
    for (const auto& x : d) addStep(QString("%1 : %2  (S/N %3)").arg(x.name, x.productType, x.serial), true);
    if (d.isEmpty()) { addStep("Aucune carte NI détectée", false); finishFail("Aucune carte NI détectée — vérifier la liaison PXIe-8301 / NI MAX"); return; }

    auto has = [&](const QString& s) { for (const auto& x : d) if (x.productType.contains(s)) return true; return false; };
    addStep("PXIe-4353 (températures)",       has("4353"), true);
    addStep("PXIe-4480 (hydrophones)",        has("4480"), true);
    addStep("Carte numérique MLI (6570/6537)", has("6570") || has("6537"), true);
    addStep("PXIe-5108 (oscilloscopes)",      has("5108"), true);
    finishOk(d.size());
}

void LoginWindow::finishOk(int n)
{
    ui->progressBar->setValue(100); ui->lblProgressPct->setText("100%"); ui->lblProgressMsg->setText("Matériel détecté");
    ui->lblStatus->setText(QString("Connecté — %1 carte(s) NI détectée(s)").arg(n));
    ui->lblStatus->setStyleSheet("color:#00e676;");
    m_connected = true;
    ui->btnConnect->setEnabled(true); ui->btnConnect->setText("▶  Lancer ACOUSTIMETER");
}

void LoginWindow::finishFail(const QString& msg)
{
    ui->lblStatus->setText(msg); ui->lblStatus->setStyleSheet("color:#ff4444;");
    ui->lblProgressMsg->setText("Échec de la détection");
    m_connected = false;
    ui->btnConnect->setEnabled(true); ui->btnConnect->setText("↻  Réessayer");
    m_btnOffline->setVisible(true);
}
