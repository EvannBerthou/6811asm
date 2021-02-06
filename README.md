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

Sources :
- [Résumé des instructions](http://www.science.smith.edu/dftwiki/images/9/9e/CSC270_Assembly_Instructions.pdf)
- [Tableau avec les instructions](http://www.dee.ufrj.br/microproc/HC11/68hc11ur.pdf)
- [Manuel de référence](https://www.nxp.com/docs/en/reference-manual/M68HC11RM.pdf)

Une instruction est composée d'un opcode et optionnellement d'un operand.
Il existe différent type d'operand :

- Immediate (**#**) : définie une constante.
Exemple : lda #0 -> charge la valeur 0 dans le registre A. La taille maximum de l'operand dépend de la taille du registre (acc a : 8 bits, acc x : 16 bits).
- Extended (**$**) Utilise 2 bytes afin de pointer vers une adresse mémoire qui contient l'operand.
- Direct (**$**) : Récupère uniquement le byte inférieurs (0x00 est assumé pour le byte supérieur) afin d'accéder à la zero-page(ou direct page) de la mémoire (0x00 - 0xFF). Permet d'utiliser un byte de moins de mémoire ce qui réduit d'un cycle l'accès.
- Indexed (indx, indy) :
- Inherent : L'operand est déjà connu par le cpu, c'est par exemple le cas de l'instruction LDA, qui pointe déjà vers l'accumulateur A.
r
- Relative : Est uniquement utilisé par les instructions de branches. Est 1 seul byte signe (--127 à +127) et définie une distance relative vers laquelle le programme doit aller. Par exemple un **BRA** $10 avance de 0x10 instructions. Si l'operand est 0x00, alors le saut on ne saute pas et on passe à la prochaine instruction.

Un opcode est différent en fonction de mode d'adressage de son operand.

Par exemple un ```LDA #4``` donnera ```86 04``` mais un ```LDA $5``` donnera ```96 05```.

## TODO
- Assembleur
    - Conversion du code en opcode
    - Lecture de fichier
    - Ajoute des constantes (equ)
    - Ajout des ports
    - Addressing mode

- Execution
    - Affichage du contenue
    - Execution instruction par instruction

- Tests
    - Programme de tests afin de tester le programme de tous les côtés
