#pragma once
#include <QMainWindow>
#include <QTimer>
#include <QMap>
#include <QHash>
#include <QVector>
#include <deque>
#include "DataTypes.h"
#include "HardwareManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class GraphWidget;
class ConfigWindow;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QGroupBox;
class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(const ConnectionConfig& cfg, HardwareManager* hw, QWidget* parent = nullptr);
    ~MainWindow();
public slots:
    void toggleTheme();
protected:
    void closeEvent(QCloseEvent* e) override;
private slots:
    void onToggleVoie();
    void appendLog(const QString& level, const QString& msg);
    void openConfig();
    void toggleAcquisition();
    void toggleEmission();
    void emergencyStop();
    void blinkLed();
    void onConfigChanged(const SystemConfig& cfg);
    void onDevicesDetected(const QVector<NiDevice>& d);
    void onChannelsConfigured();
    void onTemperature(const QString& label, double celsius);
    void onSlow(const QString& label, double value, QChar kind);
    void onAcoustic(const QString& key, const QVector<double>& volts, double fs);
    void onSaturation(const QString& key, bool over);
    void onHwError(const QString& text);
    void onMliGenerated(int ampId, bool ok, const QString& msg, const QVector<quint8>& preview);
    void onEmissionState(bool on);
    void onAmpVoieToggled(int ampId, bool on);
    void applyTempLimit();
    void watchdog();
    void exportCSV();
    void showDiagnostic();
private:
    void setupVoieBar();
    void rebuildVoieBar();
    void setupSidebar();
    void rebuildSidebarLists();
    void refreshSidebarStatus();
    void setupCharts();
    void setupMenuBar();
    void setupStatusBar();
    void setupLogView();
    void showAbout();
    void updateNiBadge();
    void setButtonRunning(QPushButton* b, bool running, const QString& idle, const QString& busy);
    GraphWidget* miniFor(const QString& label) const;

    struct HistRow { qint64 ms; QString label; QChar kind; double value; };

    Ui::MainWindow* ui = nullptr;
    HardwareManager* m_hw = nullptr;
    ConnectionConfig m_connConfig;
    SystemConfig     m_cfg;

    GraphWidget* m_graphTime = nullptr; GraphWidget* m_graphFFT = nullptr;
    GraphWidget* m_graphLevels = nullptr; GraphWidget* m_graphSpec = nullptr;
    GraphWidget* m_miniTemp1 = nullptr; GraphWidget* m_miniTemp2 = nullptr;
    GraphWidget* m_miniV1 = nullptr;    GraphWidget* m_miniV2 = nullptr;
    GraphWidget* m_miniI1 = nullptr;    GraphWidget* m_miniI2 = nullptr;
    QGroupBox* m_boxT1 = nullptr; QGroupBox* m_boxT2 = nullptr; QGroupBox* m_boxV1 = nullptr;
    QGroupBox* m_boxV2 = nullptr; QGroupBox* m_boxI1 = nullptr; QGroupBox* m_boxI2 = nullptr;
    QHash<QString, GraphWidget*> m_miniByLabel;

    ConfigWindow* m_configWindow = nullptr;
    QTimer* m_ledTimer = nullptr;
    QTimer* m_watchTimer = nullptr;

    QPushButton* m_acqButton = nullptr;
    QPushButton* m_emitButton = nullptr;
    QLabel*      m_ledLabel = nullptr;
    QLabel*      m_niBadge = nullptr;
    bool         m_ledOn = false;

    QMap<QString, bool>         m_voies;
    QMap<QString, QPushButton*> m_voieBtns;
    QVBoxLayout* m_ampLayout = nullptr;
    QVBoxLayout* m_hydroLayout = nullptr;
    QMap<QString, QLabel*> m_stateLbl;
    QMap<QString, qint64>  m_lastData;
    QMap<QString, bool>    m_sat;

    std::deque<HistRow> m_history;
    QString m_logPath;
};
