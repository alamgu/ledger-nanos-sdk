SECTIONS
{
  .text :
  {
    _text = .;
    _nvram_start = .;

    *(.boot*)
    *(.text*)
    *(.rodata*) 

    _etext = .;
  } > FLASH

  .data : ALIGN(4)
  {
    _data = .;
    *(vtable)
    *(.data*)
    _edata = .;
    _data_len = ABSOLUTE(_edata) - ABSOLUTE(_data);
    FILL(0xa4a4);
    /* Calculate padding so that .text + .data is a multiple of 64, as they
     * will be concatenated into flash for the app image. */
    . += (64 - (ABSOLUTE(_etext) - ABSOLUTE(_text) + ABSOLUTE(_edata) - ABSOLUTE(_data)) % 64) % 64;
  } > SRAM AT > FLASH
  _sidata = LOADADDR(.data);

  .nvm_data : ALIGN(PAGE_SIZE)
  {
    . = ALIGN(PAGE_SIZE);
    _nvram_data = .;
    *(.nvm_data*)
    . = ALIGN(PAGE_SIZE);
    _envram_data = .;
    _install_parameters = .;
    _nvram_end = .;
  } > FLASH 

   .bss :
  {
    _bss = .;
    *(.bss*)
    _ebss = .;
    _bss_len = ABSOLUTE(_ebss) - ABSOLUTE(_bss);

    . = ALIGN(4);
    app_stack_canary = .;
    . += 4;
    _stack_validation = .;
    . = _stack_validation + STACK_SIZE;
    _stack = ABSOLUTE(END_STACK) - STACK_SIZE;
    _estack = ABSOLUTE(END_STACK);
  } > SRAM

  .stack_sizes (INFO):
  {
    KEEP(*(.stack_sizes));
  }

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    *(.ARM.extab*)
    *(.debug_info)
  }
}

PROVIDE(_nvram = ABSOLUTE(_nvram_start));
PROVIDE(_envram = ABSOLUTE(_nvram_end));
