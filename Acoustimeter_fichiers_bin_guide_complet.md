# Fichiers .bin d’Acoustimeter

*Guide complet : format, lecture, conversion et lecture audio*

*Formats ACMB v1 et ACOUSBIN v1 · Acoustimeter v58*

Ce guide permet de lire les fichiers .bin d’Acoustimeter avec son propre code (Python, MATLAB, C++ ou autre langage). Tous les exemples de code ont été exécutés sur de vrais fichiers produits par l’application.

## Sommaire

| 1 | D’où viennent les fichiers .bin et comment les reconnaître |
|---|---|
| 2 | Format ACMB (export de la fenêtre Mesure) |
| 3 | Format ACOUSBIN (enregistrements de la Supervision et trames MLI) |
| 4 | Lire les fichiers en Python |
| 5 | Lire les fichiers en MATLAB / Octave |
| 6 | Lire les fichiers en C++ |
| 7 | Convertir : CSV, Excel, WAV |
| 8 | Trames MLI : reconstruire et écouter le signal |
| 9 | Pièges et vérifications |

## 1. D’où viennent les fichiers .bin

Acoustimeter écrit des fichiers .bin à trois endroits. Il n’y a que **deux formats** : **ACMB** pour les résultats de mesure, **ACOUSBIN** pour les signaux enregistrés (Supervision et trames MLI).

| Où dans Acoustimeter | Bouton | Format | Contenu | Nom et dossier par défaut |
|---|---|---|---|---|
| Fenêtre Mesure | BIN… | **ACMB** | Le tableau des résultats : un point par ligne (f, U, I, TVR, TPR, admittance, Lp, distorsion…) | Documents/Acoustimeter/ mesure_TVR_TPR_AAAAMMJJ_HHMMSS.bin |
| Supervision | ● SAVE-MANU ◉ SAVE-TRIG | **ACOUSBIN** | Les signaux enregistrés : températures, U et I (valeur efficace par seconde), hydrophones, trames MLI, signal AB, repères | Documents/Acoustimeter/enregistrements/ enregistrement_AAAAMMJJ_hhmmss.bin (ou le dossier choisi dans Banc et opérateur) |
| Configuration › Ampli n › MLI et voies (aperçu) | disquette : SAVE / ■ STOP SAVE | **ACOUSBIN** | Les trames MLI de l’ampli (Bras A, Bras B…), une voie 0/1 par trame | Dossier choisi à l’enregistrement : trames_MLI_amplin_AAAAMMJJ_HHMMSS.bin |

**Reconnaître le format** : lisez les 8 premiers octets du fichier.

| Les premiers octets du fichier | Format | Lecteur |
|---|---|---|
| `41 43 4D 42` = texte « ACMB » | Mesure | read_acmb |
| `41 43 4F 55 53 42 49 4E` = texte « ACOUSBIN » | Enregistrement / trames MLI | read_acousbin |

> **Note :** **Règles communes** : tous les nombres sont en **petit-boutiste** (little-endian, ordre des PC Windows/Linux x86). uint32 = entier non signé 4 octets ; float32 / float64 = réels IEEE 754 sur 4 / 8 octets ; les textes sont en UTF-8. Chaque format a un numéro de version (1 aujourd’hui) : un lecteur doit refuser une version qu’il ne connaît pas.

## 2. Format ACMB (export de la fenêtre Mesure)

Un fichier ACMB est un **tableau** : une ligne par point de mesure, une colonne par grandeur, en réels double précision, précédé d’un en-tête texte qui décrit la mesure et donne le nom des colonnes.

*[Figure : voir le PDF — acmb_layout.png]*

*Figure 1 – Organisation d’un fichier ACMB*

| Position (octets) | Taille | Type | Contenu |
|---|---|---|---|
| 0 | 4 | char[4] | « ACMB » |
| 4 | 4 | uint32 | version = 1 |
| 8 | 4 | uint32 | nombre de colonnes C |
| 12 | 4 | uint32 | nombre de lignes (points) N |
| 16 | 4 | uint32 | taille L de l’en-tête texte, en octets |
| 20 | L | texte UTF-8 | en-tête : une ligne « clé=valeur » par réglage (séparateur \n) |
| 20 + L | 8 × C × N | float64 | valeurs, ligne par ligne : point 1 (C valeurs), point 2… NaN = non mesuré |

### 2.1 En-tête texte

L’en-tête contient une ligne `clé=valeur` par réglage. La clé **colonnes** donne la liste des colonnes dans l’ordre, séparées par des virgules : c’est elle qui fait foi, car des colonnes optionnelles peuvent s’ajouter (températures, TCR).

| Clé de l’en-tête | Exemple / signification |
|---|---|
| mesure | type : balayage en fréquence, U=f(I), linéarité, directivité |
| date, simulation | date ISO de l’export ; 1 si mode hors ligne |
| ampli, classe, sortie_5413 | ampli mesuré ; AB ou D (porteuse, filtre) ; voie du 5413 |
| sonde_U, sonde_I, hydrophone, gain_preampli_dB | chaîne de mesure |
| distance_m, profondeur_*_m, celerite_m_s, correction_cuve | géométrie |
| etablissement_cycles, moyenne_salves, periode_ms, pulse_ms… | réglages de la salve |
| Umax_V_eff, Lp_max_global_dB_re_1uPa | extrapolation à Umax |
| colonnes | **liste des noms de colonnes, dans l’ordre** : toujours lire les colonnes par leur nom |
| bvd_* | schéma équivalent ajusté (fr, Q, Rm, C0, Lm, Cm), si disponible |
| remarque_k | remarque du point k (k = numéro de ligne à partir de 0) |

```
mesure=TVR/TPR/admittance (balayage en frequence)
date=2026-11-20T10:02:41
simulation=1
ampli=1
classe=AB
...
colonnes=f_Hz,niveau_5413_V_crete,Urms_V,Irms_A,P_W,cos_phi,V_hydro_eff_V,SL_dB_re_1uPa_m,TVR_dB_re_1uPa_m_per_V,...,salves
bvd_fr_Hz=30016.007
remarque_0=analyse : cycles 6 à 15 (0.5 ms)
```

### 2.2 Colonnes

| Colonne | Unité | Signification |
|---|---|---|
| f_Hz | Hz | fréquence de la salve |
| niveau_5413_V_crete | V crête / % | niveau demandé (V crête en classe AB, % de pleine échelle en classe D) |
| Urms_V, Irms_A | V, A | tension et courant efficaces au transducteur, composante à f (DFT) |
| P_W, Q_var, S_VA | W, var, VA | puissances active, réactive, apparente |
| cos_phi | — | facteur de puissance |
| V_hydro_eff_V | V | tension efficace de l’hydrophone à f |
| SL_dB_re_1uPa_m | dB re 1 µPa·m | niveau source ramené à 1 m |
| TVR_dB_re_1uPa_m_per_V | dB re 1 µPa·m/V | réponse en émission en tension |
| TPR_dB_re_1uPa_m_per_sqrtW | dB re 1 µPa·m/√W | réponse en émission en puissance |
| G_S, B_S, Y_S | S | conductance, susceptance, module de l’admittance |
| SLmax_dB_re_1uPa_m | dB re 1 µPa·m | SL extrapolé à Umax |
| Lpmax_Umax_dB_re_1uPa | dB re 1 µPa | Lp à l’hydrophone extrapolé à Umax |
| absorption_dB | dB | correction d’absorption appliquée |
| THDF_x_pct, SFDR_x_dB, THDN_x_pct | %, dB, % | distorsion de U, I et de l’hydrophone (x = U, I, hydro) |
| angle_deg, directivite_dB | °, dB | directivité (sinon NaN) |
| passe | — | numéro de passage (répétitions, linéarité) |
| temps_s | s | instant du point depuis le début de la mesure |
| T_nom_C | °C | une colonne par sonde de température cochée (optionnelles) |
| salves | — | nombre de salves moyennées (toujours en dernier) |
| TCR_…, Z_ohm | dB, Ω | seulement si le programme est compilé avec l’option ACOUSTIMETER_TCR_Z |

> **Note :** Une valeur non mesurée ou sans objet vaut **NaN** (par exemple angle_deg hors directivité, Lp à Umax sans Umax). Ne pas confondre NaN et 0.

*[Figure : voir le PDF — acmb_courbes.png]*

*Figure 2 – TVR, G et B relus dans un fichier ACMB (mesure simulée)*

## 3. Format ACOUSBIN (enregistrements)

Un fichier ACOUSBIN contient plusieurs **voies** (signaux) de cadences différentes. Il commence par une zone fixe de 65 536 octets qui contient un en-tête JSON, puis une suite de **blocs** de données : chaque bloc dit à quelle voie il appartient et combien de valeurs il contient.

*[Figure : voir le PDF — acousbin_layout.png]*

*Figure 3 – Organisation d’un fichier ACOUSBIN*

| Position (octets) | Taille | Type | Contenu |
|---|---|---|---|
| 0 | 8 | char[8] | « ACOUSBIN » |
| 8 | 4 | uint32 | version = 1 |
| 12 | 4 | uint32 | taille H de la zone d’en-tête = 65536 |
| 16 | H − 16 | texte UTF-8 | en-tête JSON, complété par des espaces |
| H (= 65536) | 8 + n × t | bloc | uint32 index de voie \| uint32 n \| n valeurs (t = 4 octets float32, ou 1 octet si dtype = u8) |
| … |  | bloc | blocs suivants, dans l’ordre d’arrivée, voies mélangées, jusqu’à la fin du fichier |

### 3.1 En-tête JSON

Champs : **titre**, **debut** (date ISO), **voies** (liste), **evenements** (liste), **meta** (objet libre). L’**index de voie** d’un bloc est la position dans la liste *voies* (0 = première voie).

```
{
 "titre": "Acoustimeter",
 "debut": "2026-11-20T10:03:57",
 "voies": [
  {"nom": "V_AMP1_1", "groupe": "Tension", "unite": "V", "fs": 1, "t0": 0, "dtype": "f32"},
  {"nom": "I_AMP1_1", "groupe": "Courant", "unite": "A", "fs": 1, "t0": 0, "dtype": "f32"}
 ],
 "evenements": [
  {"t": 0, "type": "debut", "detail": "début de l'enregistrement (manuel)"},
  {"t": 4.224, "type": "fin", "detail": "arrêt manuel"}
 ],
 "meta": {"declenchement": "manuel", "duree_demandee_s": 0, "wavs": []}
}
```

| Champ (voies[i]) | Signification |
|---|---|
| nom | nom de la voie (ex. « Hydro 1 », « V_AMP1_1 », « MLI3 », « A1 Bras A — haut (MLI1) ») |
| groupe | Température, Tension, Courant, Hydrophone, MLI ou Analogique AB |
| unite | °C, V, A, bit, 0/1 |
| fs | fréquence d’échantillonnage de la voie, en Hz |
| t0 | instant du premier échantillon, en s depuis le début de l’enregistrement |
| dtype | « f32 » (float32, 4 octets) ou « u8 » (un octet par échantillon, 0 ou 1) |

### 3.2 Blocs et temps

- Lire les blocs depuis la position 65 536 jusqu’à la fin : `uint32 voie, uint32 n`, puis n valeurs.
- Concaténer, pour chaque voie, les valeurs de ses blocs dans l’ordre du fichier. Les blocs de voies différentes sont entremêlés.
- Temps de l’échantillon i d’une voie : **t = t0 + i / fs** (en secondes depuis le début de l’enregistrement).
- dtype « u8 » : un octet par échantillon, 0 ou 1 (trames MLI). Sinon float32.
- Arrêter la lecture si l’index de voie est hors de la liste ou si le fichier se termine au milieu d’un bloc (fichier tronqué : on garde ce qui est lu).

### 3.3 Contenu selon l’origine

| Groupe | Unité | fs | Ce qui est écrit |
|---|---|---|---|
| Température | °C | cadence du 4353 (réglée dans la configuration) | une valeur par lecture du thermocouple |
| Tension, Courant | V, A | 1 Hz | valeur efficace vraie de chaque seconde (pas la forme d’onde) |
| Hydrophone | V | cadence du 4480 | signal brut de l’hydrophone, en volts à l’entrée de la carte |
| MLI (Supervision) | bit (u8) | Fe MLI | **une période** de la séquence jouée en boucle par le 6537 ; voies MLIk non nulles seulement |
| MLI (Configuration, disquette) | 0/1 (u8) | Fe MLI | toute la durée entre SAVE et STOP SAVE (la séquence répétée), une voie par trame |
| Analogique AB | V | cadence du WAV | **une période** du signal envoyé au 5413 (rejouée en boucle), t0 = son début |

### 3.4 Événements et méta-données

| Type d’événement | Signification |
|---|---|
| debut, fin | début et fin de l’enregistrement (detail : manuel, déclencheur, durée atteinte…) |
| acquisition_on, acquisition_off | acquisition démarrée ou arrêtée |
| emission_on, emission_off | émission démarrée ou arrêtée |
| mli_play, mli_stop | trames MLI lancées ou arrêtées |
| ab_play, ab_pause, ab_stop, ab_niveau | génération AB (classe AB) et changements de niveau |

**meta** d’un enregistrement de la Supervision : `declenchement` (manuel, événement, voie), `duree_demandee_s`, et `wavs` : pour chaque ampli, le WAV de la configuration (ampli, classe, canal, fichier, duree, fs). **meta** d’un fichier de trames MLI : ampli, wav, fe_mli_Hz, duree_s, source, modulation, topologie, decoupage_kHz, profondeur.

> **Attention :** L’en-tête est réécrit à la **fermeture** du fichier (liste finale des voies, t0, cadences). Un fichier dont l’écriture a été interrompue (plantage, coupure) peut avoir un en-tête sans voies : les blocs sont présents mais ne peuvent pas être attribués.

## 4. Lire les fichiers en Python

Module complet, à enregistrer sous `acoustimeter_bin.py` (Python 3 + numpy). Il reconnaît le format, lit ACMB et ACOUSBIN, et contient les conversions des sections 7 et 8.

```
"""Lecture des fichiers .bin d'Acoustimeter (Python 3 + numpy).

Deux formats :
  - ACMB     : export de la fenetre Mesure (tableau de points, float64)
  - ACOUSBIN : enregistrements (Supervision SAVE-MANU / SAVE-TRIG, trames MLI de la Configuration)
"""
import json
import struct
import numpy as np

def detect(path):
    """Renvoie 'ACMB', 'ACOUSBIN' ou None selon les premiers octets."""
    with open(path, "rb") as f:
        head = f.read(8)
    if head[:4] == b"ACMB":
        return "ACMB"
    if head == b"ACOUSBIN":
        return "ACOUSBIN"
    return None

def read_acmb(path):
    """Fichier de mesure -> (entete: dict, colonnes: list, donnees: ndarray [lignes x colonnes])."""
    with open(path, "rb") as f:
        if f.read(4) != b"ACMB":
            raise ValueError("pas un fichier ACMB")
        version, ncol, nrow, hlen = struct.unpack("<4I", f.read(16))
        if version != 1:
            raise ValueError(f"version ACMB {version} non prise en charge")
        text = f.read(hlen).decode("utf-8")
        data = np.frombuffer(f.read(8 * ncol * nrow), dtype="<f8").reshape(nrow, ncol)
    header = {}
    for line in text.split("\n"):
        if "=" in line:
            k, v = line.split("=", 1)
            header[k] = v
    cols = header["colonnes"].split(",")
    if len(cols) != ncol:
        raise ValueError("nombre de colonnes incoherent")
    return header, cols, data

def read_acousbin(path):
    """Enregistrement -> (entete JSON: dict, voies: dict nom -> {'info','temps','valeurs'})."""
    with open(path, "rb") as f:
        if f.read(8) != b"ACOUSBIN":
            raise ValueError("pas un fichier ACOUSBIN")
        version, hlen = struct.unpack("<2I", f.read(8))
        if version != 1:
            raise ValueError(f"version ACOUSBIN {version} non prise en charge")
        header = json.loads(f.read(hlen - 16).decode("utf-8").strip())
        voies = header["voies"]
        morceaux = [[] for _ in voies]
        while True:
            b = f.read(8)
            if len(b) < 8:
                break
            idx, n = struct.unpack("<2I", b)
            if idx >= len(voies):
                break                                   # bloc invalide : fin de lecture
            u8 = voies[idx].get("dtype", "f32") == "u8"
            raw = f.read(n * (1 if u8 else 4))
            morceaux[idx].append(np.frombuffer(raw, dtype=np.uint8 if u8 else "<f4"))
            if len(raw) < n * (1 if u8 else 4):
                break                                   # fichier tronque
    out = {}
    for v, m in zip(voies, morceaux):
        x = np.concatenate(m) if m else np.zeros(0, dtype=np.float32)
        t = v.get("t0", 0.0) + np.arange(x.size) / v["fs"]
        out[v["nom"]] = {"info": v, "temps": t, "valeurs": x}
    return header, out
```

### Exemple d’utilisation :

```
import acoustimeter_bin as ab

# Mesure
entete, colonnes, d = ab.read_acmb("mesure_TVR_TPR_20261120_100241.bin")
f   = d[:, colonnes.index("f_Hz")]
tvr = d[:, colonnes.index("TVR_dB_re_1uPa_m_per_V")]
print(entete["mesure"], entete["distance_m"], "m")

# Enregistrement
entete, voies = ab.read_acousbin("enregistrement_20261120_100357.bin")
for nom, v in voies.items():
    print(nom, v["info"]["groupe"], v["info"]["fs"], "Hz", v["valeurs"].size, "points")
h = voies["Hydro 1"]           # temps : h["temps"], valeurs : h["valeurs"]
for e in entete["evenements"]:
    print(e["t"], e["type"], e["detail"])
```

Sortie réelle du module lancé en ligne de commande sur trois fichiers :

```
== mesure_TVR_TPR.bin : ACMB
   TVR/TPR/admittance (balayage en frequence) | 71 points x 32 colonnes
   premier point : f = 10000.0 Hz, TVR = 119.37 dB
== enregistrement_supervision.bin : ACOUSBIN
   Acoustimeter 2026-11-20T10:03:57 | 2 voies | 2 evenements
   V_AMP1_1   Tension  V    fs=1 Hz t0=0.000 s n=4 dtype=f32
== trames_MLI_ampli1.bin : ACOUSBIN
   A1 Bras A — haut (MLI1)   MLI   0/1  fs=1e+07 Hz t0=0.000 s n=200000 dtype=u8
```

## 5. Lire les fichiers en MATLAB / Octave

Deux fonctions, à enregistrer chacune dans un fichier .m du même nom. Testées avec GNU Octave (même syntaxe que MATLAB R2016b et suivants).

### read_acmb.m

```
function [entete, colonnes, donnees] = read_acmb(chemin)
% READ_ACMB  Lit un export .bin de la fenetre Mesure d'Acoustimeter (format ACMB v1).
%   entete   : struct (cles = noms de l'entete, valeurs = texte)
%   colonnes : cellule des noms de colonnes
%   donnees  : matrice [points x colonnes] (double, NaN = non mesure)
f = fopen(chemin, 'r', 'ieee-le');
if f < 0, error('ouverture impossible : %s', chemin); end
c = onCleanup(@() fclose(f));
if ~strcmp(char(fread(f, 4, 'uint8=>char')'), 'ACMB'), error('pas un fichier ACMB'); end
v = fread(f, 4, 'uint32');            % version, nb colonnes, nb lignes, taille entete
if v(1) ~= 1, error('version ACMB %d non prise en charge', v(1)); end
ncol = v(2); nlig = v(3);
texte = native2unicode(fread(f, v(4), 'uint8=>uint8')', 'UTF-8');
donnees = fread(f, [ncol, nlig], 'double')';
entete = struct();
lignes = strsplit(texte, sprintf('\n'));
for k = 1:numel(lignes)
    p = strfind(lignes{k}, '=');
    if isempty(p), continue; end
    cle = matlab.lang.makeValidName(lignes{k}(1:p(1)-1));
    entete.(cle) = lignes{k}(p(1)+1:end);
end
colonnes = strsplit(entete.colonnes, ',');
end
```

### read_acousbin.m

```
function [entete, voies] = read_acousbin(chemin)
% READ_ACOUSBIN  Lit un enregistrement .bin d'Acoustimeter (format ACOUSBIN v1) :
%   Supervision (SAVE-MANU / SAVE-TRIG) ou trames MLI enregistrees depuis la Configuration.
%   entete : struct issue du JSON (titre, debut, voies, evenements, meta)
%   voies  : tableau de struct (nom, groupe, unite, fs, t0, dtype, valeurs, temps)
f = fopen(chemin, 'r', 'ieee-le');
if f < 0, error('ouverture impossible : %s', chemin); end
c = onCleanup(@() fclose(f));
if ~strcmp(char(fread(f, 8, 'uint8=>char')'), 'ACOUSBIN'), error('pas un fichier ACOUSBIN'); end
v = fread(f, 2, 'uint32');            % version, taille de l'en-tete (65536)
if v(1) ~= 1, error('version ACOUSBIN %d non prise en charge', v(1)); end
js = native2unicode(fread(f, v(2) - 16, 'uint8=>uint8')', 'UTF-8');
entete = jsondecode(strtrim(js));
d = entete.voies; if ~iscell(d), d = num2cell(d); end
n = numel(d);
morceaux = cell(n, 1);
while true
    b = fread(f, 2, 'uint32');
    if numel(b) < 2, break; end
    idx = b(1) + 1; nb = b(2);
    if idx > n, break; end
    if strcmp(d{idx}.dtype, 'u8'), x = fread(f, nb, 'uint8=>uint8'); else, x = fread(f, nb, 'single=>single'); end
    morceaux{idx}{end+1} = x;
    if numel(x) < nb, break; end
end
voies = struct('nom', {}, 'groupe', {}, 'unite', {}, 'fs', {}, 't0', {}, 'dtype', {}, 'valeurs', {}, 'temps', {});
for k = 1:n
    x = vertcat(morceaux{k}{:});
    if isempty(x), x = zeros(0, 1, 'single'); end
    voies(k).nom = d{k}.nom; voies(k).groupe = d{k}.groupe; voies(k).unite = d{k}.unite;
    voies(k).fs = d{k}.fs; voies(k).t0 = d{k}.t0; voies(k).dtype = d{k}.dtype;
    voies(k).valeurs = x;
    voies(k).temps = d{k}.t0 + (0:numel(x)-1)' / d{k}.fs;
end
end
```

### Exemple :

```
[h, col, d] = read_acmb('mesure_TVR_TPR_20261120_100241.bin');
f = d(:, strcmp(col, 'f_Hz')); tvr = d(:, strcmp(col, 'TVR_dB_re_1uPa_m_per_V'));
plot(f/1e3, tvr); xlabel('kHz'); ylabel('TVR (dB re 1 µPa·m/V)');

[e, v] = read_acousbin('enregistrement_20261120_100357.bin');
k = find(strcmp({v.nom}, 'Hydro 1')); plot(v(k).temps, v(k).valeurs);
audiowrite('hydro1.wav', v(k).valeurs / max(abs(v(k).valeurs)), v(k).fs);   % pour l'écouter
```

## 6. Lire les fichiers en C++

Lecteur en C++17 sans Qt, dans un seul fichier d’en-tête. Il utilise la bibliothèque libre **nlohmann/json** (un fichier json.hpp) pour l’en-tête JSON. Compilé et testé avec g++ 11 (`g++ -std=c++17 exemple.cpp`) ; fonctionne aussi avec MSVC.

```
// Lecture des fichiers .bin d'Acoustimeter en C++17 (sans Qt).
// Dependance : nlohmann/json (un seul fichier json.hpp) pour l'en-tete JSON des enregistrements.
#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "json.hpp"

namespace acoustimeter {

// ---------------------------------------------------------------- ACMB : export de la fenetre Mesure
struct Mesure {
    std::map<std::string, std::string> entete;     // cle -> valeur (texte)
    std::vector<std::string> colonnes;
    std::vector<std::vector<double>> lignes;       // [point][colonne], NaN = non mesure
    int colonne(const std::string& nom) const {
        for (size_t i = 0; i < colonnes.size(); ++i) if (colonnes[i] == nom) return int(i);
        return -1;
    }
};

inline uint32_t lireU32(std::ifstream& f) {        // petit-boutiste (PC x86/x64)
    unsigned char b[4];
    if (!f.read(reinterpret_cast<char*>(b), 4)) throw std::runtime_error("fin de fichier inattendue");
    return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
}

inline Mesure lireAcmb(const std::string& chemin) {
    std::ifstream f(chemin, std::ios::binary);
    if (!f) throw std::runtime_error("ouverture impossible : " + chemin);
    char magic[4];
    f.read(magic, 4);
    if (std::memcmp(magic, "ACMB", 4) != 0) throw std::runtime_error("pas un fichier ACMB");
    const uint32_t version = lireU32(f), ncol = lireU32(f), nlig = lireU32(f), hlen = lireU32(f);
    if (version != 1) throw std::runtime_error("version ACMB non prise en charge");
    std::string texte(hlen, '\0');
    f.read(&texte[0], hlen);
    Mesure m;
    size_t p = 0;
    while (p <= texte.size()) {
        size_t q = texte.find('\n', p); if (q == std::string::npos) q = texte.size();
        const std::string l = texte.substr(p, q - p);
        const size_t e = l.find('=');
        if (e != std::string::npos) m.entete[l.substr(0, e)] = l.substr(e + 1);
        p = q + 1;
    }
    const std::string cols = m.entete["colonnes"];
    for (size_t a = 0, b; a <= cols.size(); a = b + 1) { b = cols.find(',', a); if (b == std::string::npos) b = cols.size(); m.colonnes.push_back(cols.substr(a, b - a)); }
    if (m.colonnes.size() != ncol) throw std::runtime_error("nombre de colonnes incoherent");
    m.lignes.assign(nlig, std::vector<double>(ncol));
    for (auto& r : m.lignes) f.read(reinterpret_cast<char*>(r.data()), std::streamsize(ncol * sizeof(double)));   // float64 petit-boutiste
    return m;
}

// ---------------------------------------------------------------- ACOUSBIN : enregistrements
struct Voie {
    std::string nom, groupe, unite, dtype;      // dtype : "f32" ou "u8"
    double fs = 1, t0 = 0;                      // Hz, s
    std::vector<float> valeurs;                 // u8 converti en 0.f / 1.f
    double temps(size_t i) const { return t0 + double(i) / fs; }
};
struct Enregistrement {
    nlohmann::json entete;                      // titre, debut, voies, evenements, meta
    std::vector<Voie> voies;
};

inline Enregistrement lireAcousbin(const std::string& chemin) {
    std::ifstream f(chemin, std::ios::binary);
    if (!f) throw std::runtime_error("ouverture impossible : " + chemin);
    char magic[8];
    f.read(magic, 8);
    if (std::memcmp(magic, "ACOUSBIN", 8) != 0) throw std::runtime_error("pas un fichier ACOUSBIN");
    const uint32_t version = lireU32(f), hlen = lireU32(f);
    if (version != 1) throw std::runtime_error("version ACOUSBIN non prise en charge");
    std::string js(hlen - 16, '\0');
    f.read(&js[0], std::streamsize(js.size()));
    Enregistrement r;
    r.entete = nlohmann::json::parse(js.substr(0, js.find_last_not_of(' ') + 1));
    for (const auto& v : r.entete["voies"]) {
        Voie c; c.nom = v["nom"]; c.groupe = v["groupe"]; c.unite = v["unite"];
        c.fs = v["fs"]; c.t0 = v["t0"]; c.dtype = v.value("dtype", "f32");
        r.voies.push_back(c);
    }
    f.seekg(hlen);
    unsigned char hdr[8];
    while (f.read(reinterpret_cast<char*>(hdr), 8)) {
        const uint32_t idx = hdr[0] | hdr[1] << 8 | hdr[2] << 16 | uint32_t(hdr[3]) << 24;
        const uint32_t n   = hdr[4] | hdr[5] << 8 | hdr[6] << 16 | uint32_t(hdr[7]) << 24;
        if (idx >= r.voies.size()) break;                       // bloc invalide
        Voie& c = r.voies[idx];
        const size_t o = c.valeurs.size();
        if (c.dtype == "u8") {
            std::vector<unsigned char> b(n);
            if (!f.read(reinterpret_cast<char*>(b.data()), n)) break;
            c.valeurs.resize(o + n);
            for (uint32_t i = 0; i < n; ++i) c.valeurs[o + i] = b[i] ? 1.f : 0.f;
        } else {
            c.valeurs.resize(o + n);
            if (!f.read(reinterpret_cast<char*>(c.valeurs.data() + o), std::streamsize(n) * 4)) { c.valeurs.resize(o); break; }
        }
    }
    return r;
}

} // namespace acoustimeter
```

### Exemple :

```
#include "acoustimeter_bin.hpp"
#include <cstdio>
int main(int argc, char** argv) {
    using namespace acoustimeter;
    const Mesure m = lireAcmb(argv[1]);
    const int f = m.colonne("f_Hz"), tvr = m.colonne("TVR_dB_re_1uPa_m_per_V");
    std::printf("%s : %zu points ; f=%g Hz TVR=%.2f dB\n", m.entete.at("mesure").c_str(), m.lignes.size(), m.lignes[0][f], m.lignes[0][tvr]);
    const Enregistrement r = lireAcousbin(argv[2]);
    for (const Voie& v : r.voies) std::printf("%s [%s] fs=%g n=%zu\n", v.nom.c_str(), v.unite.c_str(), v.fs, v.valeurs.size());
    std::printf("evenements : %zu\n", r.entete["evenements"].size());
}
```

> **Note :** Autre langage (LabVIEW, C#, Java…) : appliquer exactement les tableaux des sections 2 et 3 ; l’en-tête ACOUSBIN est du JSON standard.

## 7. Convertir : CSV, Excel, WAV

Fonctions de conversion du module Python (suite de acoustimeter_bin.py) :

```
def acmb_to_csv(path, out_csv):
    """Export ACMB -> CSV (separateur ;, point decimal), entete en commentaires '#'."""
    header, cols, data = read_acmb(path)
    with open(out_csv, "w", encoding="utf-8") as f:
        for k, v in header.items():
            f.write(f"# {k}={v}\n")
        f.write(";".join(cols) + "\n")
        for row in data:
            f.write(";".join("" if np.isnan(x) else repr(float(x)) for x in row) + "\n")

def voie_to_csv(voie, out_csv):
    """Une voie d'enregistrement -> CSV temps;valeur."""
    unite = voie["info"]["unite"]
    np.savetxt(out_csv, np.column_stack([voie["temps"], voie["valeurs"]]), delimiter=";",
               header=f"temps_s;{voie['info']['nom']}_{unite}", comments="")

def write_wav_float(out_wav, x, fs):
    """Ecrit un WAV mono float32 (lisible par Audacity, Python, MATLAB) sans dependance."""
    x = np.asarray(x, dtype="<f4")
    data = x.tobytes()
    fmt = struct.pack("<HHIIHH", 3, 1, int(fs), int(fs) * 4, 4, 32)      # 3 = IEEE float
    with open(out_wav, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 4 + 8 + len(fmt) + 8 + len(data)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<I", len(fmt)) + fmt)
        f.write(b"data" + struct.pack("<I", len(data)) + data)

def voie_to_wav(voie, out_wav, normaliser=True):
    """Voie hydrophone (ou AB) -> WAV pour l'ecouter ; normalisee a +-1 par defaut."""
    x = voie["valeurs"].astype(np.float64)
    if normaliser and np.max(np.abs(x)) > 0:
        x = x / np.max(np.abs(x))
    write_wav_float(out_wav, x, voie["info"]["fs"])
```

### Exemples :

```
import acoustimeter_bin as ab

# Mesure -> CSV (ouvrable dans Excel)
ab.acmb_to_csv("mesure.bin", "mesure.csv")

# Mesure -> Excel (avec pandas)
import pandas as pd
h, col, d = ab.read_acmb("mesure.bin")
pd.DataFrame(d, columns=col).to_excel("mesure.xlsx", index=False)

# Enregistrement -> un CSV par voie, et l'hydrophone en WAV
h, voies = ab.read_acousbin("enregistrement.bin")
for nom, v in voies.items():
    ab.voie_to_csv(v, nom.replace(" ", "_") + ".csv")
ab.voie_to_wav(voies["Hydro 1"], "hydro1.wav")      # à écouter dans Audacity ou VLC

# Mettre deux voies sur la même base de temps (cadences différentes)
import numpy as np
T = voies["T ampli 1"]; H = voies["Hydro 1"]
T_sur_H = np.interp(H["temps"], T["temps"], T["valeurs"])
```

> **Note :** Le WAV est écrit en float32 normalisé à ±1 pour l’écoute. Pour garder les volts, utilisez `voie_to_wav(v, fichier, normaliser=False)` (les lecteurs audio saturent au-delà de ±1).

## 8. Trames MLI : reconstruire et écouter le signal

Une voie MLI contient 0 ou 1 à la fréquence d’échantillonnage MLI (Fe, souvent plusieurs MHz). Pour retrouver le signal envoyé au transducteur :

1. **Pont complet** : tension de sortie normalisée = (Bras A haut) − (Bras B haut), soit −1, 0 ou +1. Multiplier par la tension de bus de l’ampli pour obtenir des volts.
2. **Demi-pont** : tension normalisée = 2 × (Bras A haut) − 1.
3. **Filtrer** comme le filtre de sortie de l’ampli : un passe-bas qui garde la bande utile et supprime la porteuse (la fonction ci-dessous fait une moyenne glissante puis une décimation).
4. **Écouter** : écrire le résultat en WAV à la cadence réduite (par exemple 200 kHz ; pour l’oreille, ramener sous 48 kHz).

```
def mli_pont_complet(voies, haut_a, haut_b, fs_sortie=200000, f_coupure=None):
    """Tension normalisee d'un pont complet : (bras A haut) - (bras B haut), valeurs -1, 0, +1.
    Puis filtre passe-bas (moyenne glissante) et decimation pour obtenir le signal 'audio'
    que verrait la charge apres le filtre de l'ampli. Multiplier par la tension de bus pour avoir des volts."""
    a = voies[haut_a]["valeurs"].astype(np.int8)
    b = voies[haut_b]["valeurs"].astype(np.int8)
    fe = voies[haut_a]["info"]["fs"]
    pont = (a - b).astype(np.float32)                    # -1 / 0 / +1
    k = max(1, int(round(fe / fs_sortie)))               # facteur de decimation
    n = (pont.size // k) * k
    lisse = pont[:n].reshape(-1, k).mean(axis=1)         # moyenne sur k echantillons = passe-bas + decimation
    return pont, lisse, fe / k
```

```
h, voies = ab.read_acousbin("trames_MLI_ampli1_20261116.bin")
print([n for n in voies])          # noms des trames
pont, signal, fs = ab.mli_pont_complet(voies, "A1 Bras A — haut (MLI1)", "A1 Bras B — haut (MLI3)")
ab.write_wav_float("sortie_ampli1.wav", signal, fs)
```

*[Figure : voir le PDF — mli_pont.png]*

*Figure 4 – Trames relues (zoom), pont complet A − B, puis signal filtré : on retrouve la salve d’amplitude 0,8*

> **Note :** Dans un enregistrement de la **Supervision**, les voies MLI (MLI1, MLI2…) contiennent **une seule période** de la séquence : la carte la rejoue en boucle tant que l’émission dure (voir les événements mli_play / mli_stop). Dans un fichier enregistré depuis la **Configuration** (bouton disquette), la séquence est déjà répétée sur toute la durée de l’enregistrement.

## 9. Pièges et vérifications

- Toujours vérifier la signature (ACMB / ACOUSBIN) et la version avant de lire.
- ACMB : lire les colonnes **par leur nom** (clé colonnes), jamais par leur position : des colonnes optionnelles décalent les suivantes.
- ACMB : les valeurs sont rangées **ligne par ligne** (un point complet, puis le suivant). En MATLAB, lire [C, N] puis transposer.
- ACOUSBIN : les données commencent toujours à l’octet 65 536, quelle que soit la longueur du JSON (ignorer les espaces de remplissage).
- ACOUSBIN : une voie peut être répartie sur des centaines de blocs ; il faut tous les concaténer.
- ACOUSBIN : les voies ont des cadences et des t0 différents ; utiliser t = t0 + i / fs pour les superposer, jamais l’indice seul.
- Tension et Courant enregistrés depuis la Supervision : valeur efficace par seconde (1 Hz), pas la forme d’onde.
- Hydrophone : tension à l’entrée du 4480 (V) ; pour des pascals, diviser par la sensibilité de l’hydrophone et le gain du préampli.
- Trames MLI de fichiers créés avant la v58 : le t0 des voies MLI peut différer de quelques millisecondes. Les trames sont en réalité simultanées : utiliser t0 = 0 (ou le t0 de la première voie MLI) pour toutes.
- Taille : une voie u8 prend 1 octet par échantillon. 10 s à Fe = 10 MHz = 100 Mo par voie : lire par morceaux si la mémoire est limitée.
- Les fichiers sont petit-boutistes : sur une machine grand-boutiste (rare), inverser l’ordre des octets.

> **Note :** Vérification rapide d’un lecteur maison : relire le fichier de mesure et comparer la première ligne avec le CSV exporté par Acoustimeter pour la même mesure (bouton CSV) ; les valeurs doivent être identiques.
