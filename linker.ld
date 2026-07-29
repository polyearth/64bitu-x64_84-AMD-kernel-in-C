ENTRY(_start)

SECTIONS
{
    /* Sākam ielādi no 1MB, standarta vieta kernelim */
    . = 1M;

    /* Koda sadaļa */
    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* Tikai lasāmie dati (Tavi jaunie komandu teksti dzīvos šeit) */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Mainīgie un dati ar sākotnējām vērtībām */
    .data : ALIGN(4K)
    {
        *(.data)
    }

    /* Tukšie mainīgie (kā cmd_buffer), ko inicializē ar nullēm */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
