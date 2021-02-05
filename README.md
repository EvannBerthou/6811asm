# Emulateur 6811

## Fonctionnement

### Registres :
    A,B: 8bits -> Accumulateurs
    D = A + B  -> Accumulateur
    X,Y: 16 bits -> Registres d'index
    Stack Pointer (SP): 16 bits
    Program Counter (CP): 16 bits -> Pointe vers l'instruction en cours d'execution. S'autoincrémente
    Status registre: 8 bits -> Registre qui sert a retenir les flags.
        0 - 7:
        Carry, overflow, zero, negative, mask IRQ, half-carry, mask XIRQ, stop disable

### Memory map:
    $0000 - $00FF: RAM
    $0100 - $0FFF: Non utilisé
    $1000 - $103F: Registres spéciaux
    $1040 - $F7FF: Non utilisé
    $F800 - $FFFF: EEPROM


La ram est utilisée afin de stocker des informations durant le déroulement du programme (256 bytes).
Les registres spéciaux servent à controller et définir le fonctionnements des ports.
Les parties non utilisés peuvent servir à stocker le programme.
Le programme est contenu das dans les parties non utilisés ainsi que dans l'EEPROM.

### Instructions

<http://www.science.smith.edu/dftwiki/images/9/9e/CSC270_Assembly_Instructions.pdf>

## TODO
- Assembleur
    - Conversion du code en opcode
    - Lecture de fichier
    - Ajoute des constantes (equ)
    - Ajout des ports

- Execution
    - Affichage du contenue
    - Execution instruction par instruction

- Tests
    - Programme de tests afin de tester le programme de tous les côtés
