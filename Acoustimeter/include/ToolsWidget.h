#pragma once
#include <QScrollArea>
#include <QVBoxLayout>
#include "DataTypes.h"
class QDoubleSpinBox; class QSpinBox; class QComboBox; class QLineEdit;
class ToolsWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit ToolsWidget(QWidget* parent = nullptr);
    SvConfig svConfig() const;
    void loadSv(const SvConfig& c);
signals:
    void hydroModelsChanged();
    void saveConfigRequested();
    void loadConfigRequested();
    void deleteConfigRequested();
    void exportConfigRequested();
    void importConfigRequested();
    void hardwareMappingChanged();
private:
    void setupUi();
    void setupHydroMgmt(QVBoxLayout* l);
    void setupConfigMgmt(QVBoxLayout* l);
    void setupHardware(QVBoxLayout* l);
    void setupSvParams(QVBoxLayout* l);
    void setupSvFilter(QVBoxLayout* l);
    QComboBox*      m_comboModels=nullptr;
    QSpinBox*       m_spinListenCh=nullptr;
    QComboBox*      m_comboHydro=nullptr;
    QDoubleSpinBox* m_spinGain=nullptr;
    QSpinBox*       m_spinFs=nullptr;
    QDoubleSpinBox* m_spinPMin=nullptr,*m_spinPMax=nullptr;
    QDoubleSpinBox* m_spinRMin=nullptr,*m_spinRMax=nullptr;
    QDoubleSpinBox* m_spinAngRes=nullptr,*m_spinMeasT=nullptr;
    QDoubleSpinBox* m_spinFMin=nullptr,*m_spinFMax=nullptr,*m_spinFStep=nullptr;
    QDoubleSpinBox* m_spinHD=nullptr,*m_spinTD=nullptr,*m_spinDist=nullptr;
};
