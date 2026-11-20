# Fichiers .bin d’Acoustimeter

*Structure détaillée : octets, colonnes, en-têtes, et construction d’un fichier*

*Formats ACMB v1 et ACOUSBIN v1 · Acoustimeter v58*

Ce document décrit chaque octet des fichiers, la signification et le calcul de chaque colonne, et la façon dont Acoustimeter construit les fichiers. Il donne aussi un programme qui fabrique des fichiers compatibles. Exemples d’octets tirés de vrais fichiers ; programmes vérifiés (relecture identique octet pour octet, et lecture par Acoustimeter).

## 1. Les briques de base

Les fichiers .bin sont des suites d’octets. Chaque information a un **type** de taille fixe. Tous les nombres sont écrits en **petit-boutiste** : l’octet de poids faible en premier (ordre natif des PC).

| Type | Taille | Contenu | Exemple (octets dans le fichier) |
|---|---|---|---|
| char[n] | n octets | texte ASCII fixe (signature) | « ACMB » → 41 43 4D 42 |
| uint32 | 4 octets | entier 0 à 4 294 967 295, petit-boutiste | 1 → 01 00 00 00 ; 65536 → 00 00 01 00 |
| float32 | 4 octets | réel IEEE 754 simple précision (~7 chiffres) | 1.0 → 00 00 80 3F |
| float64 | 8 octets | réel IEEE 754 double précision (~16 chiffres) | 1.0 → 00 00 00 00 00 00 F0 3F ; NaN → 00 00 00 00 00 00 F8 7F |
| uint8 | 1 octet | 0 ou 1 pour les trames MLI | 1 → 01 |
| UTF-8 | variable | texte avec accents (é = 2 octets C3 A9) | « é » → C3 A9 |

> **Note :** Les vidages d’octets de ce document se lisent ainsi : position (en hexadécimal) · 16 octets en hexadécimal · les mêmes octets en texte (un point quand l’octet n’est pas un caractère imprimable).

## 2. Fichier de mesure ACMB

### 2.1 Les octets un par un

| Position | Taille | Type | Contenu | Valeur dans l’exemple |
|---|---|---|---|---|
| 0 | 4 | char[4] | signature | « ACMB » |
| 4 | 4 | uint32 | version | 1 |
| 8 | 4 | uint32 | nombre de colonnes C | 32 |
| 12 | 4 | uint32 | nombre de points N | 71 |
| 16 | 4 | uint32 | taille L de l’en-tête | 9779 |
| 20 | L | UTF-8 | en-tête « clé=valeur » | mesure=TVR/TPR/… |
| 20 + L = 9799 | 8·C·N | float64 | valeurs, point par point | 10000.0, 1.0, 7.06, … |

Début du fichier d’exemple : signature, version 1, C = 0x20 = 32, N = 0x47 = 71, L = 0x2633 = 9779, puis le texte de l’en-tête.

```
00000000  41 43 4D 42 01 00 00 00 20 00 00 00 47 00 00 00  ACMB.... ...G...
00000010  33 26 00 00 6D 65 73 75 72 65 3D 54 56 52 2F 54  3&..mesure=TVR/T
00000020  50 52 2F 61 64 6D 69 74 74 61 6E 63 65 20 28 62  PR/admittance (b
```

Début des valeurs, position 20 + L = 9799 (0x2647) : f_Hz = 10000.0, niveau = 1.0, Urms = 7.0603 V, Irms = 0.00794 A…

```
00002647  00 00 00 00 00 88 C3 40 00 00 00 00 00 00 F0 3F  .......@.......?
00002657  EA 76 2E B9 B6 3D 1C 40 A2 BD AA 52 E9 41 80 3F  .v...=.@...R.A.?
```

### 2.2 Taille et position d’une valeur

**Taille du fichier** = 20 + L + 8 × C × N. Exemple : 20 + 9779 + 8 × 32 × 71 = **27 975 octets** (taille réelle du fichier). **Position de la valeur** du point i (0 = premier) et de la colonne j : **20 + L + 8 × (i × C + j)**. Exemple : TVR (j = 8) du premier point → 20 + 9779 + 64 = 9863.

> **Note :** Une taille de fichier différente de la formule signale un fichier tronqué ou modifié.

### 2.3 Les colonnes

Ordre par défaut (32 colonnes). La liste exacte d’un fichier est toujours celle de la clé **colonnes** de son en-tête. Si le programme est compilé avec l’option ACOUSTIMETER_TCR_Z, deux colonnes s’ajoutent : TCR_dB_re_1uPa_m_per_A après TVR, et Z_ohm après TPR.

| n° | Colonne | Unité | Comment elle est obtenue | NaN si… |
|---|---|---|---|---|
| 0 | f_Hz | Hz | fréquence de la salve | jamais |
| 1 | niveau_5413_V_crete | V crête / % | niveau demandé au générateur (classe AB) ou profondeur MLI en % (classe D) | jamais |
| 2 | Urms_V | V | valeur efficace de la composante à f de U (DFT sur la fenêtre d’analyse), moyennée sur les salves | pas de sonde U |
| 3 | Irms_A | A | idem pour I | pas de sonde I |
| 4 | P_W | W | puissance active moyenne (U·I sur la fenêtre) | U ou I absent |
| 5 | cos_phi | — | P / S | U ou I absent |
| 6 | V_hydro_eff_V | V | valeur efficace à f du signal hydrophone (arrivée directe) | pas d’hydrophone |
| 7 | SL_dB_re_1uPa_m | dB | Lp + 20 log d (+ correction de cuve, + absorption si cochée) ; Lp = 20 log Vh − M(f) − G | pas de sensibilité M |
| 8 | TVR_dB_re_1uPa_m_per_V | dB | SL − 20 log Urms | SL ou U absent |
| 9 | TPR_dB_re_1uPa_m_per_sqrtW | dB | SL − 10 log P | P ≤ 0 |
| 10–12 | G_S, B_S, Y_S | S | admittance Y = I/U à f : partie réelle, imaginaire, module | U ou I absent |
| 13–14 | Q_var, S_VA | var, VA | puissances réactive et apparente | U ou I absent |
| 15 | SLmax_dB_re_1uPa_m | dB | TVR + 20 log Umax | Umax non renseigné |
| 16 | Lpmax_Umax_dB_re_1uPa | dB | Lp + 20 log (Umax / Urms) | Umax non renseigné |
| 17 | absorption_dB | dB | α(f) × d (Ainslie-McColm), ajoutée à SL, TVR, TPR | correction non cochée |
| 18–26 | THDF / SFDR / THDN de U, I, hydro | %, dB, % | distorsion harmonique (N harmoniques), plage dynamique, THD+N ; THD en moyenne quadratique sur les salves | signal absent |
| 27 | angle_deg | ° | angle de la table tournante | hors directivité |
| 28 | directivite_dB | dB | niveau relatif au maximum (ou à la référence choisie) | hors directivité |
| 29 | passe | — | numéro de passage (répétitions, niveau en linéarité) | jamais |
| 30 | temps_s | s | instant du point depuis le début de la mesure | jamais |
| 30+k | T_<nom>_C | °C | une colonne par sonde de température cochée, insérée après temps_s | sonde non lue |
| dernière | salves | — | nombre de salves réellement moyennées | jamais |

Notations : M(f) = sensibilité de l’hydrophone en dB re 1 V/µPa (interpolée à f), G = gain du préampli en dB, d = distance en m, Umax = tension maximale du transducteur (V eff). Toutes les grandeurs électriques sont calculées sur la composante à f (DFT), sur la fenêtre d’analyse (après les cycles d’établissement), puis moyennées sur les salves.

### 2.4 Les clés de l’en-tête

Une clé par ligne, dans cet ordre. Les valeurs sont du texte (nombres avec un point décimal). Les clés bvd_* et remarque_k ne sont présentes que si elles existent.

| Clé | Format de la valeur | Exemple |
|---|---|---|
| mesure | texte | TVR/TPR/admittance (balayage en frequence) \| U=f(I) … \| linearite … \| directivite … |
| date | ISO 8601 | 2026-11-20T10:02:41 |
| simulation | 0 / 1 | 1 = mode hors ligne |
| ampli | entier | 1 |
| classe | texte | AB, ou D (MLI PXIe-6537, porteuse … kHz, filtre MLI …) |
| sortie_5413 | texte | Générateur/ao0 |
| sonde_U, sonde_I | texte | V_AMP1_1, I_AMP1_1 |
| hydrophone | texte | H1 TC4034 |
| gain_preampli_dB | réel | 20 |
| distance_m, profondeur_hydro_m, profondeur_source_m | réel (m) | 1.5, 2, 2 |
| celerite_m_s | réel | 1480 |
| correction_cuve | 0 / 1 |  |
| niveau_5413_V_crete | réel | 1 |
| etablissement_cycles | entier | 5 |
| moyenne_salves | entier | 8 |
| periode_ms | réel | 50 |
| absorption | texte | corrigee (T eau 15 degC, salinite 0 psu, Ainslie-McColm) |
| Umax_V_eff | réel | 100 |
| Lp_max_global_dB_re_1uPa | texte | 176.52 a 30000.0 Hz |
| signal | texte | sinus (pas a pas) |
| pulse_ms, inter_pulse_ms | réel | 1, 50 |
| distorsion | texte | N 5 harmoniques ; bande THD+N f/2 a (N+1/2).f |
| temperatures | liste | T ampli 1,T eau |
| arret_si_T_sup_C | réel | 80 |
| directivite_de | texte | référence et normalisation |
| colonnes | liste séparée par des virgules | f_Hz,niveau_5413_V_crete,…,salves (C noms) |
| bvd_fr_Hz, bvd_f1_Hz, bvd_f2_Hz, bvd_Q, bvd_Rm_ohm, bvd_C0_F, bvd_Lm_H, bvd_Cm_F | réel | schéma équivalent, si l’ajustement a réussi |
| remarque_k | texte | remarque du point k (k = 0 pour la première ligne) |

## 3. Fichier d’enregistrement ACOUSBIN

### 3.1 Les octets un par un

| Position | Taille | Type | Contenu |
|---|---|---|---|
| 0 | 8 | char[8] | signature « ACOUSBIN » |
| 8 | 4 | uint32 | version = 1 |
| 12 | 4 | uint32 | taille H de la zone d’en-tête = 65 536 |
| 16 | H − 16 | UTF-8 | JSON compact, puis des espaces jusqu’à la position H |
| H | 8 | uint32 × 2 | bloc : index de la voie, nombre n de valeurs |
| H + 8 | n × 4 ou n × 1 | float32 ou uint8 | les n valeurs du bloc |
| … |  |  | bloc suivant, et ainsi de suite jusqu’à la fin du fichier |

Début d’un fichier de trames MLI : signature, version 1, H = 00 00 01 00 = 65 536, puis le JSON.

```
00000000  41 43 4F 55 53 42 49 4E 01 00 00 00 00 00 01 00  ACOUSBIN........
00000010  7B 22 64 65 62 75 74 22 3A 22 32 30 32 36 2D 31  {"debut":"2026-1
00000020  31 2D 32 30 54 31 30 3A 30 37 3A 30 39 22 2C 22  1-20T10:07:09","
```

Premier bloc, position 65 536 (0x10000) : voie 0, n = 0x00030D40 = 200 000 valeurs uint8 (0 ou 1).

```
00010000  00 00 00 00 40 0D 03 00 01 01 01 01 01 01 01 01  ....@...........
00010010  01 01 01 01 01 00 00 00                          ........
```

### 3.2 L’en-tête JSON

| Champ | Type | Rôle |
|---|---|---|
| titre | texte | « Acoustimeter » (Supervision) ou « Trames MLI ampli n » |
| debut | texte ISO 8601 | date et heure de début |
| voies | liste d’objets | une entrée par voie ; l’index dans cette liste est l’index utilisé dans les blocs |
| voies[i].nom / groupe / unite | texte | voir le tableau des noms |
| voies[i].fs | réel (Hz) | cadence de la voie |
| voies[i].t0 | réel (s) | instant du premier échantillon |
| voies[i].dtype | « f32 » ou « u8 » | type des valeurs dans les blocs |
| evenements | liste d’objets | { t (s), type, detail } — 800 au plus |
| meta | objet | informations libres (WAV de la configuration, réglages MLI…) |

### 3.3 Noms des voies

| Origine | Nom de la voie | Groupe | Unité | dtype |
|---|---|---|---|---|
| Sonde de température | libellé de la sonde (ex. « T ampli 1 ») | Température | °C | f32 |
| Sonde de tension / courant | nom du tracé (ex. « V_AMP1_1 », « I_AMP1_1 ») | Tension / Courant | V / A | f32 |
| Hydrophone | « Hydro n » | Hydrophone | V | f32 |
| Trames MLI (Supervision) | « MLIk » (k = sortie 1 à 32 du 6537) | MLI | bit | u8 |
| Signal AB (Supervision) | « AB Ampli n » | Analogique AB | V | f32 |
| Trames MLI (Configuration) | « An (MLIk) », ex. « A1 Bras A — haut (MLI1) » | MLI | 0/1 | u8 |

### 3.4 Les blocs

*[Figure : voir le PDF — blocs.png]*

*Figure 1 – Les blocs de plusieurs voies se suivent dans l’ordre d’arrivée ; à la lecture, on les remet bout à bout par voie*

- Un bloc = 8 octets d’en-tête (voie, n) + n valeurs. Taille d’un bloc : 8 + 4n (f32) ou 8 + n (u8).
- Il n’y a pas d’index des blocs : pour trouver les données d’une voie, il faut parcourir les blocs depuis la position 65 536.
- Taille du fichier = 65 536 + somme des tailles des blocs.
- Une voie peut ne contenir aucun bloc (voie déclarée mais sans données) : elle est alors vide.

### 3.5 Comment Acoustimeter construit le fichier

1. À l’ouverture : écriture de la signature, de la version, de H = 65 536 et d’un en-tête provisoire (sans voie) complété par des espaces. Les données commenceront toujours à la position 65 536.
2. Pendant l’enregistrement : chaque paquet de mesures reçu (bloc de l’hydrophone, valeur de température, valeur efficace par seconde de U ou I, trames MLI…) est ajouté à la fin du fichier sous forme de bloc. Une voie est déclarée la première fois qu’elle reçoit des données.
3. Instant t0 d’une voie : pour les mesures en continu, c’est l’instant du premier bloc moins sa durée (n / fs) ; pour les trames MLI et le signal AB, c’est l’instant où la sortie a été lancée (toutes les trames MLI ont le même t0 depuis la v58).
4. Les événements (début, émission, lecture MLI…) sont datés en secondes depuis l’ouverture et gardés en mémoire.
5. À la fermeture (STOP, durée atteinte) : l’en-tête est réécrit au début du fichier avec la liste finale des voies, leurs t0 et fs, les événements et meta.

> **Attention :** Conséquence : si Acoustimeter s’arrête brutalement pendant un enregistrement, l’en-tête reste provisoire (aucune voie). Les blocs sont dans le fichier mais on ne sait plus à quelle voie, quelle cadence et quel instant ils correspondent.

## 4. Construire un fichier soi-même

Le module `acoustimeter_bin_ecriture.py` écrit les deux formats exactement comme Acoustimeter. Utile pour fabriquer des fichiers de test, convertir des données d’un autre appareil, ou réécrire un fichier modifié.

```
"""Ecriture de fichiers .bin au format Acoustimeter (ACMB v1 et ACOUSBIN v1), Python 3 + numpy.
Les fichiers produits sont lus par Acoustimeter (ACOUSBIN : fenetre Analyse) et par acoustimeter_bin.py."""
import json
import struct
import numpy as np

ACOUSBIN_ENTETE = 65536                               # taille fixe de la zone d'en-tete

def write_acmb(path, entete, colonnes, donnees):
    """entete : dict ordonne cle -> valeur (texte) ; la cle 'colonnes' est (re)ecrite a partir de `colonnes`.
    donnees : tableau [N points x C colonnes] (NaN = non mesure)."""
    d = np.asarray(donnees, dtype="<f8")
    if d.ndim != 2 or d.shape[1] != len(colonnes):
        raise ValueError("donnees : il faut une colonne par nom de colonne")
    lignes = []
    for k, v in entete.items():
        lignes.append(f"{k}={','.join(colonnes) if k == 'colonnes' else v}")
    if "colonnes" not in entete:
        lignes.append("colonnes=" + ",".join(colonnes))
    texte = "\n".join(lignes).encode("utf-8")
    with open(path, "wb") as f:
        f.write(b"ACMB")
        f.write(struct.pack("<4I", 1, d.shape[1], d.shape[0], len(texte)))
        f.write(texte)
        f.write(np.ascontiguousarray(d).tobytes())        # ligne par ligne (ordre C)

class AcousbinWriter:
    """Ecriture progressive d'un enregistrement : open -> voie() -> ajoute()... -> close()."""

    def __init__(self, path, titre="Acoustimeter", debut="2026-01-01T00:00:00", meta=None):
        self.f = open(path, "wb")
        self.titre, self.debut, self.meta = titre, debut, meta or {}
        self.voies, self.evenements = [], []
        self.f.write(b"\0" * ACOUSBIN_ENTETE)              # zone d'en-tete reservee, ecrite a la fermeture

    def voie(self, nom, groupe, unite, fs, t0=0.0, dtype="f32"):
        """Declare une voie et renvoie son index (0, 1, 2...)."""
        self.voies.append({"nom": nom, "groupe": groupe, "unite": unite, "fs": float(fs), "t0": float(t0), "dtype": dtype})
        return len(self.voies) - 1

    def ajoute(self, index, valeurs):
        """Ajoute un bloc de valeurs a la voie `index` (blocs de voies differentes : ordre libre)."""
        u8 = self.voies[index]["dtype"] == "u8"
        x = np.asarray(valeurs, dtype=np.uint8 if u8 else "<f4")
        self.f.write(struct.pack("<2I", index, x.size))
        self.f.write(x.tobytes())

    def evenement(self, t, type_, detail=""):
        self.evenements.append({"t": float(t), "type": type_, "detail": detail})

    def close(self):
        js = json.dumps({"titre": self.titre, "debut": self.debut, "voies": self.voies,
                         "evenements": self.evenements, "meta": self.meta}, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        if len(js) > ACOUSBIN_ENTETE - 16:
            raise ValueError("en-tete JSON trop long (trop de voies ou d'evenements)")
        self.f.seek(0)
        self.f.write(b"ACOUSBIN" + struct.pack("<2I", 1, ACOUSBIN_ENTETE))
        self.f.write(js + b" " * (ACOUSBIN_ENTETE - 16 - len(js)))
        self.f.close()
```

### Exemples

```
import numpy as np
import acoustimeter_bin as lecture
import acoustimeter_bin_ecriture as ecriture

# 1) Relire une mesure, corriger une colonne, la réécrire
h, col, d = lecture.read_acmb("mesure.bin")
d = d.copy()                                          # le tableau lu est en lecture seule
d[:, col.index("TVR_dB_re_1uPa_m_per_V")] += 0.5          # exemple : recalage de 0,5 dB
h["remarque_0"] = "TVR recale de +0,5 dB"
ecriture.write_acmb("mesure_corrigee.bin", h, col, d)

# 2) Fabriquer un enregistrement avec un hydrophone, une température et une trame MLI
fs = 51200; t = np.arange(fs) / fs
w = ecriture.AcousbinWriter("essai.bin", titre="Essai", debut="2026-11-20T10:30:00", meta={"origine": "script"})
hyd = w.voie("Hydro 1", "Hydrophone", "V", fs)
tmp = w.voie("T eau", "Température", "°C", 2)
mli = w.voie("MLI1", "MLI", "bit", 1e6, dtype="u8")
for k in range(4):                                  # blocs entremêlés, comme pendant un enregistrement
    w.ajoute(hyd, 0.1 * np.sin(2 * np.pi * 1000 * t[k*12800:(k+1)*12800]))
    w.ajoute(tmp, [20 + 0.1*k, 20.05 + 0.1*k])
w.ajoute(mli, (np.arange(1000) % 4 < 2).astype(np.uint8))
w.evenement(0, "debut", "script"); w.evenement(1.0, "fin", "script")
w.close()
```

### Vérifications faites

- ACMB : un fichier réel relu puis réécrit avec write_acmb est **identique octet pour octet** à l’original.
- ACOUSBIN : le fichier « essai.bin » ci-dessus est lu sans erreur par le lecteur d’Acoustimeter lui-même (code C++ de la fenêtre Analyse) : 3 voies, 2 événements ; Hydro 1 = 51 200 valeurs (valeur efficace 0,0707 V), T eau = 8 valeurs, MLI1 = 1 000 valeurs.

### Règles pour qu’un fichier soit accepté

- Signature et version exactes ; nombres en petit-boutiste.
- ACMB : la clé colonnes doit avoir exactement C noms ; les valeurs sont rangées point par point (C valeurs, puis le point suivant).
- ACOUSBIN : la zone d’en-tête fait exactement 65 536 octets ; le JSON (UTF-8) tient dans 65 520 octets, complété par des espaces.
- ACOUSBIN : chaque bloc référence une voie existante (index 0 ; dtype « f32 » ou « u8 ».
- Pour être affichée correctement par la fenêtre Analyse, une voie doit utiliser l’un des groupes connus : Température, Tension, Courant, Hydrophone, MLI, Analogique AB.
