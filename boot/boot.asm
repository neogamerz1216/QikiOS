; boot.asm - Minimal Multiboot2 header + 64-bit long mode entry
BITS 32

section .multiboot2
align 8
mb2_header_start:
    dd 0xE85250D6           ; Multiboot2 magic
    dd 0                    ; Architecture (i386)
    dd mb2_header_end - mb2_header_start
    dd 0x100000000 - (0xE85250D6 + 0 + (mb2_header_end - mb2_header_start))
    
    ; Framebuffer tag (starts at offset 16, 8-byte aligned)
    dd 5                     ; type = framebuffer
    dd 20                    ; size (actual data bytes)
    dd 1024                  ; Preferred width
    dd 768                   ; Preferred height
    dd 32                    ; Preferred bpp
    
    ; Pad to 8-byte alignment (GRUB advances by ALIGN_UP(size, 8))
    dd 0
    
    ; End tag (starts at offset 40, 8-byte aligned)
    dd 0                     ; type = end
    dd 8                     ; size
mb2_header_end:

section .bss
align 16
stack_bottom:
    resb 16384              ; 16KB stack
stack_top:

align 4096
global pml4_table
pml4_table:
    resb 4096
global pdpt_table
pdpt_table:
    resb 4096
global page_directory_low
page_directory_low:
    resb 4096
global page_directory_high
page_directory_high:
    resb 4096

section .text
global start_32
start_32:
    mov esp, stack_top
    
    ; Save multiboot info
    mov [mb2_info_ptr], ebx
    mov [mb2_magic], eax
    
    ; Check long mode support
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz no_long_mode
    
    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    ; Set up page tables (identity map first 2MB + map 3-4GB range for framebuffer)
    mov edi, pml4_table
    mov dword [edi], pdpt_table + 0x003
    mov dword [edi + 4], 0
    mov edi, pdpt_table
    mov dword [edi], page_directory_low + 0x003    ; PDPT[0] → low page directory (0-1GB)
    mov dword [edi + 4], 0
    mov dword [edi + 24], page_directory_high + 0x003  ; PDPT[3] → high page directory (3-4GB)
    mov dword [edi + 28], 0
    mov edi, page_directory_low
    mov dword [edi], 0x00000083  ; 2MB page, present, R/W, huge
    mov dword [edi + 4], 0
    
    ; Load PML4
    mov eax, pml4_table
    mov cr3, eax
    
    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    
    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    ; Load GDT
    lgdt [gdt64.pointer]
    
    ; Jump to 64-bit
    jmp gdt64.code:long_mode_start

no_long_mode:
    mov al, "L"
    jmp error

error:
    mov edx, 0xB8000
    mov ah, 0x4F
    mov [edx], ax
    hlt
    jmp error

section .rodata
gdt64:
    dq 0                    ; Null descriptor
.code equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)  ; Code: exec, 64-bit, present, 64-bit
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .text
BITS 64
global long_mode_start
extern kernel_main
long_mode_start:
    ; Set up segments
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call C kernel
    mov rdi, [mb2_info_ptr]
    call kernel_main
    
    ; Halt
    cli
.halt:
    hlt
    jmp .halt

section .data
global mb2_info_ptr
global mb2_magic
mb2_info_ptr:
    dq 0
mb2_magic:
    dq 0
