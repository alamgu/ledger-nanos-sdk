/*PHDRS
{
	headers PT_PHDR PHDRS ;
	text PT_LOAD PHDRS ;
	data PT_LOAD ;
	sram PT_LOAD ;
}
*/

SECTIONS
{
  .text :
  {
    _text = .;
    _nvram_start = .;

    *(.boot*)
    *(.text*)
    /* .rodata is moved to nvm_data so we can update it */

    _etext = .;
  } > FLASH :text


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
  } > SRAM AT > FLASH :data
  _sidata = LOADADDR(.data);

  .rel_flash : ALIGN(PAGE_SIZE) {
	  . = ALIGN(PAGE_SIZE);
	  _relocs = .;
	  . += _reloc_size;
	  /*(.rel.rodata .rel.rodata*)*/
	  . = ALIGN(PAGE_SIZE);
	  _relocs_end = .;
  } > FLASH :text /* > SRAM /* > FLASH */

  .nvm_data : ALIGN(PAGE_SIZE)
  {
    . = ALIGN(PAGE_SIZE);
    /* Moved here from .text so we can update it with nvm_write() */
    _nvram_data = .;
    _rodata = .;
    _rodata_src = .;
    *(.rodata*)
    . = ALIGN(PAGE_SIZE);
    _erodata = .;
    _rodata_len = ABSOLUTE(_erodata) - ABSOLUTE(_rodata);
    *(.nvm_data*)
    _nvram_end = .;
    _envram_data = .;
    . = ALIGN(PAGE_SIZE);
    _install_parameters = .;
  } > FLASH :text

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
  } > SRAM :sram
  

  .stack_sizes (INFO):
  {
    KEEP(*(.stack_sizes));
  } :NONE

  /*
  .rel (INFO):
  {
    KEEP(*(.rel.*));
  } :NONE
  */



  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    *(.ARM.extab*)
    /* *(.debug_info) */
  }
}

PROVIDE(_nvram = ABSOLUTE(_nvram_start));
PROVIDE(_envram = ABSOLUTE(_nvram_end));
