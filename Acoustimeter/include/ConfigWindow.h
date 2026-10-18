#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QSpinBox>
#include <QVector>
#include "DataTypes.h"

class HydroWidget;
class AmplifierWidget;
class ToolsWidget;

class ConfigWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ConfigWindow(QWidget* parent = nullptr);
    SystemConfig config() const;
    void setConfig(const SystemConfig& c);
public slots:
    void setMliResult(int ampId, bool ok, const QString& message, const QVector<quint8>& preview);
signals:
    void configChanged(const SystemConfig& config);
    void generateMliRequested(const AmplifierConfig& cfg);
    void ampVoieToggled(int ampId, bool on);
    void hardwareMappingChanged();
private slots:
    void onValidate();
private:
    void setupUi();
    void setupLeftPanel();
    void syncTabs();
    void addHydro(); void addAmp();
    void removeHydro(int index); void removeAmp(int index);
    void retitle();
    void saveNamed(); void loadNamed(); void deleteNamed(); void exportTo(); void importFrom();
    bool writeFile(const QString& path) const;
    bool readFile(const QString& path);
    QString configDir() const;

    QWidget*    m_leftPanel  = nullptr;
    QTabWidget* m_tabWidget  = nullptr;
    QSpinBox*   m_spinHydros = nullptr;
    QSpinBox*   m_spinAmplis = nullptr;
    QVector<HydroWidget*>     m_hydros;
    QVector<AmplifierWidget*> m_amps;
    ToolsWidget* m_tools = nullptr;
    bool m_syncing = false;
};
