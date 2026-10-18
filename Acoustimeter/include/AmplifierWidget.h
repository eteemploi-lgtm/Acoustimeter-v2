#pragma once
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVector>
#include "DataTypes.h"
class QLineEdit; class QSpinBox; class QDoubleSpinBox; class QComboBox; class QGroupBox;
class QPushButton; class QLabel; class MliCanvas;
class AmplifierWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit AmplifierWidget(int index, QWidget* parent = nullptr);
    AmplifierConfig config() const;
    void loadConfig(const AmplifierConfig& c);
    void setIndex(int index);
public slots:
    void setMliResult(bool ok, const QString& message, const QVector<quint8>& preview);
signals:
    void createRequested();
    void applyRequested();
    void deleteRequested(int index);
    void generateMliRequested(const AmplifierConfig& cfg);
    void voieToggled(int index, bool on);
private:
    void setupUi();
    void setupSignalGroup(QVBoxLayout* l);
    void setupCarrierGroup(QVBoxLayout* l);
    void setupTempGroup(QVBoxLayout* l);
    void setupVoltGroup(QVBoxLayout* l);
    void setupCurrGroup(QVBoxLayout* l);
    void setupActions(QVBoxLayout* l);
    void invalidateMli();
    int m_index;
    bool m_mliReady = false;
    QGroupBox* m_grpSig = nullptr;
    QLineEdit*      m_editWav=nullptr, *m_editFilter=nullptr, *m_editPin=nullptr;
    QComboBox*      m_comboCarrier=nullptr;
    QDoubleSpinBox* m_spinCFreq=nullptr, *m_spinAmp=nullptr, *m_spinOff=nullptr;
    QPushButton*    m_btnActive=nullptr;
    QLabel*         m_lblMli=nullptr;
    MliCanvas*      m_canvas=nullptr;
    QSpinBox*       m_spinTProbes=nullptr;
    QComboBox*      m_comboTType=nullptr;
    QDoubleSpinBox* m_spinTFreq=nullptr;
    QLineEdit*      m_editTLabel=nullptr;
    QSpinBox*       m_spinVCh=nullptr;
    QComboBox*      m_comboVDiv=nullptr;
    QDoubleSpinBox* m_spinVFreq=nullptr;
    QLineEdit*      m_editVLabel=nullptr;
    QSpinBox*       m_spinICh=nullptr;
    QComboBox*      m_comboIDiv=nullptr;
    QDoubleSpinBox* m_spinIFreq=nullptr;
    QLineEdit*      m_editILabel=nullptr;
};
