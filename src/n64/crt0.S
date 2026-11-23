/* crt0.S – Einstieg nach libdragons IPL3 und Interrupt-Handler für den Bildwechsel (VI).
 * IPL3 hat RDRAM gelöscht, $sp ans Ende des Speichers gesetzt und springt nach _start. */
    .set noreorder
    .set noat
    .set mips3

    .section .text.entry, "ax"
    .global _start
_start:
    /* .bss löschen */
    la      $t0, __bss_start
    la      $t1, __bss_end
1:  beq     $t0, $t1, 2f
    nop
    sw      $zero, 0($t0)
    b       1b
    addiu   $t0, $t0, 4
2:
    /* Daten-Cache zurückschreiben und leeren (8 KB, 16-Byte-Zeilen) */
    lui     $t0, 0x8000
    ori     $t1, $t0, 0x2000
3:  cache   0x01, 0($t0)
    addiu   $t0, $t0, 16
    bne     $t0, $t1, 3b
    nop
    /* Stapel: 16 Byte Luft unter dem von IPL3 gesetzten Ende */
    addiu   $sp, $sp, -16
    jal     main
    nop
4:  b       4b
    nop

/* Wird nach 0x80000180 kopiert. Benutzt nur k0/k1. VI-Interrupt: Bildanfang je Halbbild setzen
 * (Zeilensprung: gerades Halbbild beginnt eine Zeile tiefer) und Zähler erhöhen. */
    .global vi_handler
    .global vi_handler_ende
vi_handler:
    mfc0    $k0, $13            /* Cause */
    andi    $k0, $k0, 0x7c      /* ExcCode */
    beqz    $k0, 5f
    nop
    j       absturz
    nop
5:  lui     $k1, 0xA440
    lw      $k0, 0x10($k1)      /* VI_V_CURRENT */
    andi    $k0, $k0, 1
    sll     $k0, $k0, 2
    lui     $k1, %hi(vi_anfang)
    addu    $k1, $k1, $k0
    lw      $k0, %lo(vi_anfang)($k1)
    lui     $k1, 0xA440
    sw      $k0, 4($k1)         /* VI_ORIGIN */
    sw      $zero, 0x10($k1)    /* Interrupt quittieren */
    lui     $k1, %hi(vi_zaehler)
    lw      $k0, %lo(vi_zaehler)($k1)
    addiu   $k0, $k0, 1
    sw      $k0, %lo(vi_zaehler)($k1)
    eret
    nop
vi_handler_ende:

    .global absturz
absturz:
    /* Unerwartete Ausnahme: Bildschirm rot lassen und stehen bleiben */
    lui     $k1, 0xA440
    sw      $zero, 0($k1)
6:  b       6b
    nop
