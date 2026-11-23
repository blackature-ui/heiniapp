/* n64.ld – statisches ELF für libdragons IPL3, geladen ab 0x80000400 */
OUTPUT_FORMAT("elf32-tradbigmips")
OUTPUT_ARCH(mips)
ENTRY(_start)

PHDRS
{
    text PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    . = 0x80000400;
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    } :text
    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    } :text
    .data : ALIGN(16) {
        *(.data .data.*)
        *(.sdata .sdata.*)
        *(.got)
    } :data
    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } :data
    . = ALIGN(16);
    __heap_start = .;
    /DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.comment) *(.pdr) *(.gnu.attributes) }
}
