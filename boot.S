.section .multiboot
.align 8
multiboot_header:
    .long 0xe85250d6
    .long 0
    .long multiboot_header_end - multiboot_header
    .long -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header))
    .short 0
    .short 0
    .long 8
multiboot_header_end:

.section .text
.code32

.global keyboard_handler_asm  /* Šo mēs iedosim C kodam */
.extern keyboard_handler      /* Šo C funkciju mēs izsauksim no šejienes */

.global mouse_handler_asm     /* JAUNS: Peles stabs C kodam */
.extern mouse_handler         /* JAUNS: C funkcija no mouse.c */

.global _start
.extern kernel_main

_start:
    mov $stack_32_top, %esp

    # Nodzēšam ekrānu
    mov $0xB8000, %edi
    mov $80 * 25, %ecx
    mov $0x0F20, %ax
    rep stosw

    # Page Tables
    movl $pdp_table, %eax
    orl $0x3, %eax
    movl %eax, (pml4_table)

    movl $pd_table, %eax
    orl $0x3, %eax
    movl %eax, (pdp_table)

    movl $0x0, %eax
    orl $0x83, %eax
    movl %eax, (pd_table)

    movl $pml4_table, %eax
    movl %eax, %cr3

    # PAE
    movl %cr4, %eax
    orl $1 << 5, %eax
    movl %eax, %cr4

    # Long Mode
    movl $0xC0000080, %ecx
    rdmsr
    orl $1 << 8, %eax
    wrmsr

    # Paging
    movl %cr0, %eax
    orl $1 << 31, %eax
    movl %eax, %cr0

    lgdt gdt_64_pointer
    ljmp $0x08, $start_64

.code64
start_64:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Uzstādām kārtīgu 64-bitu steku pirms saucam C kodu!
    mov $stack_64_top, %rsp

    call kernel_main

halt_loop:
    cli
    hlt
    jmp halt_loop


.section .rodata
.align 8
gdt_64:
    .quad 0x0000000000000000
    .quad 0x00209A0000000000 # Koda segments (0x08)
    .quad 0x0000920000000000 # Datu segments (0x10)
gdt_64_pointer:
    .short . - gdt_64 - 1
    .quad gdt_64


.section .bss
.align 4096
pml4_table:
    .skip 4096
pdp_table:
    .skip 4096
pd_table:
    .skip 4096

# Divi steki - vienu startam, otru lielajai 64-bitu videi
.align 16
stack_32_bottom:
    .skip 4096
stack_32_top:

.align 16
stack_64_bottom:
    .skip 16384              # Lielais 64-bitu steks (16KB)
stack_64_top:


# ==========================================
# Asamblejas tiltiņš tastatūrai (64-bit)
# ==========================================
.section .text
.code64
.align 16
keyboard_handler_asm:
    # 1. Saglabājam visus svarīgākos reģistrus
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11

    # 2. Izsaucam C funkciju
    call keyboard_handler

    # 3. Atjaunojam visu precīzi apgrieztā secībā
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax

    # 4. Atgriežamies no interapta
    iretq


# ==========================================
# JAUNS: Asamblejas tiltiņš pelei (64-bit)
# ==========================================
.align 16
mouse_handler_asm:
    # 1. Saglabājam reģistrus, lai C kods tos nesabojātu
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11

    # 2. Izsaucam C funkciju no mouse.c
    call mouse_handler

    # 3. Atjaunojam reģistrus spoguļattēlā
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax

    # 4. Droša atgriešanās 64-bitu vidē
    iretq
