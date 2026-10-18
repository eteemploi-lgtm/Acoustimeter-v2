#include "StyleManager.h"

StyleManager& StyleManager::instance() {
    static StyleManager s; return s;
}
void StyleManager::setTheme(Theme t) { m_theme = t; }
QString StyleManager::appStyleSheet()    const { return isDark()?darkQSS():lightQSS(); }
QString StyleManager::loginStyleSheet()  const { return appStyleSheet(); }
QString StyleManager::mainStyleSheet()   const { return appStyleSheet(); }
QString StyleManager::configStyleSheet() const { return appStyleSheet(); }

QString StyleManager::darkQSS() const { return R"(
QMainWindow,QDialog,QWidget#centralWidget{background-color:#0d1117;color:#e6edf3;}
QWidget{font-family:"Courier New",monospace;font-size:14px;color:#e6edf3;background-color:transparent;}
QFrame#panelFrame{background-color:#161b22;border:1px solid #30363d;border-radius:3px;}
QGroupBox{background-color:#161b22;border:1px solid #30363d;border-radius:3px;margin-top:22px;padding-top:10px;}
QGroupBox::title{color:#00d4ff;font-size:13px;font-weight:bold;subcontrol-origin:margin;subcontrol-position:top left;left:10px;top:-2px;padding:2px 6px;background-color:#161b22;}
QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox{background-color:#1f2937;border:1px solid #444c56;border-radius:2px;padding:5px 8px;color:#e6edf3;font-size:14px;selection-background-color:#003355;min-height:26px;}
QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus{border-color:#00d4ff;}
QComboBox::drop-down{border:none;width:20px;}
QComboBox QAbstractItemView{background-color:#1f2937;border:1px solid #444c56;selection-background-color:#003355;color:#e6edf3;font-size:14px;}
QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{background-color:#2d3748;border:1px solid #444c56;width:24px;border-radius:2px;}
QSpinBox::up-button:hover,QSpinBox::down-button:hover,QDoubleSpinBox::up-button:hover,QDoubleSpinBox::down-button:hover{background-color:#00d4ff;border-color:#00d4ff;}
QPushButton{background-color:#1f2937;border:1px solid #444c56;border-radius:2px;padding:6px 14px;color:#8b949e;font-size:14px;min-height:28px;}
QPushButton:hover{border-color:#8b949e;color:#e6edf3;}
QPushButton:pressed{background-color:#2d3748;}
QPushButton:disabled{color:#444c56;border-color:#30363d;}
QPushButton[cssClass="primary"]{background-color:#001a2e;border-color:#00d4ff;color:#00d4ff;font-size:14px;}
QPushButton[cssClass="primary"]:hover{background-color:#002a42;}
QPushButton[cssClass="connect"]{background-color:#0a4a6e;border:1px solid #00d4ff;color:#00d4ff;font-size:14px;font-weight:bold;padding:8px;min-height:32px;}
QPushButton[cssClass="connect"]:hover{background-color:#0d6a9e;}
QPushButton[cssClass="warning"]{background-color:#1a0d00;border-color:#ffaa00;color:#ffaa00;font-size:14px;}
QPushButton[cssClass="warning"]:hover{background-color:#2a1800;}
QPushButton[cssClass="danger"]{background-color:#1a0000;border-color:#ff4444;color:#ff4444;font-size:14px;}
QPushButton[cssClass="danger"]:hover{background-color:#2a0000;}
QPushButton#themeBtn{background-color:transparent;border:1px solid #444c56;color:#8b949e;padding:2px 8px;max-width:34px;font-size:14px;}
QPushButton#themeBtn:hover{border-color:#00d4ff;color:#00d4ff;}
QTabWidget::pane{border:1px solid #30363d;background-color:#0d1117;}
QTabBar::tab{background-color:#0d1117;border:1px solid transparent;border-bottom:none;color:#6e7681;padding:6px 14px;font-size:13px;font-weight:bold;min-width:80px;}
QTabBar::tab:selected{color:#00d4ff;border-color:#00d4ff;background-color:#001a2e;}
QTabBar::tab:hover{color:#8b949e;}
QToolBar{background-color:#161b22;border-bottom:1px solid #30363d;spacing:0px;padding:2px 0px;min-height:38px;}
QToolBar QLabel{color:#8b949e;font-size:13px;padding:0 6px;}
QMenuBar{background-color:#161b22;border-bottom:1px solid #30363d;color:#8b949e;font-size:13px;padding:0;min-height:28px;}
QMenuBar::item{padding:6px 12px;background:transparent;}
QMenuBar::item:selected{background-color:#1f2937;color:#e6edf3;}
QMenu{background-color:#161b22;border:1px solid #444c56;color:#e6edf3;font-size:13px;}
QMenu::item{padding:7px 22px 7px 16px;}
QMenu::item:selected{background-color:#1f2937;}
QMenu::separator{height:1px;background:#30363d;margin:3px 0;}
QStatusBar{background-color:#161b22;border-top:1px solid #30363d;color:#8b949e;font-size:13px;min-height:24px;}
QScrollBar:vertical{background:#0d1117;width:8px;border:none;}
QScrollBar::handle:vertical{background:#30363d;border-radius:4px;min-height:20px;}
QScrollBar::handle:vertical:hover{background:#444c56;}
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}
QScrollBar:horizontal{background:#0d1117;height:8px;border:none;}
QScrollBar::handle:horizontal{background:#30363d;border-radius:4px;min-width:20px;}
QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}
QProgressBar{background-color:#30363d;border:none;border-radius:2px;height:5px;text-align:center;color:transparent;}
QProgressBar::chunk{background-color:#00d4ff;border-radius:2px;}
QLabel#logoLabel{color:#00d4ff;font-size:22px;font-weight:bold;letter-spacing:3px;}
QLabel#subLabel{color:#6e7681;font-size:12px;letter-spacing:1px;}
QLabel#visaLabel{color:#00d4ff;font-family:"Courier New",monospace;font-size:13px;background-color:#0d1117;border:1px solid #30363d;border-radius:2px;padding:5px 10px;}
QLabel#sectionLabel{color:#6e7681;font-size:12px;font-weight:bold;border-bottom:1px solid #30363d;padding-bottom:3px;}
QLabel#metricValue{color:#00e676;font-weight:bold;font-size:14px;}
QLabel#metricValue[state="warn"]{color:#ffaa00;}
QLabel#metricValue[state="err"]{color:#ff4444;}
QLabel#metricValue[state="info"]{color:#00d4ff;}
QLabel#metricLabel{color:#8b949e;font-size:13px;}
QTextEdit#logView{background-color:#161b22;border:none;border-top:1px solid #30363d;color:#8b949e;font-size:13px;font-family:"Courier New",monospace;}
QFrame#niBadge{background-color:#1a1200;border:1px solid #e8aa14;border-radius:3px;}
QLabel#niLabel{color:#e8aa14;font-size:12px;letter-spacing:2px;font-weight:bold;}
QLabel#niModel{color:#e8aa14;font-size:14px;font-weight:bold;letter-spacing:1px;}
QListWidget{background-color:#0d1117;border:none;font-size:13px;color:#8b949e;}
QListWidget::item{padding:2px 4px;}
)"; }

QString StyleManager::lightQSS() const { return R"(
QMainWindow,QDialog,QWidget#centralWidget{background-color:#f0f4f8;color:#1a2332;}
QWidget{font-family:"Courier New",monospace;font-size:14px;color:#1a2332;background-color:transparent;}
QFrame#panelFrame,QGroupBox{background-color:#ffffff;border:1px solid #cbd5e0;border-radius:3px;}
QGroupBox{margin-top:22px;padding-top:10px;}
QGroupBox::title{color:#185fa5;font-size:13px;font-weight:bold;subcontrol-origin:margin;subcontrol-position:top left;left:10px;top:-2px;padding:2px 6px;background-color:#ffffff;}
QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox{background-color:#e8edf2;border:1px solid #a0aec0;border-radius:2px;padding:5px 8px;color:#1a2332;font-size:14px;min-height:26px;}
QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus{border-color:#185fa5;}
QComboBox QAbstractItemView{background-color:#ffffff;border:1px solid #a0aec0;color:#1a2332;font-size:14px;}
QPushButton{background-color:#e8edf2;border:1px solid #a0aec0;border-radius:2px;padding:6px 14px;color:#4a5568;font-size:14px;min-height:28px;}
QPushButton:hover{border-color:#185fa5;color:#1a2332;}
QPushButton[cssClass="primary"]{background-color:#e6f1fb;border-color:#185fa5;color:#185fa5;font-size:14px;}
QPushButton[cssClass="connect"]{background-color:#185fa5;border-color:#185fa5;color:#ffffff;font-size:14px;font-weight:bold;min-height:32px;}
QPushButton[cssClass="connect"]:hover{background-color:#0c447c;}
QPushButton[cssClass="warning"]{background-color:#faeeda;border-color:#ba7517;color:#ba7517;font-size:14px;}
QPushButton[cssClass="danger"]{background-color:#fcebeb;border-color:#a32d2d;color:#a32d2d;font-size:14px;}
QMenuBar{background-color:#ffffff;border-bottom:1px solid #cbd5e0;color:#4a5568;font-size:13px;min-height:28px;}
QMenuBar::item{padding:6px 12px;}
QMenuBar::item:selected{background-color:#e8edf2;}
QMenu{background-color:#ffffff;border:1px solid #a0aec0;color:#1a2332;font-size:13px;}
QMenu::item{padding:7px 22px 7px 16px;}
QMenu::item:selected{background-color:#e8edf2;}
QTabBar::tab{background-color:#f0f4f8;border:1px solid transparent;color:#718096;padding:6px 14px;font-size:13px;font-weight:bold;min-width:80px;}
QTabBar::tab:selected{color:#185fa5;border-color:#185fa5;background-color:#e6f1fb;}
QStatusBar{background-color:#ffffff;border-top:1px solid #cbd5e0;color:#718096;font-size:13px;min-height:24px;}
QScrollBar:vertical{background:#f0f4f8;width:8px;}
QScrollBar::handle:vertical{background:#a0aec0;border-radius:4px;}
QProgressBar{background-color:#d1d9e0;border:none;border-radius:2px;height:5px;}
QProgressBar::chunk{background-color:#185fa5;border-radius:2px;}
QLabel#logoLabel{color:#185fa5;font-size:22px;font-weight:bold;letter-spacing:3px;}
QLabel#visaLabel{color:#185fa5;background-color:#e6f1fb;border:1px solid #cbd5e0;border-radius:2px;padding:5px 10px;font-size:13px;}
QLabel#metricValue{color:#3b6d11;font-weight:bold;font-size:14px;}
QLabel#metricValue[state="warn"]{color:#854f0b;}
QLabel#metricValue[state="err"]{color:#a32d2d;}
QLabel#metricValue[state="info"]{color:#185fa5;}
QTextEdit#logView{background-color:#ffffff;border-top:1px solid #cbd5e0;color:#4a5568;font-size:13px;}
QFrame#niBadge{background-color:#faeeda;border:1px solid #ba7517;border-radius:3px;}
QLabel#niLabel{color:#ba7517;font-size:12px;font-weight:bold;}
QLabel#niModel{color:#ba7517;font-size:14px;font-weight:bold;}
QListWidget{background-color:#f0f4f8;border:none;font-size:13px;}
)"; }
