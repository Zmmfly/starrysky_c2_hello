ENTRY(_start)
MEMORY {
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 16M
    RAM (rwx) : ORIGIN = 0x30000000, LENGTH = 128K
}
SECTIONS {
    .text : {
        KEEP(*(.startup_code))
        *(.text .text.*)
        *(.rodata .rodata.* .srodata .srodata.*)
        . = ALIGN(4);
        __fsymtab_start = .;
        KEEP(*(FSymTab))
        __fsymtab_end = .;
        . = ALIGN(4);
    } > FLASH
    .data : {
        . = ALIGN(16);
        _sdata = .;
        *(.data .data.* .sdata .sdata.*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) : {
        . = ALIGN(16);
        _sbss = .;
        *(.bss .bss.* .sbss .sbss.* COMMON)
        . = ALIGN(16);
        _ebss = .;
    } > RAM
    .stack (NOLOAD) : {
        . = ALIGN(16);
        . += 4096;
        _estack = .;
    } > RAM
    /DISCARD/ : { *(.eh_frame .eh_frame.*) }
}
ASSERT(_start == 0, "C2 reset/IRQ entry must be at zero")
ASSERT(_estack <= ORIGIN(RAM) + LENGTH(RAM), "C2 SRAM overflow")
