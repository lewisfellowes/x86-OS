ENTRY(_start)

SECTIONS
{
  . = 0x00100000;

  .text   : { *(.text*)   }
  .rodata : { *(.rodata*) }
  .data   : { *(.data*)   }

  .bss (NOLOAD) : {
    *(COMMON)
    *(.bss*)
  }

  . = ALIGN(4096);
  _kernel_end = .;
}
