// Mesure Acoustimeter (.bin ACMB) : resume, courbes, export CSV.   acoustimeter_mesure --help
#include "programmes.h"
int main(int argc, char** argv) { return acous::lancer(argc, argv, acous::Programme::Mesure); }
