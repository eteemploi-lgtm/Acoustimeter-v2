#pragma once
#include <QString>

class StyleManager {
public:
    enum Theme { Dark, Light };
    static StyleManager& instance();
    void    setTheme(Theme t);
    Theme   theme() const { return m_theme; }
    bool    isDark() const { return m_theme == Dark; }
    QString appStyleSheet() const;
    QString loginStyleSheet()  const;
    QString mainStyleSheet()   const;
    QString configStyleSheet() const;
private:
    StyleManager() = default;
    Theme m_theme = Dark;
    QString darkQSS()  const;
    QString lightQSS() const;
};
