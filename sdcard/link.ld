
MEMORY
{
  Flash_M4 (rx) : 	ORIGIN = 0x1a000000, LENGTH = 0x80000
  RAM_M4 (rwx) : 	ORIGIN = 0x10080000, LENGTH = 0xA000
  RAM_extra (rwx):  ORIGIN = 0x10000000, LENGTH = 0x8000
}

/* Define a symbol for the top of each memory region */
__top_Flash_M4 = ORIGIN(Flash_M4) + LENGTH(Flash_M4);
__top_RAM_M4 = ORIGIN(RAM_M4) + LENGTH(RAM_M4);
__top_RAM_extra = ORIGIN(RAM_extra) + LENGTH(RAM_extra);


SECTIONS
{
    /* BSS section for RAM_M0_global */
    .extra_bss (NOLOAD) : ALIGN(4)
    {
        __RAM_M0_global_begin__ = ABSOLUTE(.) ;
        *(.bss.$extra_bss*)
        . = ALIGN(4) ;
} > RAM_extra
. = __top_RAM_extra ;
__RAM_extra_end__ = ABSOLUTE(.);

}
