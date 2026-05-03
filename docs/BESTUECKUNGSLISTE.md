# Bestückungsliste und Bestückungsplan (OpenSX Servodekoder)

Hinweis: Erzeugt aus den EAGLE-Dateien im Repo. Koordinaten in mm (EAGLE-Board-Koordinaten).

## Servo-Aufsatz (PCA9685)

### BOM (nach Bauteiltyp gruppiert)

| Menge | Wert | Typ/Deviceset | Package | Referenzen |
|---:|---|---|---|---|
| 1 | 10uF | CAP_CERAMIC | 0805-NO | C1 |
| 1 | 470µF | CPOL-US | E3,5-8 | C2 |
| 1 | J1 | MA20-1 | MA20-1 | JP1 |
| 1 | J2 | MA20-1 | MA20-1 | JP2 |
| 1 | AOD417 | MOSFET-P | TO252 | Q1 |
| 1 | PCA9685 | PCA9685 | TSSOP28 | U1 |
| 3 | 10K | RESISTOR | 0805-NO | R10, R7, R9 |
| 4 | 220 | RESISTOR_4PACK | RESPACK_4X0603 | R11, R12, R13, R14 |

### Bestückungsplan (Einzelteile)

| Ref | Wert | Package | Seite | X | Y | Rotation |
|---|---|---|---|---:|---:|---|
| C1 | 10uF | 0805-NO | Top | 17.526 | 36.322 | R270 |
| C2 | 470µF | E3,5-8 | Top | 36.068 | 5.334 | R180 |
| JP1 | J1 | MA20-1 | Top | 2.54 | 30.734 | R270 |
| JP2 | J2 | MA20-1 | Top | 43.18 | 30.734 | R90 |
| Q1 | AOD417 | TO252 | Top | 23.622 | 6.35 | R90 |
| R10 | 10K | 0805-NO | Top | 21.336 | 43.688 | R90 |
| R11 | 220 | RESPACK_4X0603 | Top | 28.448 | 28.194 | R180 |
| R12 | 220 | RESPACK_4X0603 | Top | 35.814 | 50.8 | R270 |
| R13 | 220 | RESPACK_4X0603 | Top | 24.13 | 28.194 | R180 |
| R14 | 220 | RESPACK_4X0603 | Top | 35.814 | 44.196 | R270 |
| R7 | 10K | 0805-NO | Top | 23.368 | 44.196 | R90 |
| R9 | 10K | 0805-NO | Top | 18.542 | 41.656 | R270 |
| U1 | PCA9685 | TSSOP28 | Top | 24.13 | 35.814 | R0 |

## Basisplatine SXV0300_Servo

### BOM (nach Bauteiltyp gruppiert)

| Menge | Wert | Typ/Deviceset | Package | Referenzen |
|---:|---|---|---|---|
| 1 | 0R-JUMPA | 0R-JUMP | A0R-JMP | 20_OR_12V_INP |
| 1 | 0R-JUMPB | 0R-JUMP | B0R-JMP | A5_RX_JMP |
| 1 | - | 10-XX | B3F-10XX | S1 |
| 1 | 7805TV | 78* | TO220V | IC1 |
| 1 | 7812TV | 78* | TO220V | IC7 |
| 1 | - | ARDUINO_PRO_MINI | ARDUINO_PRO_MINI | ARDUINO1 |
| 2 | BC557B | BC557* | TO92-EBC | Q1, Q2 |
| 2 | 100n | C-EU | C050-025X075 | C4, C5 |
| 1 | 10uF | CPOL-EU | E2,5-7 | C1 |
| 1 | 10uF | CPOL-EU | SMC_C | C3 |
| 1 | 1uF | CPOL-EU | E2,5-7 | C2 |
| 4 | - | HEADER-3X04 | 3X04 | JP1, JP2, JP3, JP4 |
| 1 | - | LED | LED3MM | LED1 |
| 1 | LM393N | LM393 | DIL08 | IC4 |
| 2 | - | MA20-1 | MA20-1 | J1, J2 |
| 2 | MAB5SH | MAB5SH | MAB5SH | X1, X2 |
| 4 | MOUNT-HOLE2.8 | MOUNT-HOLE | 2,8 | H5, H6, H7, H8 |
| 1 | MSTBA2 | MSTBA2 | MSTBA2 | X3 |
| 1 | 0..47 | R-EU_ | 0207/12 | R1 |
| 1 | 100 | R-EU_ | 0207/10 | R4 |
| 2 | 100k | R-EU_ | 0207/10 | R11, R8 |
| 1 | 180 | R-EU_ | 0207/2V | R9 |
| 1 | 2,2k | R-EU_ | 0207/10 | R12 |
| 3 | 27k | R-EU_ | 0207/10 | R10, R13, R3 |
| 4 | 7,5k | R-EU_ | 0207/10 | R2, R5, R6, R7 |
| 1 | - | SJ | SJ | RAW_POW |

### Bestückungsplan (Einzelteile)

| Ref | Wert | Package | Seite | X | Y | Rotation |
|---|---|---|---|---:|---:|---|
| 20_OR_12V_INP | 0R-JUMPA | A0R-JMP | Top | 40.64 | 62.23 | R270 |
| A5_RX_JMP | 0R-JUMPB | B0R-JMP | Top | 27.94 | 62.23 | R270 |
| ARDUINO1 | - | ARDUINO_PRO_MINI | Top | 36.83 | 43.18 | R0 |
| C1 | 10uF | E2,5-7 | Top | 19.05 | 3.81 | R0 |
| C2 | 1uF | E2,5-7 | Top | 39.37 | 3.81 | R180 |
| C3 | 10uF | SMC_C | Top | 20.32 | 62.23 | R180 |
| C4 | 100n | C050-025X075 | Top | 44.45 | 5.08 | R270 |
| C5 | 100n | C050-025X075 | Top | 48.26 | 5.08 | R270 |
| H5 | MOUNT-HOLE2.8 | 2,8 | Top | 10.16 | 60.96 | R0 |
| H6 | MOUNT-HOLE2.8 | 2,8 | Top | 53.34 | 60.96 | R0 |
| H7 | MOUNT-HOLE2.8 | 2,8 | Top | 10.16 | 3.81 | R0 |
| H8 | MOUNT-HOLE2.8 | 2,8 | Top | 53.34 | 3.81 | R0 |
| IC1 | 7805TV | TO220V | Top | 11.43 | 57.15 | R0 |
| IC4 | LM393N | DIL08 | Top | 30.48 | 6.35 | R0 |
| IC7 | 7812TV | TO220V | Top | 11.43 | 11.43 | R0 |
| J1 | - | MA20-1 | Top | 20.32 | 33.02 | R270 |
| J2 | - | MA20-1 | Top | 60.96 | 33.02 | R90 |
| JP1 | - | 3X04 | Top | 80.01 | 58.42 | R270 |
| JP2 | - | 3X04 | Top | 80.01 | 48.26 | R270 |
| JP3 | - | 3X04 | Top | 80.01 | 38.1 | R270 |
| JP4 | - | 3X04 | Top | 80.01 | 27.94 | R270 |
| LED1 | - | LED3MM | Top | 69.85 | 5.08 | R180 |
| Q1 | BC557B | TO92-EBC | Top | 54.61 | 36.83 | R90 |
| Q2 | BC557B | TO92-EBC | Top | 54.61 | 29.21 | R90 |
| R1 | 0..47 | 0207/12 | Top | 24.13 | 17.78 | R270 |
| R10 | 27k | 0207/10 | Top | 24.13 | 34.29 | R270 |
| R11 | 100k | 0207/10 | Top | 57.15 | 46.99 | R90 |
| R12 | 2,2k | 0207/10 | Top | 57.15 | 19.05 | R270 |
| R13 | 27k | 0207/10 | Top | 54.61 | 19.05 | R90 |
| R2 | 7,5k | 0207/10 | Top | 26.67 | 46.99 | R90 |
| R3 | 27k | 0207/10 | Top | 43.18 | 13.97 | R0 |
| R4 | 100 | 0207/10 | Top | 27.94 | 19.05 | R270 |
| R5 | 7,5k | 0207/10 | Top | 43.18 | 17.78 | R180 |
| R6 | 7,5k | 0207/10 | Top | 24.13 | 46.99 | R90 |
| R7 | 7,5k | 0207/10 | Top | 29.21 | 46.99 | R90 |
| R8 | 100k | 0207/10 | Top | 54.61 | 46.99 | R90 |
| R9 | 180 | 0207/2V | Top | 59.69 | 2.54 | R0 |
| RAW_POW | - | SJ | Top | 46.99 | 62.23 | R180 |
| S1 | - | B3F-10XX | Top | 77.47 | 5.08 | R0 |
| X1 | MAB5SH | MAB5SH | Top | 7.62 | 21.59 | R270 |
| X2 | MAB5SH | MAB5SH | Top | 7.62 | 41.91 | R270 |
| X3 | MSTBA2 | MSTBA2 | Top | 80.01 | 15.24 | R90 |

