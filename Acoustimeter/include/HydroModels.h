#pragma once
// Base locale des modeles d'hydrophones (QSettings). Gere depuis Outils > Gestion hydrophones.
#include <QSettings>
#include <QStringList>
namespace HydroModels {
inline QStringList list() {
    QSettings s; QStringList l = s.value("hydroModels").toStringList();
    if (l.isEmpty()) l = QStringList{"Reson TC4013", "Bruel & Kjaer 8103", "Custom"};
    return l;
}
inline void save(const QStringList& l) { QSettings s; s.setValue("hydroModels", l); }
}
