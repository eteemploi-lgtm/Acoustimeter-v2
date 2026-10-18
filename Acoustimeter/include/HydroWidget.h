#pragma once
#include <QScrollArea>
#include "DataTypes.h"
class QLineEdit; class QSpinBox; class QDoubleSpinBox; class QComboBox; class QGroupBox;
class HydroWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit HydroWidget(int index, QWidget* parent = nullptr);
    HydrophoneConfig config() const;
    void loadConfig(const HydrophoneConfig& c);
    void setIndex(int index);
    void refreshModels();
signals:
    void createRequested();
    void applyRequested();
    void deleteRequested(int index);
private:
    void setupUi();
    int m_index;
    QGroupBox*      m_grpV = nullptr;
    QSpinBox*       m_spinChannel  = nullptr;
    QDoubleSpinBox* m_spinGain     = nullptr;
    QSpinBox*       m_spinFs       = nullptr;
    QComboBox*      m_comboDyn     = nullptr;
    QDoubleSpinBox* m_spinDist     = nullptr;
    QDoubleSpinBox* m_spinDepth    = nullptr;
    QComboBox*      m_comboModel   = nullptr;
    QLineEdit*      m_editSens     = nullptr;
};
