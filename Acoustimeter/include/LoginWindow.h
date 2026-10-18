#pragma once
#include <QDialog>
#include <QTimer>
#include "DataTypes.h"
#include "HardwareManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LoginWindow; }
QT_END_NAMESPACE
class QPushButton;

class LoginWindow : public QDialog
{
    Q_OBJECT
public:
    explicit LoginWindow(HardwareManager* hw, QWidget* parent = nullptr);
    ~LoginWindow();
    ConnectionConfig connectionConfig() const { return m_config; }
signals:
    void connectionEstablished(const ConnectionConfig& config);
private slots:
    void onConnectClicked();
    void onDevices(const QVector<NiDevice>& d);
    void onTimeout();
private:
    void setupUi();
    void setupConnections();
    void updateVisaString();
    void onProfileToggled();
    void addStep(const QString& text, bool ok, bool warn = false);
    void finishOk(int nDevices);
    void finishFail(const QString& msg);
    void fillConfig();
    Ui::LoginWindow* ui = nullptr;
    HardwareManager* m_hw = nullptr;
    QPushButton* m_btnOp      = nullptr;
    QPushButton* m_btnAdmin   = nullptr;
    QPushButton* m_btnOffline = nullptr;
    QTimer* m_timeout = nullptr;
    bool m_waiting = false;
    bool m_connected = false;
    ConnectionConfig m_config;
};
