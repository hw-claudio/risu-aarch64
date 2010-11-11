;###############################################################################
;# Copyright (c) 2010 Linaro Limited
;# All rights reserved. This program and the accompanying materials
;# are made available under the terms of the Eclipse Public License v1.0
;# which accompanies this distribution, and is available at
;# http://www.eclipse.org/legal/epl-v10.html
;#
;# Contributors:
;#     Peter Maydell (Linaro) - initial implementation
;###############################################################################

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
