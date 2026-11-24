#pragma once
namespace acous {
enum class Programme { Mesure, Enregistrement };
int lancer(int argc, char** argv, Programme programme);
}
