.globl _start
    .code16
_start:

	mov $0x3f8, %dx
    add %bl, %al
	out %al, (%dx)
	hlt