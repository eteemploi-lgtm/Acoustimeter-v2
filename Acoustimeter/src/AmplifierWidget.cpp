#include "AmplifierWidget.h"
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
#include <QPainter>
#include <QPainterPath>
#include <QFileDialog>
#include <QMessageBox>
#include <QStyle>

// Apercu de la MLI REELLEMENT calculee (premiers echantillons de la sequence envoyee a la carte).
class MliCanvas : public QWidget {
public:
    explicit MliCanvas(QWidget* p) : QWidget(p) { setMinimumHeight(56); setMaximumHeight(56); }
    void setBits(const QVector<quint8>& b) { m_bits = b; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#1f2937"));
        if (m_bits.isEmpty()) {
            p.setPen(QColor("#8b949e")); p.drawText(rect(), Qt::AlignCenter, "MLI non générée");
            return;
        }
        p.setPen(QPen(QColor("#00e676"), 1.2)); QPainterPath path;
        const int W = width(), H = height(), n = m_bits.size();
        for (int i = 0; i < n; ++i) {
            const double x = double(i) / n * W, y = m_bits[i] ? H * 0.15 : H * 0.85;
            if (i == 0) path.moveTo(x, y); else { path.lineTo(x, path.currentPosition().y()); path.lineTo(x, y); }
        }
        path.lineTo(W, path.currentPosition().y());
        p.drawPath(path);
    }
private:
    QVector<quint8> m_bits;
};

AmplifierWidget::AmplifierWidget(int index, QWidget* parent) : QScrollArea(parent), m_index(index)
{ setWidgetResizable(true); setFrameShape(QFrame::NoFrame); setupUi(); }

void AmplifierWidget::setIndex(int index)
{
    m_index = index;
    m_grpSig->setTitle(QString("Amp %1 — Signal && filtre").arg(m_index));
}

void AmplifierWidget::setupUi()
{
    auto* c = new QWidget(this); setWidget(c);
    auto* ml = new QVBoxLayout(c); ml->setContentsMargins(10,10,10,10); ml->setSpacing(8);
    setupSignalGroup(ml); setupCarrierGroup(ml); setupTempGroup(ml); setupVoltGroup(ml); setupCurrGroup(ml);
    setupActions(ml);
    ml->addStretch();
}

void AmplifierWidget::setupSignalGroup(QVBoxLayout* l)
{
    m_grpSig = new QGroupBox(QString("Amp %1 — Signal && filtre").arg(m_index));
    auto* g = new QGridLayout(m_grpSig); g->setSpacing(6);
    m_editWav = new QLineEdit(); m_editWav->setPlaceholderText("fichier .wav du signal");
    m_editFilter = new QLineEdit(); m_editFilter->setPlaceholderText("filtre FIR (.csv de coefficients) — optionnel");
    auto* bw = new QPushButton("Charger WAV"); bw->setProperty("cssClass","primary");
    auto* bf = new QPushButton("Charger filtre"); bf->setProperty("cssClass","primary");
    connect(bw, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getOpenFileName(this, "Signal WAV", "", "WAV (*.wav)");
        if (!f.isEmpty()) { m_editWav->setText(f); invalidateMli(); } });
    connect(bf, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getOpenFileName(this, "Filtre", "", "CSV (*.csv);;Tous (*.*)");
        if (!f.isEmpty()) { m_editFilter->setText(f); invalidateMli(); } });
    g->addWidget(new QLabel("Fichier WAV"),0,0); g->addWidget(m_editWav,0,1); g->addWidget(bw,0,2);
    g->addWidget(new QLabel("Filtre"),1,0);      g->addWidget(m_editFilter,1,1); g->addWidget(bf,1,2);
    l->addWidget(m_grpSig);
}

void AmplifierWidget::setupCarrierGroup(QVBoxLayout* l)
{
    auto* grp = new QGroupBox(QString("Porteuse MLI"));
    auto* ml2 = new QVBoxLayout(grp); ml2->setSpacing(6);
    auto* g = new QGridLayout(); g->setSpacing(6);
    m_comboCarrier = new QComboBox(); m_comboCarrier->addItems({"Triangle","Dent de scie"});
    m_spinCFreq = new QDoubleSpinBox(); m_spinCFreq->setRange(1, 10e6); m_spinCFreq->setValue(38400); m_spinCFreq->setSuffix(" Hz");
    m_spinAmp = new QDoubleSpinBox(); m_spinAmp->setRange(0, 2); m_spinAmp->setValue(1.0); m_spinAmp->setDecimals(2); m_spinAmp->setSingleStep(0.05);
    m_spinOff = new QDoubleSpinBox(); m_spinOff->setRange(-1, 1); m_spinOff->setValue(0.0); m_spinOff->setDecimals(2); m_spinOff->setSingleStep(0.05);
    m_editPin = new QLineEdit(QString("MLI%1").arg(m_index));
    m_editPin->setToolTip("Nom de pin (carte numérique) pilotant cet amplificateur");
    g->addWidget(new QLabel("Type porteuse"),0,0); g->addWidget(m_comboCarrier,0,1);
    g->addWidget(new QLabel("Fréquence"),0,2);     g->addWidget(m_spinCFreq,0,3);
    g->addWidget(new QLabel("Amplitude (0→2)"),1,0); g->addWidget(m_spinAmp,1,1);
    g->addWidget(new QLabel("Offset (−1→+1)"),1,2);  g->addWidget(m_spinOff,1,3);
    g->addWidget(new QLabel("Pin MLI"),2,0);         g->addWidget(m_editPin,2,1);
    ml2->addLayout(g);

    auto* br = new QHBoxLayout();
    auto* bm = new QPushButton("Générer MLI"); bm->setProperty("cssClass","primary");
    m_btnActive = new QPushButton("Activer voie"); m_btnActive->setProperty("cssClass","warning"); m_btnActive->setCheckable(true);
    m_lblMli = new QLabel("MLI non générée"); m_lblMli->setStyleSheet("color:#8b949e;");
    connect(bm, &QPushButton::clicked, this, [this]() { emit generateMliRequested(config()); });
    connect(m_btnActive, &QPushButton::toggled, this, [this](bool on) {
        if (on && !m_mliReady) {
            QMessageBox::warning(this, "Activer voie", "Générer la MLI avant d'activer la voie.");
            QSignalBlocker b(m_btnActive); m_btnActive->setChecked(false); return;
        }
        m_btnActive->setText(on ? "Désactiver voie" : "Activer voie");
        m_btnActive->setProperty("cssClass", on ? "danger" : "warning");
        m_btnActive->style()->unpolish(m_btnActive); m_btnActive->style()->polish(m_btnActive);
        emit voieToggled(m_index, on);
    });
    br->addWidget(bm); br->addWidget(m_btnActive); br->addWidget(m_lblMli, 1);
    ml2->addLayout(br);
    m_canvas = new MliCanvas(this); ml2->addWidget(m_canvas);
    for (auto* w : {static_cast<QWidget*>(m_comboCarrier), static_cast<QWidget*>(m_spinCFreq),
                    static_cast<QWidget*>(m_spinAmp), static_cast<QWidget*>(m_spinOff)}) {
        if (auto* d = qobject_cast<QDoubleSpinBox*>(w)) connect(d, &QDoubleSpinBox::valueChanged, this, [this](double){ invalidateMli(); });
        if (auto* c = qobject_cast<QComboBox*>(w)) connect(c, &QComboBox::currentIndexChanged, this, [this](int){ invalidateMli(); });
    }
    l->addWidget(grp);
}

void AmplifierWidget::invalidateMli()
{
    if (!m_mliReady) return;
    m_mliReady = false;
    m_lblMli->setText("Paramètres modifiés : régénérer la MLI"); m_lblMli->setStyleSheet("color:#ffaa00;");
    if (m_btnActive->isChecked()) { m_btnActive->setChecked(false); }   // emet voieToggled(false)
}

void AmplifierWidget::setMliResult(bool ok, const QString& message, const QVector<quint8>& preview)
{
    m_mliReady = ok;
    m_lblMli->setText(message);
    m_lblMli->setStyleSheet(ok ? "color:#00e676;" : "color:#ff4444;");
    m_canvas->setBits(ok ? preview : QVector<quint8>());
}

void AmplifierWidget::setupTempGroup(QVBoxLayout* l)
{
    auto* grp = new QGroupBox("Température (PXIe-4353)");
    auto* g = new QGridLayout(grp); g->setSpacing(6);
    m_spinTProbes = new QSpinBox(); m_spinTProbes->setRange(0, 32); m_spinTProbes->setValue(2);
    m_comboTType = new QComboBox(); m_comboTType->addItems({"K","J","T","E","N","R","S","B"});
    m_spinTFreq = new QDoubleSpinBox(); m_spinTFreq->setRange(0.01, 100); m_spinTFreq->setValue(1); m_spinTFreq->setSuffix(" Hz");
    m_editTLabel = new QLineEdit(QString("T_AMP%1").arg(m_index));
    g->addWidget(new QLabel("Nb sondes"),0,0); g->addWidget(m_spinTProbes,0,1);
    g->addWidget(new QLabel("Type TC"),0,2);   g->addWidget(m_comboTType,0,3);
    g->addWidget(new QLabel("Freq. acq."),1,0); g->addWidget(m_spinTFreq,1,1);
    g->addWidget(new QLabel("Label"),1,2);      g->addWidget(m_editTLabel,1,3);
    l->addWidget(grp);
}

void AmplifierWidget::setupVoltGroup(QVBoxLayout* l)
{
    auto* grp = new QGroupBox("Tension");
    auto* g = new QGridLayout(grp); g->setSpacing(6);
    m_spinVCh = new QSpinBox(); m_spinVCh->setRange(0, 16); m_spinVCh->setValue(0);
    m_comboVDiv = new QComboBox(); m_comboVDiv->addItems({"1/10","1/100","1/1000"});
    m_spinVFreq = new QDoubleSpinBox(); m_spinVFreq->setRange(0.01, 1000); m_spinVFreq->setValue(10); m_spinVFreq->setSuffix(" Hz");
    m_editVLabel = new QLineEdit(QString("V_AMP%1").arg(m_index));
    g->addWidget(new QLabel("Nb voies"),0,0);  g->addWidget(m_spinVCh,0,1);
    g->addWidget(new QLabel("Division"),0,2);  g->addWidget(m_comboVDiv,0,3);
    g->addWidget(new QLabel("Freq. acq."),1,0); g->addWidget(m_spinVFreq,1,1);
    g->addWidget(new QLabel("Label"),1,2);      g->addWidget(m_editVLabel,1,3);
    l->addWidget(grp);
}

void AmplifierWidget::setupCurrGroup(QVBoxLayout* l)
{
    auto* grp = new QGroupBox("Courant");
    auto* g = new QGridLayout(grp); g->setSpacing(6);
    m_spinICh = new QSpinBox(); m_spinICh->setRange(0, 16); m_spinICh->setValue(0);
    m_comboIDiv = new QComboBox(); m_comboIDiv->addItems({"1/10","1/100","1/1000"});
    m_spinIFreq = new QDoubleSpinBox(); m_spinIFreq->setRange(0.01, 1000); m_spinIFreq->setValue(10); m_spinIFreq->setSuffix(" Hz");
    m_editILabel = new QLineEdit(QString("I_AMP%1").arg(m_index));
    g->addWidget(new QLabel("Nb voies"),0,0);  g->addWidget(m_spinICh,0,1);
    g->addWidget(new QLabel("Division"),0,2);  g->addWidget(m_comboIDiv,0,3);
    g->addWidget(new QLabel("Freq. acq."),1,0); g->addWidget(m_spinIFreq,1,1);
    g->addWidget(new QLabel("Label"),1,2);      g->addWidget(m_editILabel,1,3);
    l->addWidget(grp);
}

void AmplifierWidget::setupActions(QVBoxLayout* l)
{
    auto* br = new QHBoxLayout();
    auto* bc = new QPushButton("Créer");    bc->setProperty("cssClass","primary");
    auto* bm = new QPushButton("Modifier / Appliquer"); bm->setProperty("cssClass","warning");
    auto* bd = new QPushButton("Supprimer");bd->setProperty("cssClass","danger");
    connect(bc, &QPushButton::clicked, this, &AmplifierWidget::createRequested);
    connect(bm, &QPushButton::clicked, this, &AmplifierWidget::applyRequested);
    connect(bd, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, "Supprimer", QString("Supprimer Amplificateur %1 ?").arg(m_index)) == QMessageBox::Yes)
            emit deleteRequested(m_index);
    });
    br->addWidget(bc); br->addWidget(bm); br->addWidget(bd); br->addStretch();
    l->addLayout(br);
}

AmplifierConfig AmplifierWidget::config() const
{
    AmplifierConfig a;
    a.id = m_index; a.wavFile = m_editWav->text(); a.filterFile = m_editFilter->text();
    a.carrierType = m_comboCarrier->currentText(); a.carrierFreq = m_spinCFreq->value();
    a.amplitude = m_spinAmp->value(); a.offset = m_spinOff->value(); a.mliPin = m_editPin->text();
    a.tempProbeCount = m_spinTProbes->value(); a.tempType = m_comboTType->currentText();
    a.tempAcqFreq = m_spinTFreq->value(); a.tempLabel = m_editTLabel->text();
    a.voltChannelCount = m_spinVCh->value(); a.voltDivision = m_comboVDiv->currentText();
    a.voltAcqFreq = m_spinVFreq->value(); a.voltLabel = m_editVLabel->text();
    a.currChannelCount = m_spinICh->value(); a.currDivision = m_comboIDiv->currentText();
    a.currAcqFreq = m_spinIFreq->value(); a.currLabel = m_editILabel->text();
    return a;
}

void AmplifierWidget::loadConfig(const AmplifierConfig& a)
{
    m_editWav->setText(a.wavFile); m_editFilter->setText(a.filterFile);
    m_comboCarrier->setCurrentText(a.carrierType); m_spinCFreq->setValue(a.carrierFreq);
    m_spinAmp->setValue(a.amplitude); m_spinOff->setValue(a.offset);
    if (!a.mliPin.isEmpty()) m_editPin->setText(a.mliPin);
    m_spinTProbes->setValue(a.tempProbeCount); m_comboTType->setCurrentText(a.tempType);
    m_spinTFreq->setValue(a.tempAcqFreq); m_editTLabel->setText(a.tempLabel);
    m_spinVCh->setValue(a.voltChannelCount); m_comboVDiv->setCurrentText(a.voltDivision);
    m_spinVFreq->setValue(a.voltAcqFreq); m_editVLabel->setText(a.voltLabel);
    m_spinICh->setValue(a.currChannelCount); m_comboIDiv->setCurrentText(a.currDivision);
    m_spinIFreq->setValue(a.currAcqFreq); m_editILabel->setText(a.currLabel);
}
