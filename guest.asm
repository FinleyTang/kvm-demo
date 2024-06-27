[bits 16]
global _start

_start:
    xor ax, ax
    xor bx, bx
    mov dx, 0x3f8
    add al, bl
    add al, 1
    out dx, al
    mov al, 10
    out dx, al
    hlt
