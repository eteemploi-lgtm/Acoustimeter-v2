# Acoustimeter — version production (donnees reelles uniquement)

## Compiler (MSVC + Ninja recommande ; MinGW possible si NIDAQmx.lib se lie)
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
          -DCMAKE_PREFIX_PATH=<Qt>/msvc2022_64 -DACOUSTIMETER_WITH_NI=ON
    cmake --build build
Options : NI_DAQMX_DIR (dossier ExternalCompilerSupport/C), ACOUSTIMETER_MLI_BACKEND=NONE|NIDIGITAL.

## Ou modifier selon le materiel
- Carte MLI 6570 -> 6537 : src/MliOutput.cpp, section "A MODIFIER SI CARTE 6537" (interface MliOutput inchangee),
  et include/CMake ACOUSTIMETER_MLI_BACKEND. La detection accepte deja 6570 ou 6537 (HardwareManager::digitalDevice).
- Cartes tension/courant : Configuration > Outils > Mapping materiel (voies physiques "Dev3/ai0,Dev3/ai1").
- Limite de temperature (coupure emission) : Outils > Mapping materiel (defaut 80 C).
- Filtre passe-bas ADC 4353 : src/HardwareManager.cpp, startTemperature (commentaire "AI ADCLowpass").
- Noms d'erreur NI : kErrSamplesNotYetAvailable en tete de HardwareManager.cpp (verifier dans NIDAQmx.h).

## Journaux
%APPDATA%/Acoustimeter/logs/acoustimeter_AAAAMMJJ.log (un fichier par jour).
