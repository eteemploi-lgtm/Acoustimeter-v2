#include "HydroWidget.h"
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
#include <QFileDialog>
#include <QMessageBox>

HydroWidget::HydroWidget(int index, QWidget* parent) : QScrollArea(parent), m_index(index)
{ setWidgetResizable(true); setFrameShape(QFrame::NoFrame); setupUi(); }

void HydroWidget::setIndex(int index)
{
    m_index = index;
    m_grpV->setTitle(QString("Hydro %1 — Voie && acquisition").arg(m_index));
}

void HydroWidget::refreshModels()
{
    const QString cur = m_comboModel->currentText();
    m_comboModel->clear(); m_comboModel->addItems(HydroModels::list());
    const int i = m_comboModel->findText(cur); if (i >= 0) m_comboModel->setCurrentIndex(i);
}

void HydroWidget::setupUi()
{
    auto* c = new QWidget(this); setWidget(c);
    auto* ml = new QVBoxLayout(c); ml->setContentsMargins(10,10,10,10); ml->setSpacing(8);

    m_grpV = new QGroupBox(QString("Hydro %1 — Voie && acquisition").arg(m_index));
    auto* g = new QGridLayout(m_grpV); g->setSpacing(6);
    m_spinChannel = new QSpinBox(); m_spinChannel->setRange(1, 6); m_spinChannel->setValue(qMin(m_index, 6));
    m_spinChannel->setToolTip("Voie physique de la PXIe-4480 (1 a 6)");
    m_spinGain = new QDoubleSpinBox(); m_spinGain->setRange(0, 80); m_spinGain->setValue(20); m_spinGain->setSuffix(" dB");
    m_spinFs = new QSpinBox(); m_spinFs->setRange(1000, 204800); m_spinFs->setValue(96000); m_spinFs->setSuffix(" Hz");
    m_spinFs->setToolTip("PXIe-4480 : jusqu'a 204,8 kHz ; NI-DAQmx peut ajuster a la cadence supportee la plus proche");
    m_comboDyn = new QComboBox(); m_comboDyn->addItems({"±1V","±2V","±5V","±10V"}); m_comboDyn->setCurrentIndex(3);
    m_spinDist = new QDoubleSpinBox(); m_spinDist->setRange(0, 1000); m_spinDist->setValue(1.0); m_spinDist->setSuffix(" m"); m_spinDist->setDecimals(2);
    m_spinDepth = new QDoubleSpinBox(); m_spinDepth->setRange(0, 1000); m_spinDepth->setValue(0.5); m_spinDepth->setSuffix(" m"); m_spinDepth->setDecimals(2);
    g->addWidget(new QLabel("N° voie (4480)"),0,0); g->addWidget(m_spinChannel,0,1);
    g->addWidget(new QLabel("Gain préampli"),0,2);  g->addWidget(m_spinGain,0,3);
    g->addWidget(new QLabel("Fréquence éch."),1,0); g->addWidget(m_spinFs,1,1);
    g->addWidget(new QLabel("Dynamique"),1,2);      g->addWidget(m_comboDyn,1,3);
    g->addWidget(new QLabel("Distance h-t"),2,0);   g->addWidget(m_spinDist,2,1);
    g->addWidget(new QLabel("Profondeur"),2,2);     g->addWidget(m_spinDepth,2,3);
    ml->addWidget(m_grpV);

    auto* grpM = new QGroupBox("Hydrophone — Sélection");
    auto* gm = new QGridLayout(grpM); gm->setSpacing(6);
    m_comboModel = new QComboBox(); m_comboModel->addItems(HydroModels::list());
    m_editSens = new QLineEdit(); m_editSens->setPlaceholderText("fichier de sensibilité (.csv)");
    auto* btnB = new QPushButton("Importer..."); btnB->setProperty("cssClass","primary");
    connect(btnB, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getOpenFileName(this, "Sensibilité", "", "CSV (*.csv);;Tous (*.*)");
        if (!f.isEmpty()) m_editSens->setText(f);
    });
    gm->addWidget(new QLabel("Modèle"),0,0); gm->addWidget(m_comboModel,0,1,1,3);
    gm->addWidget(new QLabel("Fichier sensibilité"),1,0); gm->addWidget(m_editSens,1,1,1,2); gm->addWidget(btnB,1,3);
    ml->addWidget(grpM);

    auto* br = new QHBoxLayout();
    auto* bc = new QPushButton("Créer");    bc->setProperty("cssClass","primary");
    auto* bm = new QPushButton("Modifier / Appliquer"); bm->setProperty("cssClass","warning");
    auto* bd = new QPushButton("Supprimer");bd->setProperty("cssClass","danger");
    connect(bc, &QPushButton::clicked, this, &HydroWidget::createRequested);
    connect(bm, &QPushButton::clicked, this, &HydroWidget::applyRequested);
    connect(bd, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, "Supprimer", QString("Supprimer Hydro %1 ?").arg(m_index)) == QMessageBox::Yes)
            emit deleteRequested(m_index);
    });
    br->addWidget(bc); br->addWidget(bm); br->addWidget(bd); br->addStretch();
    ml->addLayout(br); ml->addStretch();
}

HydrophoneConfig HydroWidget::config() const
{
    HydrophoneConfig h;
    h.id = m_index; h.channel = m_spinChannel->value(); h.gain = m_spinGain->value();
    h.sampleRate = m_spinFs->value();
    QString d = m_comboDyn->currentText(); d.remove(QChar(0x00B1)); d.remove('V');
    h.dynamic = d.toDouble();
    h.distance = m_spinDist->value(); h.depth = m_spinDepth->value();
    h.model = m_comboModel->currentText(); h.sensitivityFile = m_editSens->text();
    return h;
}

void HydroWidget::loadConfig(const HydrophoneConfig& h)
{
    m_spinChannel->setValue(h.channel); m_spinGain->setValue(h.gain); m_spinFs->setValue(h.sampleRate);
    const int di = m_comboDyn->findText(QString("±%1V").arg(h.dynamic, 0, 'g', 3));
    if (di >= 0) m_comboDyn->setCurrentIndex(di);
    m_spinDist->setValue(h.distance); m_spinDepth->setValue(h.depth);
    const int mi = m_comboModel->findText(h.model);
    if (mi >= 0) m_comboModel->setCurrentIndex(mi); else if (!h.model.isEmpty()) m_comboModel->setCurrentText(h.model);
    m_editSens->setText(h.sensitivityFile);
}
