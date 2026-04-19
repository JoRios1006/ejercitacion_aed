/* tiny.ld */
ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5); /* R-X: Lectura y Ejecución */
}

SECTIONS {
    . = 0x400000 + SIZEOF_HEADERS;
    .text : { *(.text*) *(.rodata*) } :text
    .data : { *(.data*) *(.bss*) } :text
    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment*) }
}
