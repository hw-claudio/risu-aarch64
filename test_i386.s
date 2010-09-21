; A trivial test image for x86

BITS 32
; Initialise the registers to avoid spurious mismatches
mov eax, 0x12345678
mov ebx, 0x9abcdef0
mov ecx, 0x97361234
mov edx, 0x84310284
mov edi, 0x83624173
mov esi, 0xfaebfaeb
mov ebp, 0x84610123
; UD1 : do compare
UD1

; UD2 : exit test
UD2
