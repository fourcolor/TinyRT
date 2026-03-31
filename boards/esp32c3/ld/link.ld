MEMORY
{
    flash (r):    org = 0x00000000, len = 0x400000
    irom (x):     org = 0x42000000, len = 0x400000
    drom (r):     org = 0x3C000000, len = 0x400000
    ram (rw):     org = 0x3FC80000, len = 0x50000
    rtc_ram (rx): org = 0x50000000, len = 0x2000
}

ENTRY(_start)

SECTIONS
{
  /* Direct-boot header at flash offset 0 */
  .header :
  {
    _irom_start = .;
    LONG(0xaedb041d)
    LONG(0xaedb041d)
  } > irom AT > flash

  /* Entry stub must be at flash offset 0x8 */
  .text.entry ORIGIN(irom) + 8 :
  {
    KEEP(*(.text.entry))
  } > irom AT > flash

  /*
   * Padding to sync VMA/LMA alignment for XIP.
   * .text input sections may have up to 256-byte alignment, which would
   * advance the irom VMA (e.g. to 0x42000100) while the flash LC stays at
   * 0x2c.  For XIP, VMA - ORIGIN(irom) must equal LMA (flash offset), so
   * we must pad flash to the same 256-byte boundary here.
   */
  .text.align : { FILL(0x00); BYTE(0); . = ALIGN(256); } > irom AT > flash

  /* All executable code */
  .text :
  {
    *(.text .stub .text.* .gnu.linkonce.t.*)
    *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
    *(.gnu.warning)
  } > irom AT > flash

  . = ALIGN(4);
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);

  /* Read-only data (XIP via drom window)
   *
   * drom XIP rule: VMA - ORIGIN(drom) must equal LMA (flash offset).
   * The drom region LC starts at ORIGIN(drom) = 0x3C000000, but the flash
   * LC is already at LOADADDR(.text) + SIZEOF(.text) after all irom sections.
   * Setting the explicit VMA here syncs them; subsequent drom sections
   * placed with "> drom AT > flash" will naturally stay in sync.
   */
  .rodata ORIGIN(drom) + LOADADDR(.text) + SIZEOF(.text) :
  {
    *(.rodata .rodata.* .srodata .srodata.* .sdata2 .sdata2.* .gnu.linkonce.r.*)
    *(.rela.data .rela.data.* .rela.gnu.linkonce.r.*)
  } > drom AT > flash

  .rodata1 :
  {
    *(.rodata1)
  } > drom AT > flash

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  } > drom AT > flash

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } > drom AT > flash

  .ctors :
  {
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
  } > drom AT > flash

  .dtors :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  } > drom AT > flash

  /* Mutable data: stored in flash, copied to RAM by startup code */
  .data ORIGIN(ram) :
  {
    _data_start = .;
    __DATA_BEGIN__ = .;
    *(.data .data.* .gnu.linkonce.d.*)
    *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*) *(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*)
    SORT(CONSTRUCTORS)
  } > ram AT > flash

  .data1 :
  {
    *(.data1)
  } > ram AT > flash

  .sdata :
  {
    __SDATA_BEGIN__ = .;
    *(.sdata .sdata.* .gnu.linkonce.s.*)
  } > ram AT > flash

  . = ALIGN(4);
  _edata = .; PROVIDE (edata = .);

  /*
   * _data_lma: drom-window VMA of data in flash, used by startup memcpy.
   * LOADADDR(.data) gives the flash physical offset; adding ORIGIN(drom)
   * maps it to the drom XIP window so the CPU can read it.
   */
  _data_lma = ORIGIN(drom) + LOADADDR(.data);
  _data_size = _edata - _data_start;

  __bss_start = .;

  .sbss :
  {
    *(.dynsbss)
    *(.sbss .sbss.* .gnu.linkonce.sb.*)
    *(.scommon)
  } > ram

  .bss :
  {
   *(.dynbss)
   *(.bss .bss.* .gnu.linkonce.b.*)
   *(COMMON)
  } > ram

  . = ALIGN(4);
  __BSS_END__ = .;
  __global_pointer$ = MIN(__SDATA_BEGIN__ + 0x800,
                          MAX(__DATA_BEGIN__ + 0x800, __BSS_END__ - 0x800));

  /*
   * Shared thread pointer scratch.
   *
   * Startup initializes tp to __tp_start. Keep this area out of the heap so
   * ROM code or future TLS/reentrancy state cannot overlap malloc blocks.
   */
  . = ALIGN(64);
  __tp_start = .;
  . += 64;
  __tp_end = .;

  _end = .; PROVIDE (end = .);

  /*
   * Heap and boot/ISR stack.
   *
   * Heap starts after all static RAM sections and grows upward.
   * The boot/ISR stack lives at the top of RAM and grows downward.
   */
  . = ALIGN(16);
  __heap_start = .;
  PROVIDE (_heap_start = __heap_start);

  __stack_size_min = 0x4000;
  __stack_top = ORIGIN(ram) + LENGTH(ram);
  __heap_end = __stack_top - __stack_size_min;
  __stack_bottom = __heap_end;
  PROVIDE (_heap_end = __heap_end);

  /* Stack */
  .stack :
  {
    ASSERT(__heap_start < __heap_end, "Error: no space for heap");
    ASSERT(__stack_bottom + __stack_size_min <= __stack_top, "Error: no space for stack");
  } > ram

  /* Stabs debugging sections.  */
  .stab          0 : { *(.stab) }
  .stabstr       0 : { *(.stabstr) }
  .stab.excl     0 : { *(.stab.excl) }
  .stab.exclstr  0 : { *(.stab.exclstr) }
  .stab.index    0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment       0 : { *(.comment) }
  .gnu.build.attributes : { *(.gnu.build.attributes .gnu.build.attributes.*) }
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* DWARF 3 */
  .debug_pubtypes 0 : { *(.debug_pubtypes) }
  .debug_ranges   0 : { *(.debug_ranges) }
  /* DWARF Extension.  */
  .debug_macro    0 : { *(.debug_macro) }
  .debug_addr     0 : { *(.debug_addr) }
  .gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }
  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) }
}

/* ROM function interface esp32c3.rom.libgcc.ld for esp32c3
 *
 *
 * Generated from ./interface-esp32c3.yml md5sum 93b28a9e1fe42d212018eb4336849208
 *
 * Compatible with ROM where ECO version equal or greater to 0.
 */

/***************************************
 Group libgcc
 ***************************************/

/* Functions */
PROVIDE(__absvdi2 = 0x40000764);
PROVIDE(__absvsi2 = 0x40000768);
PROVIDE(__adddf3 = 0x4000076c);
PROVIDE(__addsf3 = 0x40000770);
PROVIDE(__addvdi3 = 0x40000774);
PROVIDE(__addvsi3 = 0x40000778);
PROVIDE(__ashldi3 = 0x4000077c);
PROVIDE(__ashrdi3 = 0x40000780);
PROVIDE(__bswapdi2 = 0x40000784);
PROVIDE(__bswapsi2 = 0x40000788);
PROVIDE(__clear_cache = 0x4000078c);
PROVIDE(__clrsbdi2 = 0x40000790);
PROVIDE(__clrsbsi2 = 0x40000794);
PROVIDE(__clzdi2 = 0x40000798);
PROVIDE(__clzsi2 = 0x4000079c);
PROVIDE(__cmpdi2 = 0x400007a0);
PROVIDE(__ctzdi2 = 0x400007a4);
PROVIDE(__ctzsi2 = 0x400007a8);
PROVIDE(__divdc3 = 0x400007ac);
PROVIDE(__divdf3 = 0x400007b0);
PROVIDE(__divdi3 = 0x400007b4);
PROVIDE(__divsc3 = 0x400007b8);
PROVIDE(__divsf3 = 0x400007bc);
PROVIDE(__divsi3 = 0x400007c0);
PROVIDE(__eqdf2 = 0x400007c4);
PROVIDE(__eqsf2 = 0x400007c8);
PROVIDE(__extendsfdf2 = 0x400007cc);
PROVIDE(__ffsdi2 = 0x400007d0);
PROVIDE(__ffssi2 = 0x400007d4);
PROVIDE(__fixdfdi = 0x400007d8);
PROVIDE(__fixdfsi = 0x400007dc);
PROVIDE(__fixsfdi = 0x400007e0);
PROVIDE(__fixsfsi = 0x400007e4);
PROVIDE(__fixunsdfsi = 0x400007e8);
PROVIDE(__fixunssfdi = 0x400007ec);
PROVIDE(__fixunssfsi = 0x400007f0);
PROVIDE(__floatdidf = 0x400007f4);
PROVIDE(__floatdisf = 0x400007f8);
PROVIDE(__floatsidf = 0x400007fc);
PROVIDE(__floatsisf = 0x40000800);
PROVIDE(__floatundidf = 0x40000804);
PROVIDE(__floatundisf = 0x40000808);
PROVIDE(__floatunsidf = 0x4000080c);
PROVIDE(__floatunsisf = 0x40000810);
PROVIDE(__gcc_bcmp = 0x40000814);
PROVIDE(__gedf2 = 0x40000818);
PROVIDE(__gesf2 = 0x4000081c);
PROVIDE(__gtdf2 = 0x40000820);
PROVIDE(__gtsf2 = 0x40000824);
PROVIDE(__ledf2 = 0x40000828);
PROVIDE(__lesf2 = 0x4000082c);
PROVIDE(__lshrdi3 = 0x40000830);
PROVIDE(__ltdf2 = 0x40000834);
PROVIDE(__ltsf2 = 0x40000838);
PROVIDE(__moddi3 = 0x4000083c);
PROVIDE(__modsi3 = 0x40000840);
PROVIDE(__muldc3 = 0x40000844);
PROVIDE(__muldf3 = 0x40000848);
PROVIDE(__muldi3 = 0x4000084c);
PROVIDE(__mulsc3 = 0x40000850);
PROVIDE(__mulsf3 = 0x40000854);
PROVIDE(__mulsi3 = 0x40000858);
PROVIDE(__mulvdi3 = 0x4000085c);
PROVIDE(__mulvsi3 = 0x40000860);
PROVIDE(__nedf2 = 0x40000864);
PROVIDE(__negdf2 = 0x40000868);
PROVIDE(__negdi2 = 0x4000086c);
PROVIDE(__negsf2 = 0x40000870);
PROVIDE(__negvdi2 = 0x40000874);
PROVIDE(__negvsi2 = 0x40000878);
PROVIDE(__nesf2 = 0x4000087c);
PROVIDE(__paritysi2 = 0x40000880);
PROVIDE(__popcountdi2 = 0x40000884);
PROVIDE(__popcountsi2 = 0x40000888);
PROVIDE(__powidf2 = 0x4000088c);
PROVIDE(__powisf2 = 0x40000890);
PROVIDE(__subdf3 = 0x40000894);
PROVIDE(__subsf3 = 0x40000898);
PROVIDE(__subvdi3 = 0x4000089c);
PROVIDE(__subvsi3 = 0x400008a0);
PROVIDE(__truncdfsf2 = 0x400008a4);
PROVIDE(__ucmpdi2 = 0x400008a8);
PROVIDE(__udivdi3 = 0x400008ac);
PROVIDE(__udivmoddi4 = 0x400008b0);
PROVIDE(__udivsi3 = 0x400008b4);
PROVIDE(__udiv_w_sdiv = 0x400008b8);
PROVIDE(__umoddi3 = 0x400008bc);
PROVIDE(__umodsi3 = 0x400008c0);
PROVIDE(__unorddf2 = 0x400008c4);
PROVIDE(__unordsf2 = 0x400008c8);


/***************************************
 Group newlib
 ***************************************/

/* Functions */
PROVIDE(memset = 0x40000354);
PROVIDE(memcpy = 0x40000358);
PROVIDE(memmove = 0x4000035c);
PROVIDE(memcmp = 0x40000360);
PROVIDE(strcpy = 0x40000364);
PROVIDE(strncpy = 0x40000368);
PROVIDE(strcmp = 0x4000036c);
PROVIDE(strncmp = 0x40000370);
PROVIDE(strlen = 0x40000374);
PROVIDE(strstr = 0x40000378);
PROVIDE(bzero = 0x4000037c);
PROVIDE(isalnum = 0x40000388);
PROVIDE(isalpha = 0x4000038c);
PROVIDE(isascii = 0x40000390);
PROVIDE(isblank = 0x40000394);
PROVIDE(iscntrl = 0x40000398);
PROVIDE(isdigit = 0x4000039c);
PROVIDE(islower = 0x400003a0);
PROVIDE(isgraph = 0x400003a4);
PROVIDE(isprint = 0x400003a8);
PROVIDE(ispunct = 0x400003ac);
PROVIDE(isspace = 0x400003b0);
PROVIDE(isupper = 0x400003b4);
PROVIDE(toupper = 0x400003b8);
PROVIDE(tolower = 0x400003bc);
PROVIDE(toascii = 0x400003c0);
PROVIDE(memccpy = 0x400003c4);
PROVIDE(memchr = 0x400003c8);
PROVIDE(memrchr = 0x400003cc);
PROVIDE(strcasecmp = 0x400003d0);
PROVIDE(strcasestr = 0x400003d4);
PROVIDE(strcat = 0x400003d8);
PROVIDE(strdup = 0x400003dc);
PROVIDE(strchr = 0x400003e0);
PROVIDE(strcspn = 0x400003e4);
PROVIDE(strcoll = 0x400003e8);
PROVIDE(strlcat = 0x400003ec);
PROVIDE(strlcpy = 0x400003f0);
PROVIDE(strlwr = 0x400003f4);
PROVIDE(strncasecmp = 0x400003f8);
PROVIDE(strncat = 0x400003fc);
PROVIDE(strndup = 0x40000400);
PROVIDE(strnlen = 0x40000404);
PROVIDE(strrchr = 0x40000408);
PROVIDE(strsep = 0x4000040c);
PROVIDE(strspn = 0x40000410);
PROVIDE(strtok_r = 0x40000414);
PROVIDE(strupr = 0x40000418);
PROVIDE(longjmp = 0x4000041c);
PROVIDE(setjmp = 0x40000420);
PROVIDE(abs = 0x40000424);
PROVIDE(div = 0x40000428);
PROVIDE(labs = 0x4000042c);
PROVIDE(ldiv = 0x40000430);
PROVIDE(qsort = 0x40000434);
PROVIDE(rand_r = 0x40000438);
PROVIDE(rand = 0x4000043c);
PROVIDE(srand = 0x40000440);
PROVIDE(utoa = 0x40000444);
PROVIDE(itoa = 0x40000448);
PROVIDE(atoi = 0x4000044c);
PROVIDE(atol = 0x40000450);
PROVIDE(strtol = 0x40000454);
PROVIDE(strtoul = 0x40000458);

PROVIDE(uart_tx_one_char = 0x40000068);
PROVIDE(usb_uart_tx_one_char = 0x400008d8);
PROVIDE(usb_uart_tx_flush = 0x400008d4);
PROVIDE(ets_install_putc1 = 0x40000044);
PROVIDE(printf = 0x40000040);

PROVIDE(longjmp = 0x4000041c);
PROVIDE(setjmp = 0x40000420);
PROVIDE (USB_SERIAL_JTAG = 0x60043000);

/* Functions */
PROVIDE (esprv_intc_int_set_priority = 0x400005e0);
PROVIDE (esprv_intc_int_set_threshold = 0x400005e4);
PROVIDE (esprv_intc_int_enable = 0x400005e8);
PROVIDE (esprv_intc_int_disable = 0x400005ec);
PROVIDE (esprv_intc_int_set_type = 0x400005f0);
PROVIDE (intr_matrix_set = 0x400005f4);
PROVIDE (ets_intr_lock = 0x400005f8);
PROVIDE (ets_intr_unlock = 0x400005fc);
PROVIDE( intr_handler_set = 0x40000600 );
PROVIDE (ets_isr_attach = 0x40000604);
PROVIDE (ets_isr_mask = 0x40000608);
PROVIDE (ets_isr_unmask = 0x4000060c);
