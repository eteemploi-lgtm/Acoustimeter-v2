// Enregistrement Acoustimeter (.bin ACOUSBIN) : resume, courbes, pont MLI, export CSV / WAV.   acoustimeter_enregistrement --help
#include "programmes.h"
int main(int argc, char** argv) { return acous::lancer(argc, argv, acous::Programme::Enregistrement); }
